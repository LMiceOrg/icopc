#include "stdafx.h"
#include "win_trace.h"
#include <io.h>
#include <stdio.h>
#include <string.h>

#define int64_t __int64
#define PERSEC 10000000I64
#define PERMIN (60*PERSEC)
#define PERHOUR (60*PERMIN)



#if !defined(_DEBUG)
const int lmice_trace_debug_mode = 0;
#else
const int lmice_trace_debug_mode = 1;
#endif

struct lmice_trace_name_s {
  lmice_trace_type_t type;
  WORD color;
  WORD padding;
  char name[16];
};

typedef struct lmice_trace_name_s lmice_trace_name_t;


lmice_trace_name_t lmice_trace_name[] = {
    {LMICE_TRACE_INFO,      10,  0,      " INFO    " /* light_green*/},
    {LMICE_TRACE_DEBUG,     11, 0,      " DEBUG   " /* light_cyan */},
    {LMICE_TRACE_WARNING,   14, 0,      " WARNING " /* light_yellow*/},
    {LMICE_TRACE_ERROR,     12, 0,      " ERROR   " /* light_red*/},
    {LMICE_TRACE_CRITICAL,  13, 0,      " CRITICAL" /* light_purple*/},
    {LMICE_TRACE_NONE,      7,  0,      "NULL" /* white */},
    {LMICE_TRACE_TIME,      15, 0,      "TIME" /* yellow*/},
    {LMICE_TRACE_LINE,      5, 0,      "LINE"}
};


static void get_system_time(int64_t* t)
{
    FILETIME ft_utc; 
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft_utc);
    FileTimeToLocalFileTime(&ft_utc, &ft);
    int64_t cur_tm = ft.dwHighDateTime;
    cur_tm = (cur_tm<<32)+ ft.dwLowDateTime;
    *t = cur_tm;
}
static int trace_date(int64_t trace_stm, char* trace_current_time) {

    FILETIME ft;
    SYSTEMTIME st;

    ft.dwHighDateTime = trace_stm>>32;
    ft.dwLowDateTime = trace_stm % 0xffffffff;
    FileTimeToSystemTime(&ft, &st);

    sprintf(trace_current_time, "%4d-%02d-%02d %02d:%02d:%02d.%07I64d",        
            st.wYear, st.wMonth, st.wDay, st.wHour,          
            st.wMinute, st.wSecond, trace_stm % PERSEC);

    return 27;

}

static int trace_time(int64_t trace_stm, char *trace_current_time) {
  int tm_sec = (trace_stm / PERSEC) % 60;
  int tm_min = (trace_stm / PERMIN) % 60;
  int tm_hour = (trace_stm / PERHOUR) % 24;
  int tm_nano100 = trace_stm % 10000000;
  int tm_day = (trace_stm / PERHOUR) / 24;

  /* ×î¶à720 Ìì */
  while(tm_day > 720) {
      tm_day -= 720;
  }
  
  //char trace_current_time[32] = {0};
  int day_pos = 0;
  int time_len = 10;
  int sec_pos = 0;
  int min_pos = 0;
  int hour_pos = 0;
  if (tm_day == 0 && tm_hour == 0) {
    if (tm_min == 0) {
      sec_pos = 0;
      time_len = 10;
    } else {
      sec_pos = 3;
      time_len = 13;
    }
  } else if(tm_day == 0 && tm_hour > 0){
    min_pos = 3;
    sec_pos = 6;
    time_len = 16;
  } else {
      hour_pos = 3;
      min_pos = 6;
      sec_pos = 9;
      time_len = 19;

  }
  // day
  trace_current_time[day_pos + 0] = '0' + (tm_day / 10);
  trace_current_time[day_pos + 1] = '0' + (tm_day % 10);
  trace_current_time[day_pos + 2] = ':';
  // hour
  trace_current_time[hour_pos + 0] = '0' + (tm_hour / 10);
  trace_current_time[hour_pos + 1] = '0' + (tm_hour % 10);
  trace_current_time[hour_pos + 2] = ':';
  // min
  trace_current_time[min_pos + 0] = '0' + (tm_min / 10);
  trace_current_time[min_pos + 1] = '0' + (tm_min % 10);
  trace_current_time[min_pos + 2] = ':';
  // sec
  trace_current_time[sec_pos + 0] = '0' + (tm_sec / 10);
  trace_current_time[sec_pos + 1] = '0' + (tm_sec % 10);
  trace_current_time[sec_pos + 2] = '.';
  trace_current_time[sec_pos + 3] = '0' + ((tm_nano100 % 10000000) / 1000000);
  trace_current_time[sec_pos + 4] = '0' + ((tm_nano100 % 1000000) / 100000);
  trace_current_time[sec_pos + 5] = '0' + ((tm_nano100 % 100000) / 10000);
  trace_current_time[sec_pos + 6] = '0' + ((tm_nano100 % 10000) / 1000);
  trace_current_time[sec_pos + 7] = '0' + ((tm_nano100 % 1000) / 100);
  trace_current_time[sec_pos + 8] = '0' + ((tm_nano100 % 100) / 10);
  trace_current_time[sec_pos + 9] = '0' + ((tm_nano100 % 10) / 1);
  return time_len;
}

void eal_trace_line(int ln, const char* nm) {
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    
    SetConsoleTextAttribute(hout, 
                          lmice_trace_name[LMICE_TRACE_LINE].color);
    printf("%s (%d)\n", nm, ln);
    
    SetConsoleTextAttribute(hout, 
                          lmice_trace_name[LMICE_TRACE_NONE].color);
}

void eal_trace_color_print_per_thread(int type) {

    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    int64_t cur_time = 0;
    char trace_current_time[32] = {0};
    int time_len = 0;

    static int64_t g_trace_count = 0;
    static int64_t g_last_time = 0;

    get_system_time(&cur_time); 
    
    if((g_trace_count % 1024) == 0 ) {
        time_len = trace_date(cur_time, trace_current_time);
        g_last_time = cur_time;
    } else {
        time_len = trace_time(cur_time - g_last_time, trace_current_time);
    }

    
    g_trace_count ++;

  SetConsoleTextAttribute(hout, 
                          lmice_trace_name[LMICE_TRACE_TIME].color);
  write(1, trace_current_time, time_len);

  SetConsoleTextAttribute(hout, 
                          lmice_trace_name[type].color);
  write(1, lmice_trace_name[type].name, 9);       
  
  SetConsoleTextAttribute(hout, 
                          lmice_trace_name[LMICE_TRACE_NONE].color);

}
