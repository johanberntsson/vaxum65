/* mega65.h: MEGA65 platform layer for Glulxe.

   Glulx main memory does not fit in the 64K CPU address space, so it lives
   in attic RAM and is reached through 32-bit __far pointers. The game file
   is loaded once, at startup, into a second region of attic RAM and stays
   there untouched: restart copies it back with DMA instead of rereading
   the disk.
*/

#ifndef _MEGA65_H
#define _MEGA65_H

#include <stdint.h>

/* Glulx main memory: address 0 of the VM is GAME_MEM. It may grow (through
   @setmemsize and @malloc) up to GAME_MEM_MAX bytes. */
#define GAME_MEM      (0x8000000UL)
#define GAME_MEM_MAX  (0x400000UL)

/* The game file as loaded from disk, never written after loading. */
#define GAME_IMAGE    (0x8400000UL)
#define GAME_IMAGE_MAX (0x400000UL)

#define GAME_PTR(adr) ((unsigned char __far *)(GAME_MEM + (uint32_t)(adr)))

/* Big-endian access to Glulx main memory. These do the work of the Mem*
   and MemW* macros in glulxe.h, which add the address verification. */

static inline unsigned char mem_read1(uint32_t adr)
{
  return *GAME_PTR(adr);
}

static inline uint16_t mem_read2(uint32_t adr)
{
  unsigned char __far *p = GAME_PTR(adr);
  return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t mem_read4(uint32_t adr)
{
  unsigned char __far *p = GAME_PTR(adr);
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
    | ((uint32_t)p[2] << 8) | p[3];
}

static inline void mem_write1(uint32_t adr, uint32_t val)
{
  *GAME_PTR(adr) = (unsigned char)val;
}

static inline void mem_write2(uint32_t adr, uint32_t val)
{
  unsigned char __far *p = GAME_PTR(adr);
  p[0] = (unsigned char)(val >> 8);
  p[1] = (unsigned char)val;
}

static inline void mem_write4(uint32_t adr, uint32_t val)
{
  unsigned char __far *p = GAME_PTR(adr);
  p[0] = (unsigned char)(val >> 24);
  p[1] = (unsigned char)(val >> 16);
  p[2] = (unsigned char)(val >> 8);
  p[3] = (unsigned char)val;
}

/* DMA (dma.c). Addresses are 28-bit flat. A count of 0 does nothing. */
void dma_copy(uint32_t src, uint32_t dst, uint32_t count);
void dma_fill(uint32_t dst, uint8_t value, uint32_t count);

/* The game file loader (m65load.c). game_load() reads a bare Glulx file
   from disk into GAME_IMAGE and sets gamefile_start and gamefile_len.
   It returns NULL on success, or a message saying what went wrong. */
const char *game_load(const char *name);

/* Copy len bytes of the loaded game file, starting at pos, into a near
   buffer. The caller has checked that the range is inside the file. */
void game_image_read(uint32_t pos, unsigned char *buf, uint16_t len);

#endif /* _MEGA65_H */
