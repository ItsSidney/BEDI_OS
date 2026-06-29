CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC -I../include
LDFLAGS = -lm

OBJS = math_engine.o

all: libmath_engine.a test_engine

libmath_engine.a: $(OBJS)
	ar rcs $@ $^

math_engine.o: ../src/engine/math_engine.c ../include/engine/math_engine.h
	$(CC) $(CFLAGS) -c $< -o $@

test_engine: test_engine.c $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o *.a test_engine
