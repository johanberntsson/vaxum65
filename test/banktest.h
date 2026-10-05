/* banktest.h: the banked functions of the bank test. */

#ifndef _BANKTEST_H
#define _BANKTEST_H

#include <stdint.h>

/* Near wrapper for banked code (banktest.c): Kernal file I/O, then
   bank_remap() to put the caller's bank back. */
long near_read_length(const char *name);

/* Bank A (banktest_a.c) */
__banked int a_add(int x, int y);
__banked unsigned char a_char(unsigned char c);
__banked uint32_t a_mul32(uint32_t x, uint32_t y);
__banked long a_sum6(long a, long b, long c, long d, long e, long f);
__banked uint16_t a_square(uint8_t i);
__banked int a_chain(int n);
__banked int a_callback(int (*fn)(int), int x);
__banked int a_print(int x);
__banked uint32_t a_busy(uint32_t count);

/* Bank B (banktest_b.c) */
__banked int b_twice(int x);
__banked int b_chain(int n);
__banked int b_from_a(int x);
__banked char b_tag(int i);
__banked long b_read_length(const char *name);

#endif /* _BANKTEST_H */
