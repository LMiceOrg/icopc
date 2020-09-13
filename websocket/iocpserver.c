#define WIN32_LEAN_AND_MEAN



#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "iocpserver.h"



static __inline void socket_init(void);

static __inline void iocp_server_proc_close(iocp_server* svr, iocp_data* data);

static DWORD WINAPI iocp_server_main ( LPVOID context );

void socket_init() {
    WSADATA wsaData;
    int ret;
    ret = WSAStartup(MAKEWORD(2,2), &wsaData);

}

int iocp_register(iocp_server* svr, iocp_data* data) {
    iocp_datas* datas;
    EnterCriticalSection(&svr->cs);
    datas = &svr->datas;
    do {
        int i;
        int finish = 0;
        for(i=0; i<32; ++i) {
            if(datas->data[i] == NULL) {
                datas->data[i] = data;
                finish = 1;
                break;
            }
        }
        if(finish == 1) break;

        if(datas->next == NULL) {
            datas->next = (iocp_datas*)iocp_malloc(sizeof(iocp_datas));
        }
        datas = datas->next;

    } while(datas);
    LeaveCriticalSection(&svr->cs);

    return 0;
}

int iocp_unregister(iocp_server* svr, iocp_data* data) {
    iocp_datas* datas;
    int finish = 0;
    EnterCriticalSection(&svr->cs);
    datas = &svr->datas;
    do {
        int i;

        for(i=0; i<32; ++i) {
            if(datas->data[i] == data) {
                datas->data[i]->proc_close(data);
                datas->data[i] = NULL;
                finish = 1;
                break;
            }
        }
        if(finish == 1) break;

        datas = datas->next;

    } while(datas);
    LeaveCriticalSection(&svr->cs);

    return finish;
}

iocp_server* iocp_server_start() {
    DWORD dwCPU;
    SYSTEM_INFO systemInfo;
    iocp_server* svr;
    svr = (iocp_server*)iocp_malloc(sizeof(iocp_server));
    svr->iocp_register = iocp_register;
    svr->iocp_unregister = iocp_unregister;

    InitializeCriticalSection(&svr->cs);

    socket_init();

    svr->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    
    GetSystemInfo(&systemInfo);
    svr->thread_count = systemInfo.dwNumberOfProcessors * 2;

    if(svr->thread_count < 1) {
        svr->thread_count = 1;
    }

    if(svr->thread_count > MAX_WORKER_THREAD) {
        svr->thread_count = MAX_WORKER_THREAD;
    }
    for(dwCPU = 0; dwCPU < svr->thread_count; dwCPU++ ) {
        HANDLE thread = INVALID_HANDLE_VALUE;
        DWORD tid = 0;

        thread = CreateThread(NULL, 0, iocp_server_main, svr, 0, &tid);
        svr->thread_list[dwCPU] = thread;

    }

    return svr;

}

void iocp_server_stop(iocp_server* svr) {
    DWORD i;
    iocp_datas* datas;
    if( svr->iocp ) {
        EnterCriticalSection(&svr->cs);
        svr->opt_quit_flag = 1;
        LeaveCriticalSection(&svr->cs);
        for(i = 0; i < svr->thread_count; i++ )
            PostQueuedCompletionStatus(svr->iocp, 0, 0, NULL);
    }

    /* Make sure worker threads exits. */
    if( WAIT_OBJECT_0 != WaitForMultipleObjects(svr->thread_count,
                                                svr->thread_list,
                                                TRUE,
                                                1000) ) {
        printf("WaitForMultipleObjects() failed: %d\n", GetLastError());
    } else {
        for(i=0; i<svr->thread_count; i++ ) {
            if( svr->thread_list[i] != INVALID_HANDLE_VALUE )
                CloseHandle(svr->thread_list[i]);
            svr->thread_list[i] = INVALID_HANDLE_VALUE;
        }
    }

    if( svr->iocp ) {
        CloseHandle(svr->iocp);
        svr->iocp = NULL;
    }
    DeleteCriticalSection(&svr->cs);

     
    datas = &svr->datas;
    for(i=0; i<32; ++i) {
        if(datas->data[i] != NULL) {
            datas->data[i]->proc_close(datas->data[i]);
            datas->data[i] = NULL;
        }
    }

    datas = datas->next;
    while(datas != NULL) {
        for(i=0; i<32; ++i) {
            if(datas->data[i] != NULL) {
                datas->data[i]->proc_close(datas->data[i]);
                datas->data[i] = NULL;
            }
        }
        iocp_free(datas);
        datas = datas->next;
    }
    

    iocp_free(svr);
}

void iocp_server_join(iocp_server* svr) {
    WaitForMultipleObjects(svr->thread_count,
                                                svr->thread_list,
                                                TRUE,
                                                INFINITE);
}



DWORD WINAPI iocp_server_main ( LPVOID context ) {

    iocp_server* svr;

    BOOL state = FALSE;
    int ret;
    LPOVERLAPPED overlapped = NULL;
    iocp_data *data = NULL;
    DWORD io_size = 0;

    svr = (iocp_server*)context;

    while( svr->opt_quit_flag == 0 ) {


        /* continually loop to service io completion packets */
        state = GetQueuedCompletionStatus(svr->iocp, &io_size,
                                             (PDWORD_PTR)&data,
                                             &overlapped,
                                             100 /**< wait 100 mill-secs */
                                             );
        if( state == FALSE && overlapped == NULL ) {
            /** timeout */
            continue;
        }

        if( data == NULL ) {
            return(0);
        }
        if( state == FALSE) {
            printf("state false\n");
        }
        ret = data->proc_callback(data, overlapped, io_size);
        if(ret == -1) {
            iocp_unregister(svr, data);
        }
    } //while
    return(0);

}

void iocp_server_proc_close(iocp_server* svr, iocp_data* data) {
}
