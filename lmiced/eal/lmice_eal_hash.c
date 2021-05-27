/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */

#include "lmice_eal_hash.h"
#include "lmice_eal_util.h"

#define MASK_16 (((unsigned int) 1 << 16) - 1) /* i.e., (u_int32_t)0xffff */

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
    unsigned int         i  = 0;
    const unsigned char* dp = (const unsigned char*) data;

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
    hash              = (hash >> 16) ^ (hash & MASK_16);
    return hash;
}

unsigned int eal_hash32_to16(unsigned int hash) {
    hash = (hash >> 16) ^ (hash & MASK_16);
    return hash;
}

#define MAX_32BIT ((unsigned int) 0xffffffff) /* largest 32 bit unsigned value */
#define RETRY_LEVEL(n) ((MAX_32BIT / n) * n)
#define FNV_32_PRIME ((unsigned int) 16777619)
#define FNV1_32_INIT ((unsigned int) 2166136261)

unsigned int eal_hash32_ton(unsigned int hash, unsigned int n) {
#if defined(HASH_RETRY_MODE)
    while (hash >= RETRY_LEVEL(n)) {
        hash = (hash * FNV_32_PRIME) + FNV1_32_INIT;
    }
#endif
    hash %= n;
    return hash;
}

ealu64 eal_hash64_fnv1a(const void* data, unsigned int len) {
    unsigned int         i  = 0;
    const unsigned char* dp = (const unsigned char*) data;
#ifndef EAL_INT64
    ealu64 hval = 14695981039346656037ULL;
    for (i = 0; i < len; ++i) {
        hval ^= (ealu64) *dp++;
#if defined(_DEBUG)
        hval *= 1099511628211ULL;
#else
        hval += (hval << 1) + (hval << 4) + (hval << 5) + (hval << 7) + (hval << 8) + (hval << 40);
#endif
    }
#else
    ealu64 hval; /** 14695981039346656037ULL */
    ealu64 val; /** 1099511628211ULL  */

    hval.i64.high = 0xcbf29ce4;
    hval.i64.low = 0x84222325;

    val.i64.high = 0x0100;
    val.i64.low = 0x000001b3;

    for (i = 0; i < len; ++i) {
        unsigned int a, b, c, d, e, f, g;
        unsigned int tmp;
        unsigned int high, low;
        hval.i64.low ^= (unsigned int) *dp++;

        /* 16bit: a,b,c,d * e,f,g */
        a = hval.i64.high >> 16;
        b = hval.i64.high & 0xffff;
        c = hval.i64.low >> 16;
        d = hval.i64.low & 0xffff;

        e = val.i64.high & 0xffff;
        f = val.i64.low >> 16;
        g = val.i64.low & 0xffff;

        /* 1-16 bit */
        tmp = d * g;
        low = tmp & 0xffff;

        /* 16-32 bit */
        tmp >>= 16;
        tmp += d * f + c * g;
        low += (tmp & 0xffff) << 16;

        /* 32-48 bit */
        tmp >>= 16;
        tmp += d * e + c * f + b * g;
        high = (tmp & 0xffff);

        /* 48-64 bit */
        tmp >>= 16;
        tmp += c * e + b * f + a * g;
        high += (tmp & 0xffff) << 16;

        hval.i64.high = high;
        hval.i64.low = low;
    }
#endif
    return hval;
}

ealu64 eal_hash64_more_fnv1a(const void* data, unsigned int len, ealu64 oldval) {
    unsigned int         i  = 0;
    const unsigned char* dp = (const unsigned char*) data;
#ifndef EAL_INT64
    ealu64 hval = oldval;
    for (i = 0; i < len; ++i) {
        hval ^= (ealu64) *dp++;
#if defined(_DEBUG)
        hval *= 1099511628211ULL;
#else
        hval += (hval << 1) + (hval << 4) + (hval << 5) + (hval << 7) + (hval << 8) + (hval << 40);
#endif
    }
#else
    ealu64 hval; /** oldval */
    ealu64 val; /** 1099511628211ULL  */

    val.i64.high = 0x100;
    val.i64.low = 0x000001b3;


    hval.i64.high = oldval.i64.high;
    hval.i64.low = oldval.i64.low;


    for (i = 0; i < len; ++i) {
        unsigned int a, b, c, d, e, f, g;
        unsigned int tmp;
        unsigned int low, high;
        hval.i64.low ^= (unsigned int) *dp++;

        /* 16bit: a,b,c,d * e,f,g */
        a = hval.i64.high >> 16;
        b = hval.i64.high & 0xffff;
        c = hval.i64.low >> 16;
        d = hval.i64.low & 0xffff;

        e = val.i64.high & 0xffff;
        f = val.i64.low >> 16;
        g = val.i64.low & 0xffff;

        /* 1-16 bit */
        tmp = d * g;
        low = tmp & 0xffff;

        /* 16-32 bit */
        tmp >>= 16;
        tmp += d * f + c * g;
        low += (tmp & 0xffff) << 16;

        /* 32-48 bit */
        tmp >>= 16;
        tmp += d * e + c * f + b * g;
        high = (tmp & 0xffff);

        /* 48-64 bit */
        tmp >>= 16;
        tmp += c * e + b * f + a * g;
        high += (tmp & 0xffff) << 16;

        hval.i64.high = high;
        hval.i64.low = low;
    }

#endif
    return hval;
}

