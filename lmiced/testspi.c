/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */

#include <stdio.h>
#include <string.h>

#include "eal/lmice_eal_hash.h"

int main() {
    char   key[] = "hello";
    ealu64 hval;
    hval = eal_hash64_fnv1a(key, strlen(key));
#ifdef EAL_INT64
    printf("hval :0x%08x%08x\n", hval.i64.high, hval.i64.low);
#else
    printf("hval :0x%016llx\n", hval);
#endif
    return 0;
}
