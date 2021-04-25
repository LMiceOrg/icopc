/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include "util_trace.h"
#include <stdarg.h>
#include <stdio.h>

void trace_debug(const char* format, ...) {
    va_list vlist;
    va_start(vlist, format);
    vprintf(format, vlist);
    va_end(vlist);
}

void wtrace_debug(const wchar_t* format, ...) {
    va_list vlist;
    va_start(vlist, format);
    vwprintf(format, vlist);
    va_end(vlist);
}