#ifndef HEADER_MODES_LCL_H
#define HEADER_MODES_LCL_H

#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
typedef __int64          i64;
typedef unsigned __int64 u64;
#define U64(C) C##UI64
#elif defined(__arch64__)
typedef long          i64;
typedef unsigned long u64;
#define U64(C) C##UL
#else
typedef long long          i64;
typedef unsigned long long u64;
#define U64(C) C##ULL
#endif

typedef unsigned int  u32;
typedef unsigned char u8;

#define STRICT_ALIGNMENT 1
#ifndef PEDANTIC
#if defined(__i386) || defined(__i386__) || defined(__x86_64) || defined(__x86_64__) || defined(_M_IX86) || \
    defined(_M_AMD64) || defined(_M_X64) || defined(__aarch64__) || defined(__s390__) || defined(__s390x__)
#undef STRICT_ALIGNMENT
#endif
#endif

#if !defined(PEDANTIC) && !defined(OPENSSL_NO_ASM) && !defined(OPENSSL_NO_INLINE_ASM)
#if defined(__GNUC__) && __GNUC__ >= 2
#if defined(__x86_64) || defined(__x86_64__)
#define BSWAP8(x)                      \
    ({                                 \
        u64 ret_ = (x);                \
        asm("bswapq %0" : "+r"(ret_)); \
        ret_;                          \
    })
#define BSWAP4(x)                        \
    ({                                   \
        u32 ret_ = (x);                  \
        __asm("bswapl %0" : "+r"(ret_)); \
        ret_;                            \
    })
#elif (defined(__i386) || defined(__i386__)) && !defined(I386_ONLY)
#define BSWAP8(x)                                           \
    ({                                                      \
        u32 lo_ = (u64) (x) >> 32, hi_ = (x);               \
        asm("bswapl %0; bswapl %1" : "+r"(hi_), "+r"(lo_)); \
        (u64) hi_ << 32 | lo_;                              \
    })
#define BSWAP4(x)                        \
    ({                                   \
        u32 ret_ = (x);                  \
        __asm("bswapl %0" : "+r"(ret_)); \
        ret_;                            \
    })
#elif defined(__aarch64__)
#define BSWAP8(x)                               \
    ({                                          \
        u64 ret_;                               \
        asm("rev %0,%1" : "=r"(ret_) : "r"(x)); \
        ret_;                                   \
    })
#define BSWAP4(x)                                 \
    ({                                            \
        u32 ret_;                                 \
        asm("rev %w0,%w1" : "=r"(ret_) : "r"(x)); \
        ret_;                                     \
    })
#elif (defined(__arm__) || defined(__arm)) && !defined(STRICT_ALIGNMENT)
#define BSWAP8(x)                                           \
    ({                                                      \
        u32 lo_ = (u64) (x) >> 32, hi_ = (x);               \
        asm("rev %0,%0; rev %1,%1" : "+r"(hi_), "+r"(lo_)); \
        (u64) hi_ << 32 | lo_;                              \
    })
#define BSWAP4(x)                                       \
    ({                                                  \
        u32 ret_;                                       \
        asm("rev %0,%1" : "=r"(ret_) : "r"((u32) (x))); \
        ret_;                                           \
    })
#endif
#elif defined(_MSC_VER)
#if _MSC_VER >= 1300
#pragma intrinsic(_byteswap_uint64, _byteswap_ulong)
#define BSWAP8(x) _byteswap_uint64((u64) (x))
#define BSWAP4(x) _byteswap_ulong((u32) (x))
#elif defined(_M_IX86)
__inline u32 _bswap4(u32 val) { _asm mov eax, val _asm bswap eax }
#define BSWAP4(x) _bswap4(x)
#endif
#endif
#endif
#if defined(BSWAP4) && !defined(STRICT_ALIGNMENT)
#define GETU32(p) BSWAP4(*(const u32 *) (p))
#define PUTU32(p, v) *(u32 *) (p) = BSWAP4(v)
#else
#define GETU32(p) ((u32) (p)[0] << 24 | (u32) (p)[1] << 16 | (u32) (p)[2] << 8 | (u32) (p)[3])
#define PUTU32(p, v) ((p)[0] = (u8) ((v) >> 24), (p)[1] = (u8) ((v) >> 16), (p)[2] = (u8) ((v) >> 8), (p)[3] = (u8) (v))
#endif

#endif
