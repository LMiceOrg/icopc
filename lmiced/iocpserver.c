/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include "iocpserver.h"

#include <winsock2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heap.h"
#include "lmice_private.h"

static __inline void socket_init(void);

static DWORD WINAPI iocp_server_main(LPVOID context);

static void __stdcall iocp_wait_callback(void* ptr, BOOLEAN timer_or_wait);

eal_inline int svr_fire_event(miboard* board, miu32 type, mihandle handle);

void socket_init() {
    WSADATA wsaData;

    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void* iocp_malloc(iocp_server* svr, miu32 size) { return miheap_malloc(svr->heap, size); }

void  iocp_free(iocp_server* svr, void* ptr) { miheap_free(svr->heap, ptr); }
void* iocp_realloc(iocp_server* svr, void* ptr, miu32 size) { return miheap_realloc(svr->heap, ptr, size); }

void __stdcall iocp_wait_callback(void* ptr, BOOLEAN timer_or_wait) {
    iocp_server* svr  = (iocp_server*) ptr;
    node_head*   node = (node_head*) ((miu8*) svr->board - sizeof(node_head));
    miu32        i;
    miu32        size;
    mispi_event  event[512];

    (void) timer_or_wait;

    /* 获取SPI事件列表 */
    EAL_ticket_lock(&node->lock);
    size = svr->board->size;
    if (size > 0 && size <= BOARD_SPIEVENT_SIZE) {
        memcpy(event, svr->board->event, svr->board->size * sizeof(mispi_event));
    } else {
        size = 0;
    }
    svr->board->size = 0;
    EAL_ticket_unlock(&node->lock);

    /* 处理SPI事件 */
    for (i = 0; i < size; ++i) {
        mispi_event* ev = event + i;
        miboard*     board;
        mihandle     handle;
        board = (miboard*) miheap_open(svr->heap, ev->handle.heap.heap_id, ev->handle.heap.node_id);
        switch (ev->type) {
            case MICLIENTCREATE:
                handle.i32 = MIOK;
                svr_fire_event(board, ev->type, handle);
                break;
            case MICLIENTRELEASE:
                break;
            case MICLIENTPUBCREATE:
                /*注册发布资源 */
                do {
                    mipub_event* pubev = (mipub_event*) ev;
                    miu32        size  = pubev->size;
                    miu8*        data  = NULL;
                    node_head*   head  = NULL;

                    /* 资源是可变长: 更新长度 */
                    if (size == MIRESVARLENGTH) size = MIRESSIZE;
                    /* 分配发布数据区 */
                    data = miheap_malloc(svr->heap, size);
                    if (data == NULL) break;

                    head = miheap_data_head(data);
                    #ifdef EAL_INT64
                        head->ctick.i64.high = 0;
                        head->ctick.i64.low = 0;
                        #else
                        heap->ctick = 0;
                        #endif
                } while (0);
                break;
        }
    }
}

int iocp_register(iocp_server* svr, iocp_data* data) {
    iocp_datas* datas;
    EnterCriticalSection(&svr->cs);
    datas = &svr->datas;

    do {
        int i;
        int finish = 0;
        for (i = 0; i < 32; ++i) {
            if (datas->data[i] == NULL) {
                datas->data[i] = data;
                finish         = 1;
                break;
            }
        }
        if (finish == 1) break;

        if (datas->next == NULL) {
            datas->next = (iocp_datas*) heap_malloc(sizeof(iocp_datas));
        }
        datas = datas->next;
    } while (datas);
    LeaveCriticalSection(&svr->cs);

    return 0;
}

int iocp_unregister(iocp_server* svr, iocp_data* data) {
    iocp_datas* datas;
    int         finish = 0;
    EnterCriticalSection(&svr->cs);
    datas = &svr->datas;
    do {
        int i;

        for (i = 0; i < 32; ++i) {
            if (datas->data[i] == data) {
                datas->data[i]->proc_close(data);
                datas->data[i] = NULL;
                finish         = 1;
                break;
            }
        }
        if (finish == 1) break;

        datas = datas->next;
    } while (datas);
    LeaveCriticalSection(&svr->cs);

    return finish;
}

iocp_server* iocp_server_start(DWORD period) {
    DWORD        dwCPU;
    SYSTEM_INFO  systemInfo;
    iocp_server* svr;
    svr                  = (iocp_server*) heap_malloc(sizeof(iocp_server));
    svr->iocp_register   = iocp_register;
    svr->iocp_unregister = iocp_unregister;
    svr->timeout_period  = period;

    svr->wait_callback = iocp_wait_callback;

    InitializeCriticalSection(&svr->cs);

    socket_init();

    svr->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    svr->heap         = miheap_create(0);
    svr->iocp_malloc  = iocp_malloc;
    svr->iocp_free    = iocp_free;
    svr->iocp_realloc = iocp_realloc;
    /* 创建board */
    svr->board = (miboard*) miheap_malloc(svr->heap, sizeof(miboard));
    memset(svr->board, 0, sizeof(miboard));
    memcpy(svr->board->ename, SVRBRD_EVENT_NAME, sizeof(SVRBRD_EVENT_NAME) - 1);
    svr->hboard = CreateEvent(NULL, FALSE, FALSE, svr->board->ename);

    /* 注册board事件回调 */
    RegisterWaitForSingleObject(&svr->hwait, svr->hboard, svr->wait_callback, (void*) svr, INFINITE, WT_EXECUTEDEFAULT);

    GetSystemInfo(&systemInfo);
    svr->thread_count = systemInfo.dwNumberOfProcessors * 2;

    if (svr->thread_count < 1) {
        svr->thread_count = 1;
    }

    if (svr->thread_count > MAX_WORKER_THREAD) {
        svr->thread_count = MAX_WORKER_THREAD;
    }
    for (dwCPU = 0; dwCPU < svr->thread_count; dwCPU++) {
        HANDLE thread = INVALID_HANDLE_VALUE;
        DWORD  tid    = 0;

        thread                  = CreateThread(NULL, 0, iocp_server_main, svr, 0, &tid);
        svr->thread_list[dwCPU] = thread;
    }
    return svr;
}

void iocp_server_stop(iocp_server* svr) {
    DWORD       i;
    iocp_datas* datas;
    if (svr->iocp == NULL) return;

    if (svr->iocp) {
        EnterCriticalSection(&svr->cs);
        svr->opt_quit_flag = 1;
        LeaveCriticalSection(&svr->cs);
        for (i = 0; i < svr->thread_count; i++) PostQueuedCompletionStatus(svr->iocp, 0, 0, NULL);
    }

    /* Make sure worker threads exits. */
    if (WAIT_OBJECT_0 != WaitForMultipleObjects(svr->thread_count, svr->thread_list, TRUE, 1000)) {
        printf("WaitForMultipleObjects() failed: %ld\n", GetLastError());
    } else {
        for (i = 0; i < svr->thread_count; i++) {
            if (svr->thread_list[i] != INVALID_HANDLE_VALUE) CloseHandle(svr->thread_list[i]);
            svr->thread_list[i] = INVALID_HANDLE_VALUE;
        }
    }

    if (svr->iocp) {
        CloseHandle(svr->iocp);
        svr->iocp = NULL;
    }
    DeleteCriticalSection(&svr->cs);

    datas = &svr->datas;
    for (i = 0; i < 32; ++i) {
        if (datas->data[i] != NULL) {
            datas->data[i]->proc_close(datas->data[i]);
            datas->data[i] = NULL;
        }
    }

    datas = datas->next;
    while (datas != NULL) {
        for (i = 0; i < 32; ++i) {
            if (datas->data[i] != NULL) {
                datas->data[i]->proc_close(datas->data[i]);
                datas->data[i] = NULL;
            }
        }
        heap_free(datas);
        datas = datas->next;
    }

    miheap_release(svr->heap);

    heap_free(svr);
}

void iocp_server_join(iocp_server* svr) { WaitForMultipleObjects(svr->thread_count, svr->thread_list, TRUE, INFINITE); }

DWORD WINAPI iocp_server_main(LPVOID context) {
    iocp_server* svr;

    BOOL         state = FALSE;
    int          ret;
    LPOVERLAPPED overlapped = NULL;
    iocp_data*   data       = NULL;
    DWORD        io_size    = 0;

    svr = (iocp_server*) context;

    while (svr->opt_quit_flag == 0) {
        /* continually loop to service io completion packets */
        state = GetQueuedCompletionStatus(svr->iocp, &io_size, (PDWORD_PTR) &data, &overlapped,
                                          svr->timeout_period); /**< wait 10*1000 mill-secs */

        if (data == NULL) return (0);

        if (state == FALSE) {
            printf("state false\n");
        }
        ret = data->proc_callback(data, overlapped, io_size);
        if (ret == -1) {
            iocp_unregister(svr, data);
        }
    } /* end: while */
    return (0);
}

int svr_fire_event(miboard* board, miu32 type, mihandle handle) {
    int ret = 0;
    EAL_data_lock(board);
    if (board->size < BOARD_SPIEVENT_SIZE) {
        mispi_event* ev = board->event + board->size;
        ev->size        = 1;
        ev->type        = type;
        ev->handle      = handle;
        ++board->size;
    } else {
        ret = 1;
    }
    EAL_data_unlock(board);
    return ret;
}
