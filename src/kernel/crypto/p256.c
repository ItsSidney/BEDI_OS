#include "kernel/crypto/p256.h"
#include <string.h>

static bignum_t p256_p;
static bignum_t p256_b;
static p256_point_t p256_g;

static const uint8_t P256_P_BYTES[32] = {
    0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static const uint8_t P256_B_BYTES[32] = {
    0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a, 0x93, 0xe7,
    0xb3, 0xeb, 0xbd, 0x55, 0x76, 0x98, 0x86, 0xbc,
    0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53, 0xb0, 0xf6,
    0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b
};

static const uint8_t P256_GX_BYTES[32] = {
    0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47,
    0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
    0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0,
    0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96
};

static const uint8_t P256_GY_BYTES[32] = {
    0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b,
    0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
    0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce,
    0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
};

void p256_init(void) {
    bignum_from_bytes(&p256_p, P256_P_BYTES, 32);
    bignum_from_bytes(&p256_b, P256_B_BYTES, 32);
    bignum_from_bytes(&p256_g.x, P256_GX_BYTES, 32);
    bignum_from_bytes(&p256_g.y, P256_GY_BYTES, 32);
}

static void add_mod(bignum_t* r, const bignum_t* a, const bignum_t* b) {
    bignum_t sum;
    bignum_add(&sum, a, b);
    if (bignum_cmp(&sum, &p256_p) >= 0) {
        bignum_sub(r, &sum, &p256_p);
    } else {
        *r = sum;
    }
}

static void sub_mod(bignum_t* r, const bignum_t* a, const bignum_t* b) {
    if (bignum_cmp(a, b) < 0) {
        bignum_t temp;
        bignum_add(&temp, a, &p256_p);
        bignum_sub(r, &temp, b);
    } else {
        bignum_sub(r, a, b);
    }
}

static void inv_mod(bignum_t* r, const bignum_t* a) {
    bignum_inv_mod(r, a, &p256_p);
}

static int is_infinity(const p256_point_t* p) {
    for (int i = 0; i < BIGNUM_LIMBS; ++i) {
        if (p->x.limbs[i] != 0 || p->y.limbs[i] != 0) return 0;
    }
    return 1;
}

static void set_infinity(p256_point_t* p) {
    bignum_init(&p->x);
    bignum_init(&p->y);
}

void p256_point_add(p256_point_t* r, const p256_point_t* a, const p256_point_t* b) {
    if (is_infinity(a)) { *r = *b; return; }
    if (is_infinity(b)) { *r = *a; return; }

    if (bignum_cmp(&a->x, &b->x) == 0) {
        if (bignum_cmp(&a->y, &b->y) == 0) {
            p256_point_double(r, a);
        } else {
            set_infinity(r);
        }
        return;
    }

    bignum_t dy, dx, slope, tmp1, tmp2;
    sub_mod(&dy, &b->y, &a->y);
    sub_mod(&dx, &b->x, &a->x);
    inv_mod(&tmp1, &dx);
    bignum_mul_mod(&slope, &dy, &tmp1, &p256_p);

    bignum_mul_mod(&tmp1, &slope, &slope, &p256_p);
    sub_mod(&tmp2, &tmp1, &a->x);
    sub_mod(&r->x, &tmp2, &b->x);

    sub_mod(&tmp1, &a->x, &r->x);
    bignum_mul_mod(&tmp2, &slope, &tmp1, &p256_p);
    sub_mod(&r->y, &tmp2, &a->y);
}

void p256_point_double(p256_point_t* r, const p256_point_t* a) {
    if (is_infinity(a)) { *r = *a; return; }

    bignum_t num, den, slope, tmp1, tmp2;
    
    // num = 3*x^2 - 3 = 3*(x^2 - 1)
    bignum_t one, three;
    bignum_init(&one); one.limbs[0] = 1;
    bignum_init(&three); three.limbs[0] = 3;
    
    bignum_mul_mod(&tmp1, &a->x, &a->x, &p256_p);
    sub_mod(&tmp2, &tmp1, &one);
    bignum_mul_mod(&num, &tmp2, &three, &p256_p);

    // den = 2*y
    add_mod(&den, &a->y, &a->y);
    inv_mod(&tmp1, &den);
    bignum_mul_mod(&slope, &num, &tmp1, &p256_p);

    bignum_mul_mod(&tmp1, &slope, &slope, &p256_p);
    add_mod(&tmp2, &a->x, &a->x);
    sub_mod(&r->x, &tmp1, &tmp2);

    sub_mod(&tmp1, &a->x, &r->x);
    bignum_mul_mod(&tmp2, &slope, &tmp1, &p256_p);
    sub_mod(&r->y, &tmp2, &a->y);
}

void p256_scalar_mult(p256_point_t* r, const bignum_t* k, const p256_point_t* p) {
    p256_point_t res;
    set_infinity(&res);
    p256_point_t temp = *p;

    for (int i = 0; i < BIGNUM_LIMBS; ++i) {
        if (!k->limbs[i]) {
            for(int j=0; j<64; j++) p256_point_double(&temp, &temp);
            continue;
        }
        for (int j = 0; j < 64; ++j) {
            if ((k->limbs[i] >> j) & 1) {
                p256_point_add(&res, &res, &temp);
            }
            p256_point_double(&temp, &temp);
        }
    }
    *r = res;
}

void p256_base_mult(p256_point_t* r, const bignum_t* k) {
    p256_scalar_mult(r, k, &p256_g);
}

int p256_from_bytes(p256_point_t* p, const uint8_t* bytes, size_t len) {
    if (len != 65 || bytes[0] != 0x04) return -1;
    bignum_from_bytes(&p->x, bytes + 1, 32);
    bignum_from_bytes(&p->y, bytes + 33, 32);
    return 0;
}

void p256_to_bytes(const p256_point_t* p, uint8_t* bytes) {
    bytes[0] = 0x04;
    bignum_to_bytes(&p->x, bytes + 1, 32);
    bignum_to_bytes(&p->y, bytes + 33, 32);
}
