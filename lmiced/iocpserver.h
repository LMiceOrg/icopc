/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */

#ifndef IOCPSERVER_H
#define IOCPSERVER_H

#include <mswsock.h>

#include "lmice.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WORKER_THREAD 32

/* data to be associated for every I/O operation on a socket */
typedef struct _iocp_data {
    int (*proc_callback)(void *pdata, LPOVERLAPPED over, DWORD io_len);
    void (*proc_close)(void *ptr);
} iocp_data;

typedef struct _iocp_data_list {
    iocp_data *             data[32];
    struct _iocp_data_list *next;
} iocp_datas;

typedef struct iocp_server {
    int (*proc_callback)(void *ptr, LPOVERLAPPED over, DWORD io_len);
    void (*proc_close)(void *ptr);

    void(__stdcall *wait_callback)(void *ptr, BOOLEAN timer_or_wait);

    int (*iocp_register)(struct iocp_server *svr, iocp_data *data);        /* 注册iocp任务 */
    int (*iocp_unregister)(struct iocp_server *svr, iocp_data *data);      /* 注销iocp任务 */
    void *(*iocp_malloc)(struct iocp_server *svr, miu32 size);             /* 内存分配 */
    void (*iocp_free)(struct iocp_server *svr, void *ptr);                 /* 内存释放 */
    void *(*iocp_realloc)(struct iocp_server *svr, void *ptr, miu32 size); /* 内存重新分配 */

    miu32            lock;
    HANDLE           iocp;                           /* iocp handle*/
    DWORD            thread_count;                   /* worker thread count */
    HANDLE           thread_list[MAX_WORKER_THREAD]; /* worker thread array */
    int              opt_quit_flag;                  /* 退出标志 */
    CRITICAL_SECTION cs;                             /* 同步临界区 */
    iocp_datas       datas;                          /* iocp 任务列表 */
    DWORD            timeout_period;                 /* work thread 检查状态间隔 */
    mihandle         heap;                           /* 堆内存 */
    struct miboard * board;                          /* 公告板 */
    HANDLE           hboard;                         /* 公告板事件 */
    HANDLE           hwait;                          /*等待SPI发起事件 */

} iocp_server;

/* 启动iocp服务 */
iocp_server *iocp_server_start(DWORD period);

/* 停止iocp服务 */
void iocp_server_stop(iocp_server *svr);

/* 阻塞当前线程，直到iocp服务结束 */
void iocp_server_join(iocp_server *svr);

#ifdef __cplusplus
}
#endif

#endif /** IOCPSERVER_H */
