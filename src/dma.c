/* dma.c: DMAgic (F011B) block copy and fill across the 28-bit address
   space, for moving the game between near memory and attic RAM.
*/

#include <stdint.h>

#include "mega65.h"

#define DMA_ADDR_MSB   (*(volatile uint8_t *)0xD701)
#define DMA_ADDR_BANK  (*(volatile uint8_t *)0xD702)
#define DMA_ADDR_MB    (*(volatile uint8_t *)0xD704)
#define DMA_ADDR_LSB_X (*(volatile uint8_t *)0xD705)  /* triggers, extended list */

/* One job's count is 16 bits, and 0 means 64K. Longer copies are split
   into pieces of this size. */
#define DMA_PIECE (0x8000U)

/* A 12-byte F011B job, preceded by option bytes for the source and
   destination megabytes. */
struct dma_list {
  uint8_t opt_format;  /* $0B: F011B job follows the options */
  uint8_t opt_src_mb;  /* $80 */
  uint8_t src_mb;      /* source address bits 20-27 */
  uint8_t opt_dst_mb;  /* $81 */
  uint8_t dst_mb;      /* destination address bits 20-27 */
  uint8_t opt_skip;    /* $85 */
  uint8_t dst_skip;    /* destination bytes advanced per write */
  uint8_t opt_end;     /* $00 */
  uint8_t command;     /* 0 = copy, 3 = fill */
  uint16_t count;
  uint16_t src;
  uint8_t src_bank;    /* source address bits 16-19 */
  uint16_t dst;
  uint8_t dst_bank;    /* destination address bits 16-19 */
  uint8_t command_msb;
  uint16_t modulo;
};

static struct dma_list list = {
  0x0B, 0x80, 0, 0x81, 0, 0x85, 1, 0x00, 0, 0, 0, 0, 0, 0, 0, 0,
};

static void run(uint8_t command, uint32_t src, uint32_t dst, uint16_t count)
{
  list.command = command;
  list.count = count;
  list.src = (uint16_t)src;
  list.src_bank = (uint8_t)(src >> 16) & 0x0F;
  list.src_mb = (uint8_t)(src >> 20);
  list.dst = (uint16_t)dst;
  list.dst_bank = (uint8_t)(dst >> 16) & 0x0F;
  list.dst_mb = (uint8_t)(dst >> 20);

  DMA_ADDR_BANK = 0;  /* the list itself is in bank 0 of megabyte 0 */
  DMA_ADDR_MB = 0;
  DMA_ADDR_MSB = (uint8_t)((uint16_t)&list >> 8);
  DMA_ADDR_LSB_X = (uint8_t)(uint16_t)&list;
}

void dma_copy(uint32_t src, uint32_t dst, uint32_t count)
{
  while (count) {
    uint16_t piece = count > DMA_PIECE ? DMA_PIECE : (uint16_t)count;
    run(0, src, dst, piece);
    src += piece;
    dst += piece;
    count -= piece;
  }
}

void dma_fill(uint32_t dst, uint8_t value, uint32_t count)
{
  while (count) {
    uint16_t piece = count > DMA_PIECE ? DMA_PIECE : (uint16_t)count;
    /* In a fill, the low byte of the source address is the value. */
    run(3, value, dst, piece);
    dst += piece;
    count -= piece;
  }
}
