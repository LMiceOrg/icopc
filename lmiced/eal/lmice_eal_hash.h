/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef EAL_LMICE_EAL_HASH_
#define EAL_LMICE_EAL_HASH_

#include "lmice_eal_common.h"

unsigned int eal_hash32_fnv1a(const void* data, unsigned int size);
unsigned int eal_hash16_fnv1a(const void* data, unsigned int size);
unsigned int eal_hash32_fnv1a_more(const void* data, unsigned int size, unsigned int hval);
unsigned int eal_hash32_to16(unsigned int hash);

/* return hash [0, ...n) */
unsigned int eal_hash32_ton(unsigned int hash, unsigned int n);


ealu64 eal_hash64_fnv1a(const void* data, unsigned int size);
ealu64 eal_hash64_fnv1a_more(const void* data, unsigned int size, ealu64 hval);

#endif  /** EAL_LMICE_EAL_HASH_ */
