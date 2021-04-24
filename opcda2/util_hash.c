/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */

#include "util_hash.h"

#define MASK_16 (((unsigned int)1<<16)-1) /* i.e., (u_int32_t)0xffff */

unsigned int eal_hash32_fnv1a(const void* data, unsigned int len) {
    unsigned int         i    = 0;
    unsigned int         hval = 2166136261UL;
    const unsigned char* dp   = (const unsigned char*) data;

    /** xor the bottom with the current octet */
    for (i = 0; i < len; ++i) {
        hval ^= (unsigned int) *dp++;

        /* multiply by the 32 bit FNV magic prime mod 2^32 */
#if defined(_DEBUG)
        hval *= 16777619UL;
#else
        hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24);
#endif
    }
    return hval;
}

unsigned int eal_hash32_fnv1a_more(const void* data, unsigned int len, unsigned int hval) {
    unsigned int         i    = 0;
    const unsigned char* dp   = (const unsigned char*) data;

    /** xor the bottom with the current octet */
    for (i = 0; i < len; ++i) {
        hval ^= (unsigned int) *dp++;

        /* multiply by the 32 bit FNV magic prime mod 2^32 */
#if defined(_DEBUG)
        hval *= 16777619UL;
#else
        hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24);
#endif
    }
    return hval;
}

unsigned int eal_hash16_fnv1a(const void* data, unsigned int len) {
    unsigned int hash = eal_hash32_fnv1a(data, len);
    hash = (hash>>16) ^ (hash & MASK_16);
    return hash;
}

unsigned int eal_hash32_to16(unsigned int hash) {
    hash = (hash>>16) ^ (hash & MASK_16);
    return hash;
}