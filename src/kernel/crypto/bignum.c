#include "kernel/crypto/bignum.h"
#include <string.h>

void bignum_init(bignum_t* n) {
    for (int i = 0; i < BIGNUM_LIMBS; ++i) n->limbs[i] = 0;
}

void bignum_from_bytes(bignum_t* n, const uint8_t* bytes, size_t len) {
    bignum_init(n);
    for (size_t i = 0; i < len; ++i) {
        size_t j = len - 1 - i;
        if (i / 8 >= BIGNUM_LIMBS) break;
        n->limbs[i / 8] |= ((uint64_t)bytes[j]) << ((i % 8) * 8);
    }
}

void bignum_to_bytes(const bignum_t* n, uint8_t* bytes, size_t len) {
    for (size_t i = 0; i < len; ++i) bytes[i] = 0;
    for (size_t i = 0; i < len; ++i) {
        size_t j = len - 1 - i;
        if (i / 8 >= BIGNUM_LIMBS) break;
        bytes[j] = (n->limbs[i / 8] >> ((i % 8) * 8)) & 0xFF;
    }
}

int bignum_cmp(const bignum_t* a, const bignum_t* b) {
    for (int i = BIGNUM_LIMBS - 1; i >= 0; --i) {
        if (a->limbs[i] > b->limbs[i]) return 1;
        if (a->limbs[i] < b->limbs[i]) return -1;
    }
    return 0;
}

void bignum_add(bignum_t* r, const bignum_t* a, const bignum_t* b) {
    uint64_t carry = 0;
    for (int i = 0; i < BIGNUM_LIMBS; ++i) {
        uint64_t sum = a->limbs[i] + b->limbs[i] + carry;
        carry = (sum < a->limbs[i]) || (carry && sum == a->limbs[i]) ? 1 : 0;
        r->limbs[i] = sum;
    }
}

void bignum_sub(bignum_t* r, const bignum_t* a, const bignum_t* b) {
    uint64_t borrow = 0;
    for (int i = 0; i < BIGNUM_LIMBS; ++i) {
        uint64_t diff = a->limbs[i] - b->limbs[i] - borrow;
        borrow = (a->limbs[i] < b->limbs[i]) || (borrow && a->limbs[i] == b->limbs[i]) ? 1 : 0;
        r->limbs[i] = diff;
    }
}

static void bignum_shift_left_1(bignum_t* n) {
    uint64_t carry = 0;
    for (int i = 0; i < BIGNUM_LIMBS; ++i) {
        uint64_t next_carry = n->limbs[i] >> 63;
        n->limbs[i] = (n->limbs[i] << 1) | carry;
        carry = next_carry;
    }
}

static void bignum_shift_right_1(bignum_t* n) {
    uint64_t carry = 0;
    for (int i = BIGNUM_LIMBS - 1; i >= 0; --i) {
        uint64_t next_carry = (n->limbs[i] & 1) ? (1ULL << 63) : 0;
        n->limbs[i] = (n->limbs[i] >> 1) | carry;
        carry = next_carry;
    }
}

static int bignum_bit(const bignum_t* n, int bit) {
    if (bit < 0 || bit >= BIGNUM_LIMBS * 64) return 0;
    return (n->limbs[bit / 64] >> (bit % 64)) & 1;
}

static int bignum_num_bits(const bignum_t* n) {
    for (int i = BIGNUM_LIMBS - 1; i >= 0; --i) {
        if (n->limbs[i]) {
            for (int b = 63; b >= 0; --b) {
                if ((n->limbs[i] >> b) & 1) return i * 64 + b + 1;
            }
        }
    }
    return 0;
}

/* r = a % m */
void bignum_mod(bignum_t* r, const bignum_t* a, const bignum_t* m) {
    bignum_t q, temp_m;
    bignum_init(r);
    for (int i = 0; i < BIGNUM_LIMBS; ++i) r->limbs[i] = a->limbs[i];
    
    if (bignum_cmp(r, m) < 0) return;

    int r_bits = bignum_num_bits(r);
    int m_bits = bignum_num_bits(m);
    int shifts = r_bits - m_bits;

    for (int i = 0; i < BIGNUM_LIMBS; ++i) temp_m.limbs[i] = m->limbs[i];
    for (int i = 0; i < shifts; ++i) bignum_shift_left_1(&temp_m);

    for (int i = 0; i <= shifts; ++i) {
        if (bignum_cmp(r, &temp_m) >= 0) {
            bignum_t tmp;
            bignum_sub(&tmp, r, &temp_m);
            for (int j = 0; j < BIGNUM_LIMBS; ++j) r->limbs[j] = tmp.limbs[j];
        }
        bignum_shift_right_1(&temp_m);
    }
}

