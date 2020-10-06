#ifndef WSSERVER_H
#define WSSERVER_H


#if _MSC_VER == 1200
#ifndef uint8_t
#define uint8_t unsigned char
#define uint16_t unsigned short
#define uint32_t unsigned int
#define uint64_t unsigned __int64

#define int8_t char
#define int16_t short
#define int32_t int
#define int64_t __int64
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

typedef struct _websocket1 {
  uint8_t opcode : 4;
  uint8_t rsv : 3;
  uint8_t fin : 1;

  uint8_t len : 7;
  uint8_t mask : 1;
} websocket1;

typedef struct _websocket2 {
  uint8_t opcode : 4;
  uint8_t rsv : 3;
  uint8_t fin : 1;

  uint8_t len : 7;
  uint8_t mask : 1;

  uint16_t len2;
} websocket2;

typedef struct _websocket3 {
  uint8_t opcode : 4;
  uint8_t rsv : 3;
  uint8_t fin : 1;

  uint8_t len : 7;
  uint8_t mask : 1;

  union {
    uint64_t len3;
    struct {
      uint32_t len3_high;
      uint32_t len3_low;
    };
  };
} websocket3;

/** websocket header
 * @struct wshdr
 * @brief websocket header
 */
typedef struct _websocket_header {
  union {
    websocket3 ws3;
    struct {
      char padding6[6];
      websocket2 ws2;
    };
    struct {
      char padding8[8];
      websocket1 ws1;
    };
  };
} wshdr;

/** webspcket packet
 * @struct wspkt
 * @brief websocket packet
 */
typedef struct _websocket_packet {
  wshdr hdr;
  char data[8];
} wspkt;

#define ws1pkt(pkt) (&(pkt)->hdr.ws1)
#define ws2pkt(pkt) (&(pkt)->hdr.ws2)
#define ws3pkt(pkt) (&(pkt)->hdr.ws3)

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
  WS_IO_ACCEPT,
  WS_IO_SEND,
  WS_IO_RECV,
  WS_IO_SENDING,
  WS_IO_RECVING
};

#ifndef WS_IO_CONTEXT
#define WS_IO_CONTEXT
typedef struct _ws_io_context {
  WSAOVERLAPPED overlapped;
  WSABUF wsabuf;
  int op;
  int data_len;
  int payload_len;
  int payload_pos;
  char data[65536];
} ws_io_context;
#endif

/**
 * @fn ws_timeout_callback
 * @brief Timeout callback
 * @param[in]pdata: IOCP data
 * @return None
 */
typedef void (*ws_timeout_callback)(void *);

typedef void (*ws_user_close)(void *);

typedef int (*ws_data_callback)(ws_io_context *, void *, char **, int *);
typedef int (*ws_accept_callback)(const struct sockaddr_in *,
                                  ws_data_callback *, void **, ws_user_close *,
                                  ws_timeout_callback *);

typedef struct {
  int type;
  SOCKET fd; /**< accept socket */
  int (*proc_callback)(void *pdata, LPOVERLAPPED over, DWORD io_len);
  void (*proc_close)(void *ptr);
  void (*proc_timeout)(void *ptr);

  void *server;
  int status;
  DWORD tick;

  ws_io_context recv_ctx;
  ws_io_context send_ctx;

  void *user; /**< 用户定义数据 */
  ws_data_callback data_callback;
  ws_user_close user_close;
  ws_timeout_callback timeout_callback;

} ws_client_data;

typedef struct _ws_accept_data {
  int type;
  SOCKET fd; /**< listen socket */

  /** iocp call back*/
  int (*proc_callback)(void *pdata, LPOVERLAPPED over, DWORD io_len);
  void (*proc_close)(void *ptr);
  void (*proc_timeout)(void *ptr);

  void *server;

  WSAOVERLAPPED overlapped;
  int op;
  char data[MAX_BUFF_SIZE];
  WSABUF wsabuf;

  LPFN_ACCEPTEX proc_accept;
  LPFN_GETACCEPTEXSOCKADDRS proc_getsockaddrs;

  SOCKET accept_fd; /**< accept socket */

  ws_accept_callback accept_callback; /**< 连接请求处理回调函数 */

} ws_accept_data;

/**
 * @fn wsserver_start
 * @brief register websocket server
 * @param[in] ptr iocp server
 * @param[in] acb accept callback
 * @retval 0 Success
 */
int wsserver_start(void *ptr, ws_accept_callback acb);

/**
 * @fn ws_send
 * @brief send websocket packet
 * @param[in] client
 * @param[int] buf
 * @param[int] nbytes
 * @return 0 Success
 */
int ws_send(ws_client_data *client, const char *buf, int nbytes);

/**
 * @fn ws_send_data
 * @brief send websocket data
 * @param[in] client
 * @param[in] buf
 * @param[in] nbytes
 * @return 0 Success
 */
int ws_send_data(ws_client_data *client, const char *buf, int nbytes);

int ws_send_packet(ws_client_data *client, wspkt* pkt, int nbytes);

#ifdef __cplusplus
}
#endif

#endif /* WSSERVER_H */
