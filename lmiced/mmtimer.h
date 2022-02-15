/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef MMTIMER_H_
#define MMTIMER_H_

#include <windows.h>

/* 时钟数组大小：为16的倍数 */
#define MMTIME_MAX_SIZE (16 * 8)

typedef struct {
    int   used;
    UINT  period;
    UINT  last;
    void* data;
    /* return 0: peridoc, -1: on-shot */
    int (*timer_callback)(void* pdata, UINT tick);
} timer_data;

/**
 * tick: 单位：100纳秒 == 1 节拍
 */
typedef struct {
    MMRESULT         id;
    UINT             res;
    unsigned __int64 tick;
    timer_data       td[MMTIME_MAX_SIZE];
} timer_server;

#endif /*  MMTIMER_H_ */
