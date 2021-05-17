/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */

#ifndef IOCPSERVER_H
#define IOCPSERVER_H

#include <mswsock.h>
#ifdef _DEBUG
static void *_iocp_malloc_debug(int s) {
  void *p = malloc(s);
  memset(p, 0, s);
  return p;
}

#define iocp_malloc(s) _iocp_malloc_debug(s)
#define iocp_free(p) free(p)
#else
#define iocp_malloc(s) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (s))
#define iocp_free(p) HeapFree(GetProcessHeap(), 0, (p))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WORKER_THREAD 32


/* data to be associated for every I/O operation on a socket */
typedef struct _iocp_data {
  int type;
  SOCKET fd; /**< file descriptor or signal or process id */
  int (*proc_callback)(void *pdata, LPOVERLAPPED over, DWORD io_len);
  void (*proc_close)(void *ptr);
  void (*proc_timeout)(void *ptr);
} iocp_data;

typedef struct _iocp_data_list {
  iocp_data *data[32];
  struct _iocp_data_list *next;
} iocp_datas;

typedef void(__cdecl *iocp_timeout)(void *);

typedef struct iocp_server {
  HANDLE iocp;
  DWORD thread_count;
  HANDLE thread_list[MAX_WORKER_THREAD];
  BOOL g_bEndServer;
  int opt_quit_flag;
  CRITICAL_SECTION cs;
  iocp_datas datas;
  DWORD timeout_period;
  int (*iocp_register)(struct iocp_server *svr, iocp_data *data);
  int (*iocp_unregister)(struct iocp_server *svr, iocp_data *data);
  iocp_timeout timeout_proc;
} iocp_server;

iocp_server *iocp_server_start(DWORD period);

void iocp_server_stop(iocp_server *svr);

void iocp_server_join(iocp_server *svr);

#ifdef __cplusplus
}
#endif

#endif  // IOCPSERVER_H
