/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include "lmice_eal_common.h"
eal_inline void eal_int_str(char* str, unsigned int id) {
    const char key[] = "0123456789ABCDEF";
    int        i;
    for (i = 0; i < 8; ++i) {
        str[i] = key[(id >> (4 * i)) % 0x10];
    }
}
