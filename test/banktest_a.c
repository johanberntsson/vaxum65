/* banktest_a.c: banked functions in bank A (see banktest.c). */

#include <stdint.h>
#include <stdio.h>

#include "banktest.h"

#pragma clang section text="bank_a" rodata="bank_a_data"

/* In the bank with the code, visible only while bank A is mapped. */
static const uint16_t squares[8] = { 0, 1, 4, 9, 16, 25, 36, 49 };

__banked int a_add(int x, int y)
{
  return x + y;
}

__banked unsigned char a_char(unsigned char c)
{
  return (unsigned char)(c ^ 0x5A);
}

__banked uint32_t a_mul32(uint32_t x, uint32_t y)
{
  return x * y;
}

__banked long a_sum6(long a, long b, long c, long d, long e, long f)
{
  return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f;
}

__banked uint16_t a_square(uint8_t i)
{
  return squares[i & 7];
}

__banked int a_chain(int n)
{
  /* Calls into bank B, which calls back here. */
  if (n <= 0)
    return 0;
  return b_chain(n - 1) + 1000;
}

__banked int a_callback(int (*fn)(int), int x)
{
  return fn(x) + 1;
}

__banked int a_print(int x)
{
  puts("  kernal output from bank a");
  return x + 1;
}

__banked uint32_t a_busy(uint32_t count)
{
  /* Long enough for many interrupts to arrive while bank A is mapped. */
  volatile uint32_t sum = 0;
  uint32_t i;
  for (i = 0; i < count; i++)
    sum += i & 0xFF;
  return sum;
}
