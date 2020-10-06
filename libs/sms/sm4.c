#include "sm4.h"

#if _MSC_VER == 1200
#ifndef uint8_t
#define uint8_t unsigned char
#define uint16_t unsigned short
#define uint32_t unsigned int
#define uint64_t unsigned __int64

#define int8_t char
#define int16_t short
#define int32_t int
#define int64_t __int64
#endif
#else
#include <stdint.h>
#endif

#include <string.h>

#include "sm4_common.c"

#include "modes_lcl.h"
#include "rotate.h"

#define SMS4_KEY_LENGTH 16
#define SMS4_BLOCK_SIZE 16
#define SMS4_IV_LENGTH (SMS4_BLOCK_SIZE)
#define SMS4_NUM_ROUNDS 32

#define L32(x)                                                                 \
  ((x) ^ ROL32((x), 2) ^ ROL32((x), 10) ^ ROL32((x), 18) ^ ROL32((x), 24))

#define ROUND_SBOX(x0, x1, x2, x3, x4, i)                                      \
  x4 = x1 ^ x2 ^ x3 ^ *(rk + i);                                               \
  x4 = S32(x4);                                                                \
  x4 = x0 ^ L32(x4)

#define ROUND_TBOX(x0, x1, x2, x3, x4, i)                                      \
  x4 = x1 ^ x2 ^ x3 ^ *(rk + i);                                               \
  t0 = ROL32(SMS4_T[(uint8_t)x4], 8);                                          \
  x4 >>= 8;                                                                    \
  x0 ^= t0;                                                                    \
  t0 = ROL32(SMS4_T[(uint8_t)x4], 16);                                         \
  x4 >>= 8;                                                                    \
  x0 ^= t0;                                                                    \
  t0 = ROL32(SMS4_T[(uint8_t)x4], 24);                                         \
  x4 >>= 8;                                                                    \
  x0 ^= t0;                                                                    \
  t1 = SMS4_T[x4];                                                             \
  x4 = x0 ^ t1

#define ROUND_DBOX(x0, x1, x2, x3, x4, i)                                      \
  x4 = x1 ^ x2 ^ x3 ^ *(rk + i);                                               \
  x4 = x0 ^ SMS4_D[(uint16_t)(x4 >> 16)] ^ ROL32(SMS4_D[(uint16_t)x4], 16)

#define S32(A)                                                                 \
  ((SMS4_S[((A) >> 24)] << 24) ^ (SMS4_S[((A) >> 16) & 0xff] << 16) ^          \
   (SMS4_S[((A) >> 8) & 0xff] << 8) ^ (SMS4_S[((A)) & 0xff]))

#define ROUNDS(x0, x1, x2, x3, x4)                                             \
  ROUND(x0, x1, x2, x3, x4, 0);                                                \
  ROUND(x1, x2, x3, x4, x0, 1);                                                \
  ROUND(x2, x3, x4, x0, x1, 2);                                                \
  ROUND(x3, x4, x0, x1, x2, 3);                                                \
  ROUND(x4, x0, x1, x2, x3, 4);                                                \
  ROUND(x0, x1, x2, x3, x4, 5);                                                \
  ROUND(x1, x2, x3, x4, x0, 6);                                                \
  ROUND(x2, x3, x4, x0, x1, 7);                                                \
  ROUND(x3, x4, x0, x1, x2, 8);                                                \
  ROUND(x4, x0, x1, x2, x3, 9);                                                \
  ROUND(x0, x1, x2, x3, x4, 10);                                               \
  ROUND(x1, x2, x3, x4, x0, 11);                                               \
  ROUND(x2, x3, x4, x0, x1, 12);                                               \
  ROUND(x3, x4, x0, x1, x2, 13);                                               \
  ROUND(x4, x0, x1, x2, x3, 14);                                               \
  ROUND(x0, x1, x2, x3, x4, 15);                                               \
  ROUND(x1, x2, x3, x4, x0, 16);                                               \
  ROUND(x2, x3, x4, x0, x1, 17);                                               \
  ROUND(x3, x4, x0, x1, x2, 18);                                               \
  ROUND(x4, x0, x1, x2, x3, 19);                                               \
  ROUND(x0, x1, x2, x3, x4, 20);                                               \
  ROUND(x1, x2, x3, x4, x0, 21);                                               \
  ROUND(x2, x3, x4, x0, x1, 22);                                               \
  ROUND(x3, x4, x0, x1, x2, 23);                                               \
  ROUND(x4, x0, x1, x2, x3, 24);                                               \
  ROUND(x0, x1, x2, x3, x4, 25);                                               \
  ROUND(x1, x2, x3, x4, x0, 26);                                               \
  ROUND(x2, x3, x4, x0, x1, 27);                                               \
  ROUND(x3, x4, x0, x1, x2, 28);                                               \
  ROUND(x4, x0, x1, x2, x3, 29);                                               \
  ROUND(x0, x1, x2, x3, x4, 30);                                               \
  ROUND(x1, x2, x3, x4, x0, 31)

typedef struct {
  uint32_t rk[SMS4_NUM_ROUNDS];
} sms4_key_t;

static uint32_t FK[4] = {
    0xa3b1bac6,
    0x56aa3350,
    0x677d9197,
    0xb27022dc,
};

