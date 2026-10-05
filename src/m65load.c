/* m65load.c: load a Glulx game file from disk into attic RAM.

   The file is read through the Kernal, a chunk at a time, into a near
   staging buffer and DMAd up to GAME_IMAGE.

   Reading a SEQ file through the Kernal reports EOF 256 bytes before the
   real end, so the loader never relies on reaching EOF. It reads exactly
   the length given in the game header (EXTSTART), and the Makefile pads
   the file on the disk past the bytes that cannot be reached.
*/

#include <calypsi/stubs.h>
#include <fcntl.h>

#include "glk.h"
#include "glulxe.h"

#define CHUNK (512)

static unsigned char staging[CHUNK];

/* Read length bytes, asking again if a Kernal read comes back short.
   Returns how many arrived; a read of nothing at all ends it. */
static uint16_t read_exact(int fd, unsigned char *dest, uint16_t length)
{
  uint16_t left = length;

  while (left) {
    size_t got = _Stub_read(fd, dest, left);
    if (!got || got > left)
      break;
    dest += got;
    left -= (uint16_t)got;
  }
  return length - left;
}

const char *game_load(const char *name)
{
  int fd;
  glui32 pos, length;
  uint16_t want;

  fd = _Stub_open(name, O_RDONLY | O_BINARY);
  if (fd < 0)
    return "Cannot open the game file.";

  /* The first chunk holds the header, which says how long the file is. */
  if (read_exact(fd, staging, CHUNK) != CHUNK) {
    _Stub_close(fd);
    return "The game file is too short.";
  }
  if (staging[0] != 'G' || staging[1] != 'l'
    || staging[2] != 'u' || staging[3] != 'l') {
    _Stub_close(fd);
    return "This is not a Glulx game file. (Blorb files are not supported.)";
  }
  length = Read4(staging+12);
  if (length < CHUNK || length > GAME_IMAGE_MAX) {
    _Stub_close(fd);
    return "The game file length in the header is out of range.";
  }

  dma_copy((uint32_t)(uint16_t)staging, GAME_IMAGE, CHUNK);
  for (pos = CHUNK; pos < length; pos += want) {
    want = (length - pos) > CHUNK ? CHUNK : (uint16_t)(length - pos);
    if (read_exact(fd, staging, want) != want) {
      _Stub_close(fd);
      return "The game file ended unexpectedly.";
    }
    dma_copy((uint32_t)(uint16_t)staging, GAME_IMAGE + pos, want);
  }
  _Stub_close(fd);

  gamefile_start = 0;
  gamefile_len = length;
  return NULL;
}

void game_image_read(uint32_t pos, unsigned char *buf, uint16_t len)
{
  dma_copy(GAME_IMAGE + pos, (uint32_t)(uint16_t)buf, len);
}
