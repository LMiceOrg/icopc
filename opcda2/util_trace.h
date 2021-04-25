/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_TRACE_H_
#define OPCDA2_TRACE_H_

#include <stdio.h>
#if defined(_DEBUG)
#define trace_debug(format...)                                               \
  do {                                                                         \
    fprintf(stdout, ##format);                                             \
  } while (0)

#define wtrace_debug(format, args...)                                               \
  do {                                                                         \
    wprintf(format, ##args);                                             \
  } while (0)
#else
#define trace_debug(format, ...)
#define wtrace_debug(format, ...)
#endif

#define CoMemory

#endif  /** OPCDA2_TRACE_H_ */
