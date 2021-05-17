/** Copyright 2018 He Hao<hehaoslj@sina.com> */

#ifndef WIN_TRACE_H_
#define WIN_TRACE_H_


#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum lmice_trace_type_t_ {
  LMICE_TRACE_INFO = 0,
  LMICE_TRACE_DEBUG = 1,
  LMICE_TRACE_WARNING = 2,
  LMICE_TRACE_ERROR = 3,
  LMICE_TRACE_CRITICAL = 4,
  LMICE_TRACE_NONE = 5,
  LMICE_TRACE_TIME,
  LMICE_TRACE_LINE
} lmice_trace_type_t;

void eal_trace_line(int ln, const char* nm);
void eal_trace_color_print_per_thread(int type);

#define EAL_TRACE_COLOR_PRINT_THREAD(x)              \
  eal_trace_color_print_per_thread(LMICE_TRACE_##x); \
  printf

#define EAL_TRACE_COLOR_PRINT_THREAD_W(x)              \
  eal_trace_color_print_per_thread(LMICE_TRACE_##x); \
  wprintf


#define lmice_info_print EAL_TRACE_COLOR_PRINT_THREAD(INFO)
#define lmice_debug_print                   \
  eal_trace_line(__LINE__, __FILE__);       \
  EAL_TRACE_COLOR_PRINT_THREAD(DEBUG)
#define lmice_warning_print EAL_TRACE_COLOR_PRINT_THREAD(WARNING)
#define lmice_error_print EAL_TRACE_COLOR_PRINT_THREAD(ERROR)
#define lmice_critical_print EAL_TRACE_COLOR_PRINT_THREAD(CRITICAL)

#define lmice_info_wprint EAL_TRACE_COLOR_PRINT_THREAD_W(INFO)
#define lmice_debug_wprint                   \
  eal_trace_line(__LINE__, __FILE__);       \
  EAL_TRACE_COLOR_PRINT_THREAD_W(DEBUG)
#define lmice_warning_wprint EAL_TRACE_COLOR_PRINT_THREAD_W(WARNING)
#define lmice_error_wprint EAL_TRACE_COLOR_PRINT_THREAD_W(ERROR)
#define lmice_critical_wprint EAL_TRACE_COLOR_PRINT_THREAD_W(CRITICAL)




#ifdef __cplusplus
}
#endif

#endif /* WIN_TRACE_H_ */
