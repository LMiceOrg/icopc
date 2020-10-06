#ifndef SM4_H
#define SM4_H

int sm4_size(int len);

void sm4_encrypt(const unsigned char *in, unsigned char *out, int len,
                 const unsigned char *key, int key_len);

void sm4_decrypt(const unsigned char *in, unsigned char *out, int len,
                 const unsigned char *key, int key_len);

#endif // SM4_H
