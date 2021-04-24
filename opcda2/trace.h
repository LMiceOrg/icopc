/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_TRACE_H_
#define OPCDA2_TRACE_H_

#include <stdio.h>
// #if defined(_DEBUG)
#define trace_debug(format, ...)                                               \
  do {                                                                         \
    printf(format, ##__VA_ARGS__);                                             \
  } while (0)

#define wtrace_debug(format, ...)                                               \
  do {                                                                         \
    wprintf(format, ##__VA_ARGS__);                                             \
  } while (0)
// #else
// #define trace_debug(format, ...)
// #define wtrace_debug(format, ...)
// #endif

#endif  /** OPCDA2_TRACE_H_ */
