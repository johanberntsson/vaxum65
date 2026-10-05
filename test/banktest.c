/* banktest.c: check banked code before the interpreter depends on it.

   Loads the banks (bankStore.raw, as the file BANKS) to $40000, then calls
   functions in two banks from near code and from each other, and checks:
   argument and return passing of every width, stack arguments, constant
   data kept in the bank, calls between banks in both directions, near
   callbacks, a banked function that saves registers (and so returns with
   a plain RTS), Kernal output from banked code, Kernal file I/O from banked code
   through a near wrapper that calls bank_remap(), a long banked loop with
   interrupts arriving, and that the slot is unmapped again afterwards. Built and run by `make runbanktest`.
*/

#include <calypsi/stubs.h>
#include <fcntl.h>
#include <stdio.h>

#include "glk.h"
#include "glulxe.h"
#include "banktest.h"

#define BANKS_ADDR (0x40000UL)
#define BANKS_MAX  (0x20000UL)
#define SLOT       ((volatile unsigned char *)0x6000)
#define HIGHRAM    ((volatile unsigned char *)0xA000)

/* Normally in main.c, which this test does not link. */
glui32 gamefile_start = 0;
glui32 gamefile_len = 0;

extern void bank_basic_restore(void);
extern void bank_remap(void);


static int failures = 0;

/* printf alone would not fit below the slot at $4000. */
static void put_str(const char *s)
{
  while (*s)
    putchar(*s++);
}

static void check(const char *what, int ok)
{
  int n = 0;
  put_str(what);
  while (what[n])
    n++;
  for (; n < 31; n++)
    putchar(' ');
  puts(ok ? "ok" : "FAIL");
  if (!ok)
    failures++;
}

static int near_inc(int x)
{
  return x + 1;
}

/* A sum over everything loaded to the banks, to show nothing wrote there. */
static uint32_t banks_sum(void)
{
  const unsigned char __far *p = (const unsigned char __far *)BANKS_ADDR;
  uint32_t sum = 0, i;
  for (i = 0; i < 0x4000; i++)
    sum += p[i] * (uint32_t)(i + 1);
  return sum;
}

long near_read_length(const char *name)
{
  unsigned char buf[4];
  int fd = _Stub_open(name, O_RDONLY | O_BINARY);
  long n;

  if (fd < 0) {
    bank_remap();
    return -1;
  }
  n = _Stub_read(fd, buf, 4);
  _Stub_close(fd);
  bank_remap();
  if (n != 4)
    return -2;
  return (long)buf[0] | ((long)buf[1] << 8) | ((long)buf[2] << 16)
    | ((long)buf[3] << 24);
}

int main(void)
{
  const char *err;
  long len;
  uint32_t busy, loaded_sum;

  putchar(14);  /* PETSCII: switch to the lower-case character set */
  puts("vaxum65 bank test\n");

  /* The slot is plain RAM until a bank is mapped, and with BASIC banked
     out by __low_level_init, so is $A000. */
  SLOT[0] = 0xC3;
  check("slot is ram before calls", SLOT[0] == 0xC3);
  HIGHRAM[0] = 0x3C;
  check("basic rom is out at $a000", HIGHRAM[0] == 0x3C);

  err = load_far("BANKS", BANKS_ADDR, BANKS_MAX);
  if (err) {
    put_str("loading banks failed: ");
    puts(err);
    bank_basic_restore();
    return 1;
  }

  loaded_sum = banks_sum();
  check("a: int args and return", a_add(1234, -234) == 1000);
  check("a: char in A, char out", a_char(0x33) == 0x69);
  check("a: 32-bit args and return", a_mul32(100000UL, 3000UL) == 300000000UL);
  check("a: six long args", a_sum6(100000L, 100000L, 100000L,
    100000L, 100000L, 100000L) == 2100000L);
  check("a: const data in the bank", a_square(5) == 25 && a_square(7) == 49);
  check("b: int args and return", b_twice(21) == 42);
  check("b: const data in the bank", b_tag(1) == 'a');
  check("b calls a and continues in b", b_from_a(5) == 315);
  check("a/b chain, depth 5", a_chain(5) == 3002);
  check("b/a chain, depth 6", b_chain(6) == 3003);
  check("a calls a near function", a_callback(near_inc, 41) == 43);
  puts("busy loop in bank a...");
  busy = a_busy(0x100000UL);
  check("a: long loop under interrupts", busy == 4096UL * 32640UL);

  check("a: kernal output", a_print(7) == 8);

  len = near_read_length("BANKS");
  check("b: file i/o via near wrapper", len > 0 && b_read_length("BANKS") == len);


  check("slot is ram again after calls", SLOT[0] == 0xC3);
  check("near kernal i/o after calls", near_read_length("BANKS") == len);
  check("banks unchanged at the end", banks_sum() == loaded_sum);

  puts(failures ? "\nFAILED" : "\nALL PASSED");
  bank_basic_restore();
  return 0;
}