/* r = (a * b) % m. Uses double-size accumulator limbs. */
void bignum_mul_mod(bignum_t* r, const bignum_t* a, const bignum_t* b, const bignum_t* m) {
    uint64_t prod[BIGNUM_LIMBS * 2] = {0};

    for (int i = 0; i < BIGNUM_LIMBS; ++i) {
        if (!a->limbs[i]) continue;
        uint64_t carry = 0;
        for (int j = 0; j < BIGNUM_LIMBS; ++j) {
            __uint128_t p = (__uint128_t)a->limbs[i] * b->limbs[j] + prod[i + j] + carry;
            prod[i + j] = (uint64_t)p;
            carry = (uint64_t)(p >> 64);
        }
        prod[i + BIGNUM_LIMBS] = carry;
    }

    // Find actual bit length of product (typically ~512 bits for 256-bit inputs)
    int prod_bits = 0;
    for (int i = BIGNUM_LIMBS * 2 - 1; i >= 0; --i) {
        if (prod[i]) {
            for (int b = 63; b >= 0; --b) {
                if ((prod[i] >> b) & 1) {
                    prod_bits = i * 64 + b + 1;
                    goto found_bits;
                }
            }
        }
    }
found_bits:

    bignum_t rem; bignum_init(&rem);
    
    // Process top down
    for (int bit = prod_bits - 1; bit >= 0; --bit) {
        bignum_shift_left_1(&rem);
        if ((prod[bit / 64] >> (bit % 64)) & 1) {
            rem.limbs[0] |= 1;
        }
        if (bignum_cmp(&rem, m) >= 0) {
            bignum_t tmp;
            bignum_sub(&tmp, &rem, m);
            for (int j = 0; j < BIGNUM_LIMBS; ++j) rem.limbs[j] = tmp.limbs[j];
        }
    }
    
    for (int i = 0; i < BIGNUM_LIMBS; ++i) r->limbs[i] = rem.limbs[i];
}

/* r = a^(-1) mod m using binary extended GCD */
/* Coefficients kept in [0, m-1] using only add/sub (no bignum_mod) */
void bignum_inv_mod(bignum_t* r, const bignum_t* a, const bignum_t* m) {
    bignum_t u, v, x1, x2, tmp, one, zero;
    bignum_init(&one); one.limbs[0] = 1;
    bignum_init(&zero);
    
    bignum_mod(&u, a, m);
    for (int i = 0; i < BIGNUM_LIMBS; i++) v.limbs[i] = m->limbs[i];
    
    if (bignum_cmp(&u, &zero) == 0) { bignum_init(r); return; }
    
    bignum_init(&x1); x1.limbs[0] = 1;
    bignum_init(&x2);
    
    while (bignum_cmp(&u, &one) != 0 && bignum_cmp(&v, &one) != 0) {
        /* If u or v reached 0, the other is gcd; gcd=1 required for inverse */
        if (bignum_cmp(&u, &zero) == 0 || bignum_cmp(&v, &zero) == 0) break;

        while ((u.limbs[0] & 1) == 0) {
            bignum_shift_right_1(&u);
            if ((x1.limbs[0] & 1) == 0) {
                bignum_shift_right_1(&x1);
            } else {
                bignum_add(&x1, &x1, m);
                bignum_shift_right_1(&x1);
            }
        }
        
        while ((v.limbs[0] & 1) == 0) {
            bignum_shift_right_1(&v);
            if ((x2.limbs[0] & 1) == 0) {
                bignum_shift_right_1(&x2);
            } else {
                bignum_add(&x2, &x2, m);
                bignum_shift_right_1(&x2);
            }
        }
        
        if (bignum_cmp(&u, &v) >= 0) {
            bignum_sub(&u, &u, &v);
            if (bignum_cmp(&x1, &x2) >= 0) {
                bignum_sub(&x1, &x1, &x2);
            } else {
                bignum_sub(&tmp, &x1, &x2);
                bignum_add(&x1, &tmp, m);
            }
        } else {
            bignum_sub(&v, &v, &u);
            if (bignum_cmp(&x2, &x1) >= 0) {
                bignum_sub(&x2, &x2, &x1);
            } else {
                bignum_sub(&tmp, &x2, &x1);
                bignum_add(&x2, &tmp, m);
            }
        }
    }
    
    if (bignum_cmp(&v, &one) == 0) {
        for (int i = 0; i < BIGNUM_LIMBS; i++) r->limbs[i] = x2.limbs[i];
    } else {
        for (int i = 0; i < BIGNUM_LIMBS; i++) r->limbs[i] = x1.limbs[i];
    }
}

void bignum_modexp(bignum_t* result, const bignum_t* base, const bignum_t* exp, const bignum_t* mod) {
    bignum_t res, base_mod;
    bignum_init(&res);
    res.limbs[0] = 1;
    
    bignum_mod(&base_mod, base, mod);

    int max_bit = bignum_num_bits(exp);
    for (int i = 0; i < max_bit; ++i) {
        if (bignum_bit(exp, i)) {
            bignum_t tmp;
            bignum_mul_mod(&tmp, &res, &base_mod, mod);
            for (int j = 0; j < BIGNUM_LIMBS; ++j) res.limbs[j] = tmp.limbs[j];
        }
        bignum_t tmp2;
        bignum_mul_mod(&tmp2, &base_mod, &base_mod, mod);
        for (int j = 0; j < BIGNUM_LIMBS; ++j) base_mod.limbs[j] = tmp2.limbs[j];
    }
    
    for (int i = 0; i < BIGNUM_LIMBS; ++i) result->limbs[i] = res.limbs[i];
}
