#ifndef WSSERVER_H
#define WSSERVER_H

#if _MSC_VER == 1200
    #ifndef uint8_t
    #define uint8_t  unsigned char 
    #define uint16_t unsigned short 
    #define uint32_t unsigned int 
    #define uint64_t unsigned __int64 

    #define   int8_t char
    #define  int16_t short
    #define  int32_t int
    #define  int64_t __int64
    #endif
#else
#include <stdint.h>
#endif

#ifdef _WIN32
  #include "iocpserver.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push)
#pragma pack(1)

typedef struct {
  uint8_t opcode : 4;
  uint8_t rsv : 3;
  uint8_t fin : 1;

  uint8_t len : 7;
  uint8_t mask : 1;
} websocket1;

typedef struct {
  uint8_t opcode : 4;
  uint8_t rsv : 3;
  uint8_t fin : 1;

  uint8_t len : 7;
  uint8_t mask : 1;

  uint16_t len2;
} websocket2;

typedef struct {
  uint8_t opcode : 4;
  uint8_t rsv : 3;
  uint8_t fin : 1;

  uint8_t len : 7;
  uint8_t mask : 1;

  uint64_t len3;
} websocket3;

#pragma pack(pop)


#define MAX_BUFF_SIZE 65536


enum websocket_opcode {
  WS_DATA1 = 1,
  WS_DATA2 = 2,
  WS_CLOSE = 8,
  WS_PING = 9,
  WS_PONG = 10
};



enum websocket_status {
  WS_TYPE_ACCEPT = 0x010B,
  WS_TYPE_CLIENT = 0x010A,
  WS_STATUS_0 = 0,
  WS_STATUS_DATA = 1,
  WS_STATUS_CLOSE = 2,
    WS_IO_SEND,
    WS_IO_RECV,
    WS_IO_SENDING,
    WS_IO_RECVING
};


#ifndef WS_IO_CONTEXT
#define WS_IO_CONTEXT
typedef struct _ws_io_context {
    WSAOVERLAPPED               overlapped;
    WSABUF                      wsabuf;
    int op;
    int data_len;
    int payload_len;
    int payload_pos;
    char data[65536];
} ws_io_context;
#endif

typedef int (*ws_data_callback)(ws_io_context *, char **, int* );

typedef struct {
  int type;
  SOCKET fd;   /**< accept socket */
  int (*proc_callback)(void* pdata, LPOVERLAPPED over, DWORD io_len);
  void (*proc_close)(void *ptr);

  void *server;
  int status;

  ws_io_context recv_ctx;
  ws_io_context send_ctx;

  ws_data_callback data_callback;

} ws_client_data;




typedef struct _ws_accept_data {
    int type;
    SOCKET fd; /**< listen socket */

    int (*proc_callback)(void* pdata, LPOVERLAPPED over, DWORD io_len);
    void (*proc_close)(void* ptr);

    void *server;

    WSAOVERLAPPED               overlapped;
    IO_OPERATION op;
    char data[MAX_BUFF_SIZE];
    WSABUF                      wsabuf;

    LPFN_ACCEPTEX proc_accept;
    SOCKET accept_fd; /**< accept socket */
    ws_data_callback data_callback;

} ws_accept_data;

int wsserver_start(void *ptr, ws_data_callback cb);


#ifdef __cplusplus
}
#endif

#endif  /* WSSERVER_H */
