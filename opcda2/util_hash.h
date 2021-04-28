/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_UTIL_HASH_H_
#define OPCDA2_UTIL_HASH_H_

unsigned int eal_hash32_fnv1a(const void* data, unsigned int size);
unsigned int eal_hash16_fnv1a(const void* data, unsigned int size);
unsigned int eal_hash32_fnv1a_more(const void* data, unsigned int size, unsigned int hval);
unsigned int eal_hash32_to16(unsigned int hash);

/* return hash [0, ...n) */
unsigned int eal_hash32_ton(unsigned int hash, unsigned int n);

#endif  /** OPCDA2_UTIL_HASH_H_ */
