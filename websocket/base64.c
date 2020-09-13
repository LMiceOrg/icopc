#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base64.h"

static const char base64[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

int base64_encrypt_len(int n) { return ((n + 2) / 3) * 4; }
void base64_encrypt_text2(const unsigned char *str, int nbytes, char *pbuf,
                          int elen) {
  int pos = 0;
  const unsigned char *cur;
  cur = str;
  if (elen < base64_encrypt_len(nbytes)) return;

  while (nbytes > 2) {
    pbuf[pos++] = base64[cur[0] >> 2];
    pbuf[pos++] = base64[((cur[0] & 0x03) << 4) + (cur[1] >> 4)];
    pbuf[pos++] = base64[((cur[1] & 0x0f) << 2) + (cur[2] >> 6)];
    pbuf[pos++] = base64[cur[2] & 0x3f];

    cur += 3;
    nbytes -= 3;
  }
  if (nbytes > 0) {
    pbuf[pos++] = base64[cur[0] >> 2];
    if (nbytes % 3 == 1) {
      pbuf[pos++] = base64[(cur[0] & 0x03) << 4];
      pbuf[pos++] = '=';
      pbuf[pos++] = '=';
    } else if (nbytes % 3 == 2) {
      pbuf[pos++] = base64[((cur[0] & 0x03) << 4) + (cur[1] >> 4)];
      pbuf[pos++] = base64[(cur[1] & 0x0f) << 2];
      pbuf[pos++] = '=';
    }
  }
}
void base64_encrypt_text(const unsigned char *str, int bytes, char **pebuf,
                         int *elen) {
  int pos = 0;
  const unsigned char *current;
  char *_encode_result = (char *)malloc((bytes * 4) / 3 + 16);
  memset(_encode_result, 0, (bytes * 4) / 3 + 16);
  
  current = str;
  while (bytes > 2) {
    _encode_result[pos++] = base64[current[0] >> 2];
    _encode_result[pos++] =
        base64[((current[0] & 0x03) << 4) + (current[1] >> 4)];
    _encode_result[pos++] =
        base64[((current[1] & 0x0f) << 2) + (current[2] >> 6)];
    _encode_result[pos++] = base64[current[2] & 0x3f];

    current += 3;
    bytes -= 3;
  }
  if (bytes > 0) {
    _encode_result[pos++] = base64[current[0] >> 2];
    if (bytes % 3 == 1) {
      _encode_result[pos++] = base64[(current[0] & 0x03) << 4];
      _encode_result[pos++] = '=';
      _encode_result[pos++] = '=';
    } else if (bytes % 3 == 2) {
      _encode_result[pos++] =
          base64[((current[0] & 0x03) << 4) + (current[1] >> 4)];
      _encode_result[pos++] = base64[(current[1] & 0x0f) << 2];
      _encode_result[pos++] = '=';
    }
  }
  *pebuf = _encode_result;
  *elen = pos;
}

void base64_free(char *pbuf) { free(pbuf); }
