#ifndef SHA1_H
#define SHA1_H

/*
   SHA-1 in C
   By Steve Reid <steve@edmweb.com>
   100% Public Domain
 */

#if _MSC_VER == 1200
    #ifndef uint8_t
    #define uint8_t  unsigned char 
    #define uint16_t unsigned short 
    #define uint32_t unsigned int 
    #define uint64_t unsigned __int64 

    #define   int8_t char
    #define  int16_t short
    #define  int32_t int
    #define  int64_t __int64
    #endif
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(
    uint32_t state[5],
    const unsigned char buffer[64]
    );

void SHA1Init(
    SHA1_CTX * context
    );

void SHA1Update(
    SHA1_CTX * context,
    const unsigned char *data,
    uint32_t len
    );

void SHA1Final(
    unsigned char digest[20],
    SHA1_CTX * context
    );

void SHA1(
    char *hash_out,
    const char *str,
    int len);

#ifdef __cplusplus
}
#endif

#endif /* SHA1_H */

