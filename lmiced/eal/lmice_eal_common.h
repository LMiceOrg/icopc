/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef EAL_LMICE_EAL_COMMON_H_
#define EAL_LMICE_EAL_COMMON_H_

#include <stddef.h>

/**
 Compiler                   C macros                         C++ macros
Clang/LLVM                 clang -dM -E -x c /dev/null      clang++ -dM -E -x c++ /dev/null
GNU GCC/G++                gcc   -dM -E -x c /dev/null      g++     -dM -E -x c++ /dev/null
Hewlett-Packard C/aC++     cc    -dM -E -x c /dev/null      aCC     -dM -E -x c++ /dev/null
IBM XL C/C++               xlc   -qshowmacros -E /dev/null  xlc++   -qshowmacros -E /dev/null
Intel ICC/ICPC             icc   -dM -E -x c /dev/null      icpc    -dM -E -x c++ /dev/null
Microsoft Visual Studio (none)                              (none)
Oracle Solaris Studio      cc    -xdumpmacros -E /dev/null  CC      -xdumpmacros -E /dev/null
Portland Group PGCC/PGCPP  pgcc  -dM -E                     (none)

vc http://msdn.microsoft.com/en-us/library/b0084kay%28v=vs.110%29.aspx


 */

/** c version */
#if defined(__STDC__) && defined(__STDC_VERSION__)
#if __STDC_VERSION__ >= 199901L
#define EAL_STDC_VER 99
#elif __STDC_VERSION__ >= 199409L
#define EAL_STDC_VER 94
#endif
#elif defined(__STDC__)
#define EAL_STDC_VER 90
#endif

#if EAL_STDC_VER == 90
#define inline __inline
#endif

/** supported compiler: clang gcc msvc */
#define EAL_CC_MSC 0
#if defined(__clang__) /** clang */
#include <unistd.h>
#define eal_pack(_x) __attribute__((__aligned__((_x))))
#define eal_inline inline __attribute__((always_inline))
#define eal_noexport __attribute__((visibility("hidden")))
#define __int64 long long
#elif defined(__GNUC__) /** GCC */
#include <unistd.h>
#include "sys/types.h"
#define eal_pack(_x) __attribute__((__aligned__((_x))))
#define eal_inline __attribute__((always_inline)) inline static
#define eal_noexport __attribute__((visibility("hidden")))

/** mingw */
#if defined(__MINGW32__)
#else
#define __int64 long long
#endif

#elif defined(_MSC_VER) /** MSC */
#include <io.h>
#include <process.h>
#define eal_pack(_x) __declspec(align(_x))
#define eal_inline static __forceinline
#define eal_noexport

#undef EAL_CC_MSC
#define EAL_CC_MSC 1

#define EAL_STDC_VER 90

/* _MI_PPC arch BIG_ENDIAN:XBox  */
#define __ORDER_LITTLE_ENDIAN__ 1234
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__

#if _WIN64
#define EAL_ARCH 64
#define EAL_ARCH64 1
#define EAL_ARCH32 0
#else
#define EAL_ARCH 32
#define EAL_ARCH64 0
#define EAL_ARCH32 1
#endif

#else /** Other compiler */
#error(Unsupported compiler)
#endif

/* endian */
#define eal_is_little_endian() (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define eal_is_big_endian() (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)

/* int64 support  */
#undef EAL_INT64
#if EAL_CC_MSC == 0 && EAL_STDC_VER == 90 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
typedef union eal_int64 {
    struct {
        int low;
        int high;
    } i64;
    double dval;
} eal_int64;
typedef union eal_uint64 {
    struct {
        unsigned int low;
        unsigned int high;
    } i64;
    double dval;
} eal_uint64;
#define EAL_INT64
#define eali64 eal_int64
#define ealu64 eal_uint64
#elif EAL_CC_MSC == 0 && EAL_STDC_VER == 90 && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
typedef union eal_int64 {
    struct {
        int high;
        int low;
    } i64;
    double dval;
} eal_int64;
typedef union eal_uint64 {
    struct {
        unsigned int high;
        unsigned int low;
    } i64;
    double dval;
} eal_uint64;
#define EAL_INT64
#define eali64 eal_int64
#define ealu64 eal_uint64

#elif EAL_CC_MSC == 1 /* msc */
#define eali64 __int64
#define ealu64 unsigned __int64
#endif



#endif  /** EAL_LMICE_EAL_COMMON_H_ */
