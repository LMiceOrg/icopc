/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */

#include "mmtimer.h"

static __inline UINT mmtimer_res() {
    TIMECAPS ts;
    UINT min_cap = 1;
    if (timeGetDevCaps(&ts, sizeof(ts)) == TIMERR_NOERROR) {
        min_cap = min(max(ts.wPeriodMin, 1), ts.wPeriodMax);
    }
    return min_cap;
}

void CALLBACK mmtimer_main(UINT id, UINT msg, DWORD_PTR user, DWORD_PTR dw1, DWORD_PTR dw2) {
    int           i;
    timer_server* ts = (timer_server*) user;

    ts->tick += ts->res;

    for (i = 0; i < MMTIME_MAX_SIZE; ++i) {
        timer_data* td = ts->td + i;
        if (td->used) {
            if (td->period + td->last < ts->tick) continue;

            td->last = ts->tick;
            td->timer_callback(td->data, ts->tick);
        }
    }
}

void mmtimer_start(timer_server* ts) {
    UINT res;
    MMRESULT id;
    res = mmtimer_res();
    timeBeginPeriod(res);

    id = timeSetEvent(res, res, mmtimer_main, (DWORD_PTR)ts, TIME_PERIODIC);
    ts->res = res;
    ts->id = id;
}

void mmtimer_stop(timer_server* ts) {
    timeKillEvent(ts->id);
    timeEndPeriod(ts->res);
}
