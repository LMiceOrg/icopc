/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_UTIL_ATOMIC_H_
#define OPCDA2_UTIL_ATOMIC_H_

/** Pause instruction to prevent excess processor bus usage */
#define cpu_relax() \
  { __asm volatile("pause" ::: "memory"); }

#define cpu_nop() \
  { __asm volatile("nop" ::: "memory"); }

/** Compile read - write barrier */
#define barrier() \
  { __asm volatile("" : : : "memory"); }

/** atomic  Add */
#define eal_atomic_xadd(P, V) __sync_fetch_and_add((P), (V))

/* compare and swap */
#define eal_atomic_cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))

#endif  /** OPCDA2_UTIL_ATOMIC_H_ */

