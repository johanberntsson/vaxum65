/* loadtest.c: check the game loader and attic RAM main memory on their
   own, before there is a Glk layer to run the interpreter with.

   Loads the game file, copies it into main memory the way vm_restart()
   does, verifies the Glulx checksum through the mem_read4() path, and
   checks that writes to main memory read back. Built and run by
   `make runloadtest`.
*/

#include <stdio.h>

#include "glk.h"
#include "glulxe.h"

/* Normally in main.c, which this test does not link. */
glui32 gamefile_start = 0;
glui32 gamefile_len = 0;

static int failures = 0;

static void check(const char *what, int ok)
{
  printf("%-28s %s\n", what, ok ? "ok" : "FAIL");
  if (!ok)
    failures++;
}

int main(void)
{
  unsigned char buf[36];
  const char *err;
  glui32 ramstart, extstart, endmem, stacksize, checksum;
  glui32 adr, sum;

  putchar(14);  /* PETSCII: switch to the lower-case character set */
  printf("vaxum65 load test\n\n");

  err = game_load(GAME_FILE);
  if (err) {
    printf("load failed: %s\n", err);
    return 1;
  }
  printf("loaded %s, %lu bytes\n", GAME_FILE, (unsigned long)gamefile_len);

  game_image_read(0, buf, 36);
  ramstart = Read4(buf+8);
  extstart = Read4(buf+12);
  endmem = Read4(buf+16);
  stacksize = Read4(buf+20);
  checksum = Read4(buf+32);
  printf("ramstart  %08lx\n", (unsigned long)ramstart);
  printf("extstart  %08lx\n", (unsigned long)extstart);
  printf("endmem    %08lx\n", (unsigned long)endmem);
  printf("stacksize %08lx\n", (unsigned long)stacksize);
  printf("checksum  %08lx\n\n", (unsigned long)checksum);

  check("length matches header", extstart == gamefile_len);

  /* As vm_restart() does it, with no protected range. */
  dma_copy(GAME_IMAGE, GAME_MEM, extstart);
  dma_fill(GAME_MEM + extstart, 0, endmem - extstart);

  check("magic via mem_read1",
    mem_read1(0) == 'G' && mem_read1(1) == 'l'
    && mem_read1(2) == 'u' && mem_read1(3) == 'l');
  check("ramstart via mem_read4", mem_read4(8) == ramstart);
  check("version via mem_read2", mem_read2(4) == 0x0002 || mem_read2(4) == 0x0003);

  /* The Glulx checksum: the sum of every big-endian word in the file,
     with the checksum word itself counted as zero. */
  sum = 0;
  for (adr = 0; adr < extstart; adr += 4) {
    if (adr != 0x20)
      sum += mem_read4(adr);
  }
  printf("computed  %08lx\n", (unsigned long)sum);
  check("checksum", sum == checksum);

  if (endmem > extstart)
    check("RAM beyond file is zero",
      mem_read4(extstart) == 0 && mem_read4(endmem - 4) == 0);

  mem_write4(ramstart, 0x12345678UL);
  check("write4/read4", mem_read4(ramstart) == 0x12345678UL);
  check("big-endian byte order",
    mem_read1(ramstart) == 0x12 && mem_read1(ramstart+3) == 0x78);
  mem_write2(ramstart+4, 0xBEEF);
  check("write2/read2", mem_read2(ramstart+4) == 0xBEEF);
  mem_write1(ramstart+6, 0xA5);
  check("write1/read1", mem_read1(ramstart+6) == 0xA5);
  /* A word that straddles a 64K boundary in attic RAM. */
  mem_write4(0x1FFFE, 0xCAFEF00DUL);
  check("write across 64K boundary", mem_read4(0x1FFFE) == 0xCAFEF00DUL);

  /* Restart restores the image. */
  dma_copy(GAME_IMAGE, GAME_MEM, extstart);
  sum = 0;
  for (adr = 0; adr < extstart; adr += 4) {
    if (adr != 0x20)
      sum += mem_read4(adr);
  }
  check("checksum after restore", sum == checksum);

  printf("\n%s\n", failures ? "FAILED" : "ALL PASSED");
  return 0;
}
