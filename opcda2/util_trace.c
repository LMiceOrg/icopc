/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include "util_trace.h"
#include <stdarg.h>
#include <stdio.h>

#include <sysinfoapi.h>
#include <timezoneapi.h>

#include "util_ticketlock.h"

typedef enum lmice_trace_type_t_ {
  LMICE_TRACE_INFO = 0,
  LMICE_TRACE_DEBUG = 1,
  LMICE_TRACE_WARNING = 2,
  LMICE_TRACE_ERROR = 3,
  LMICE_TRACE_CRITICAL = 4,
  LMICE_TRACE_NONE = 5,
  LMICE_TRACE_TIME
} lmice_trace_type_t;

struct lmice_trace_name_s;
typedef struct lmice_trace_name_s lmice_trace_name_t;

#if defined(_WIN32)

struct lmice_trace_name_s {
  lmice_trace_type_t type;
  WORD color;
  WORD padding;
  char name[16];
};

lmice_trace_name_t lmice_trace_name[] = {
    {LMICE_TRACE_INFO, 10, 0, " INFO" /* light_green*/},
    {LMICE_TRACE_DEBUG, 11, 0, " DEBUG" /* light_cyan */},
    {LMICE_TRACE_WARNING, 14, 0, " WARNING" /*yellow*/},
    {LMICE_TRACE_ERROR, 12, 0, " ERROR" /*light_red*/},
    {LMICE_TRACE_CRITICAL, 13, 0, " CRITICAL" /* light_purple*/},
    {LMICE_TRACE_NONE, 7, 0, "NULL" /* white */},
    {LMICE_TRACE_TIME, 14, 0, "TIME" /* yellow*/}};

#define LMAPI_WIN_COLOR_STDOUT(text, type)                                                                       \
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), lmice_trace_name[type].color); \
    printf(text);                                                                                          \
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), lmice_trace_name[LMICE_TRACE_NONE].color);

void trace_win32() {
    static ticketlock_t lock  = {0};
    static __int64     begin;
    FILETIME            now;
    SYSTEMTIME          st;
    LPSYSTEMTIME        pt = &st;
    LPFILETIME          ft = (LPFILETIME) &now;

    GetLocalTime(pt);
    SystemTimeToFileTime(pt, ft);

    eal_ticket_lock(&lock);

    if (lock.s.ticket == 0) {
        char buff[64];
        unsigned int nano100;
        /* 第1次, 第65536 次, ... */
        begin = (((__int64)now.dwHighDateTime) << 32) | (__int64)(now.dwLowDateTime);
        nano100 = (int)(begin % (__int64)10000000);

        snprintf(buff, sizeof(buff), "%4d-%02d-%02d %02d:%02d:%02d.%07u", pt->wYear, pt->wMonth, pt->wDay, pt->wHour,
                 pt->wMinute, pt->wSecond, nano100);
    }
}

#else

struct lmice_trace_name_s {
  lmice_trace_type_t type;
  char name[16];
  char color[16];
};
lmice_trace_name_t lmice_trace_name[] = {
    {LMICE_TRACE_INFO, "INFO", "\033[1;32m" /* light_green*/},
    {LMICE_TRACE_DEBUG, "DEBUG", "\033[1;36m" /* light_cyan */},
    {LMICE_TRACE_WARNING, "WARNING", "\033[1;33m" /*yellow*/},
    {LMICE_TRACE_ERROR, "ERROR", "\033[1;31m" /*light_red*/},
    {LMICE_TRACE_CRITICAL, "CRITICAL", "\033[1;35m" /* light_purple*/},
    {LMICE_TRACE_NONE, "NULL", "\033[0m"},
    {LMICE_TRACE_TIME, "TIME", "\033[1;33m" /*light_brown*/}};

#endif

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