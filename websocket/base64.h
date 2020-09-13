#ifndef BASE64_H
#define BASE64_H

#ifdef __cplusplus
extern "C" {
#endif

void base64_free(char *pbuf);
void base64_encrypt_text(const unsigned char *str, int bytes, char **pebuf,
                         int *elen);
void base64_encrypt_text2(const unsigned char *str, int nbytes, char *pbuf,
                          int elen);
int base64_encrypt_len(int bytes);
#ifdef __cplusplus
}
#endif

#endif  // BASE64_H