static uint32_t CK[32] = {
    0x00070e15, 0x1c232a31, 0x383f464d, 0x545b6269, 0x70777e85, 0x8c939aa1,
    0xa8afb6bd, 0xc4cbd2d9, 0xe0e7eef5, 0xfc030a11, 0x181f262d, 0x343b4249,
    0x50575e65, 0x6c737a81, 0x888f969d, 0xa4abb2b9, 0xc0c7ced5, 0xdce3eaf1,
    0xf8ff060d, 0x141b2229, 0x30373e45, 0x4c535a61, 0x686f767d, 0x848b9299,
    0xa0a7aeb5, 0xbcc3cad1, 0xd8dfe6ed, 0xf4fb0209, 0x10171e25, 0x2c333a41,
    0x484f565d, 0x646b7279,
};

#define L32_(x) ((x) ^ ROL32((x), 13) ^ ROL32((x), 23))

#define ENC_ROUND(x0, x1, x2, x3, x4, i)                                       \
  x4 = x1 ^ x2 ^ x3 ^ *(CK + i);                                               \
  x4 = S32(x4);                                                                \
  x4 = x0 ^ L32_(x4);                                                          \
  *(rk + i) = x4

#define DEC_ROUND(x0, x1, x2, x3, x4, i)                                       \
  x4 = x1 ^ x2 ^ x3 ^ *(CK + i);                                               \
  x4 = S32(x4);                                                                \
  x4 = x0 ^ L32_(x4);                                                          \
  *(rk + 31 - i) = x4

static void sms4_encrypt_block16(const unsigned char *in, unsigned char *out,
                                 const sms4_key_t *key);

static void sms4_set_encrypt_key(sms4_key_t *key,
                                 const unsigned char user_key[16]);

static void sms4_set_decrypt_key(sms4_key_t *key,
                                 const unsigned char user_key[16]);

void sms4_encrypt_block16(const unsigned char *in, unsigned char *out,
                          const sms4_key_t *key) {
  const uint32_t *rk = key->rk;
  uint32_t x0, x1, x2, x3, x4;
  uint32_t t0, t1;

  x0 = GETU32(in);
  x1 = GETU32(in + 4);
  x2 = GETU32(in + 8);
  x3 = GETU32(in + 12);
#define ROUND ROUND_TBOX
  ROUNDS(x0, x1, x2, x3, x4);
#undef ROUND
  PUTU32(out, x0);
  PUTU32(out + 4, x4);
  PUTU32(out + 8, x3);
  PUTU32(out + 12, x2);
}

void sms4_set_encrypt_key(sms4_key_t *key, const unsigned char user_key[16]) {
  uint32_t *rk = key->rk;
  uint32_t x0, x1, x2, x3, x4;

  x0 = GETU32(user_key) ^ FK[0];
  x1 = GETU32(user_key + 4) ^ FK[1];
  x2 = GETU32(user_key + 8) ^ FK[2];
  x3 = GETU32(user_key + 12) ^ FK[3];

#define ROUND ENC_ROUND
  ROUNDS(x0, x1, x2, x3, x4);
#undef ROUND

  x0 = x1 = x2 = x3 = x4 = 0;
}

void sms4_set_decrypt_key(sms4_key_t *key, const unsigned char user_key[16]) {
  uint32_t *rk = key->rk;
  uint32_t x0, x1, x2, x3, x4;

  x0 = GETU32(user_key) ^ FK[0];
  x1 = GETU32(user_key + 4) ^ FK[1];
  x2 = GETU32(user_key + 8) ^ FK[2];
  x3 = GETU32(user_key + 12) ^ FK[3];

#define ROUND DEC_ROUND
  ROUNDS(x0, x1, x2, x3, x4);
#undef ROUND

  x0 = x1 = x2 = x3 = x4 = 0;
}

void sm4_encrypt(const unsigned char *in, unsigned char *out, int len,
                 const unsigned char *key, int key_len) {
  sms4_key_t skey;
  unsigned char ukey[16] = {0};
  unsigned char in_block[16] = {0};
  const unsigned char *in_p;
  unsigned char *out_p;

  int blocks;
  int extra;

  if (key_len >= 16) {
    memcpy(ukey, key, 16);
  } else {
    memcpy(ukey, key, key_len);
  }
  sms4_set_encrypt_key(&skey, ukey);

  blocks = len / 16;
  extra = len - blocks * 16;

  in_p = in;
  out_p = out;
  while (blocks--) {
    sms4_encrypt_block16(in_p, out_p, &skey);
    in_p += 16;
    out_p += 16;
  };

  blocks = len / 16;
  if (extra) {
    if (blocks > 0)
      out_p += 16;
    memcpy(in_block, in + blocks * 16, extra);
    sms4_encrypt_block16(in_block, out_p, &skey);
  }
}

void sm4_decrypt(const unsigned char *in, unsigned char *out, int len,
                 const unsigned char *key, int key_len) {
  sms4_key_t skey;
  unsigned char ukey[16];
  unsigned char in_block[16] = {0};
  const unsigned char *in_p;
  unsigned char *out_p;

  int blocks;
  int extra;

  if (key_len >= 16) {
    memcpy(ukey, key, 16);
  } else {
    memset(ukey, 0, 16);
    memcpy(ukey, key, key_len);
  }
  sms4_set_decrypt_key(&skey, ukey);

  blocks = len / 16;
  extra = len - blocks * 16;

  in_p = in;
  out_p = out;
  while (blocks--) {
    sms4_encrypt_block16(in_p, out_p, &skey);
    in_p += 16;
    out_p += 16;
  };

  blocks = len / 16;
  if (extra) {
    if (blocks > 0)
      out_p += 16;
    memcpy(in_block, in + blocks * 16, extra);
    sms4_encrypt_block16(in_block, out_p, &skey);
  }
}

int sm4_size(int len) {
  int blocks;
  blocks = len / 16;
  if (len == blocks * 16)
    return len;
  else
    return (blocks + 1) * 16;
}
