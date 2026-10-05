/* banktest_b.c: banked functions in bank B (see banktest.c). */

#include <stdint.h>

#include "banktest.h"

#pragma clang section text="bank_b" rodata="bank_b_data"

static const char tag[] = "bank b";

__banked int b_twice(int x)
{
  return 2 * x;
}

__banked int b_chain(int n)
{
  if (n <= 0)
    return 0;
  return a_chain(n - 1) + 1;
}

__banked int b_from_a(int x)
{
  return a_add(x, 100) * 3;
}

__banked char b_tag(int i)
{
  return tag[i];
}

/* Kernal file I/O from banked code, through a near wrapper. */
__banked long b_read_length(const char *name)
{
  return near_read_length(name);
}
