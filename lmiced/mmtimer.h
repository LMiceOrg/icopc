/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef MMTIMER_H_
#define MMTIMER_H_

#include <windows.h>

#define MMTIME_MAX_SIZE 128

typedef struct {
    int used;
    UINT period;
    UINT last;
    void* data;
    /* return 0: peridoc, -1: on-shot */
    int (*timer_callback)(void *pdata, UINT tick);
} timer_data;

typedef struct {
    MMRESULT id;
    UINT res;
    UINT tick;
    timer_data td[MMTIME_MAX_SIZE];
} timer_server;

#endif  /*  MMTIMER_H_ */
