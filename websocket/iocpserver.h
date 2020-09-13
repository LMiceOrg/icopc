#ifndef IOCPSERVER_H
#define IOCPSERVER_H

#include <mswsock.h>

#define iocp_malloc(s) HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(s))
#define iocp_free(p)   HeapFree(GetProcessHeap(),0,(p))

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WORKER_THREAD   32


typedef enum _IO_OPERATION {
    ClientIoAccept,
    ClientIoRead,
    ClientIoWrite
} IO_OPERATION, *PIO_OPERATION;

//
// data to be associated for every I/O operation on a socket
//

typedef struct _iocp_data {
  int type;
  SOCKET fd; /**< file descriptor or signal or process id */
  int (*proc_callback)(void* pdata, LPOVERLAPPED over, DWORD io_len);
  void (*proc_close)(void* ptr);

} iocp_data;

typedef struct _iocp_data_list {
  iocp_data* data[32];
  struct _iocp_data_list* next;
}iocp_datas;


typedef struct iocp_server {
    HANDLE iocp;
    DWORD thread_count;
    HANDLE thread_list[MAX_WORKER_THREAD];
    BOOL g_bEndServer;
    int opt_quit_flag;
    CRITICAL_SECTION cs;
    iocp_datas datas;
    int (*iocp_register)(struct iocp_server* svr, iocp_data* data);
    int (*iocp_unregister)(struct iocp_server* svr, iocp_data* data);
} iocp_server;

iocp_server* iocp_server_start();

void iocp_server_stop(iocp_server* svr);

void iocp_server_join(iocp_server* svr);

#ifdef __cplusplus
}
#endif

#endif // IOCPSERVER_H
