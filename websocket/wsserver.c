/** Copyright 2018, 2019 He Hao<hehaoslj@sina.com> */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Ws2tcpip.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "base64.h"
#include "iocpserver.h"
#include "sha1.h"
#include "wsserver.h"

#define MAX_LISTEN_SIZE 4
#define WS_LEN2 126
#define WS_LEN3 127

#define WS_HANDSHAKE_0                   \
  "HTTP/1.1 101 Switching Protocols\r\n" \
  "Upgrade: websocket\r\n"               \
  "Connection: upgrade\r\n"              \
  "Sec-WebSocket-Accept: "

#define WS_HANDSHAKE_1 "\r\n\r\n"

int opt_websocket_port = 12311;
#define OPT_WEBSOCKET_PORT "12311"




int ws_accept_proc(void *ptr, LPOVERLAPPED over, DWORD io_size);
void ws_accept_close(void *ptr);

int ws_client_proc(void *ptr, LPOVERLAPPED over, DWORD io_size);
void ws_client_close(void *ptr);

int ws_io_recv(ws_client_data *client, DWORD io_size);
int ws_io_send(ws_client_data *client, DWORD io_size);

int ws_send(ws_client_data *client, const char *buf, int nbytes);
int ws_recv(ws_client_data *client);

int ws_accept(ws_accept_data* data);

int ws_handshake(ws_client_data *data);
int ws_proc(ws_client_data *data);
void ws_sec_key(const unsigned char *key, int klen, char *buf, int blen);

int ws_send_pong(ws_client_data *client);
int ws_send_data(ws_client_data *client, const char *buf, int nbytes);
int ws_proc_data(ws_io_context *data, char **sendbuf, int* size);

static __inline void ws_io_purge(ws_io_context* ctx, int pos);

/** helper */
static __inline int get_request_len(char* buf, int len) {
    int i;
    int pos = -1;
    unsigned int request_end = 0x0a0d0a0d;
    for(i = 0; i < len - 3; ++i) {
        unsigned int req=*((unsigned int*)(buf+i));
        if(req == request_end) {
            pos = i + 4;
            break;
        }
    }
    return pos;
}

static __inline int get_line_end(char *buf, int len) {
  int i;
  int pos = -1;
  for (i = 0; i < len - 1; ++i) {
    if (buf[i] == '\r' && buf[i + 1] == '\n') {
      pos = i + 1;
      break;
    }
  }
  return pos;
}

static __inline int get_space(char *buf, int len) {
  int i;
  int pos = -1;
  for (i = 0; i < len - 1; ++i) {
    if (buf[i] == ' ') {
      pos = i;
      break;
    }
  }
  return pos;
}

/** 创建SOCKET 设置参数 */
static __inline SOCKET socket_create(void) {
    int nRet = 0;
    int nZero = 0;
    SOCKET sdSocket = INVALID_SOCKET;

    sdSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if( sdSocket == INVALID_SOCKET ) {
        printf("WSASocket(sdSocket) failed: %d\n", WSAGetLastError());
        return(sdSocket);
    }

    /* Disable send buffering on the socket.*/
    nZero = 0;
    nRet = setsockopt(sdSocket, SOL_SOCKET, SO_SNDBUF, (char *)&nZero, sizeof(nZero));
    if( nRet == SOCKET_ERROR) {
        printf("setsockopt(SNDBUF) failed: %d\n", WSAGetLastError());
        /* return(sdSocket); */
    }

    return(sdSocket);

}

int wsserver_start(void *ptr, ws_data_callback cb) {
  int ret;
  SOCKET listenfd;
  HANDLE iocp;
  struct sockaddr_in server_addr;
  ws_accept_data *data;
  iocp_server *svr = (iocp_server *)ptr;
  DWORD bytes=0;
  GUID acceptex_guid = WSAID_ACCEPTEX;

  /* Resolve the interface */
  /*
  struct addrinfo hints;
  struct addrinfo *addrlocal = NULL;

  
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags  = AI_PASSIVE;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_IP;

  if( getaddrinfo(NULL, OPT_WEBSOCKET_PORT, &hints, &addrlocal) != 0 ) {
      printf("getaddrinfo() failed with error %d\n", WSAGetLastError());
      return(-1);
  }
  

  if( addrlocal == NULL ) {
      printf("getaddrinfo() failed to resolve/convert the interface\n");
      return(-1);
  }
  */


  /* create listen socket */
  listenfd = socket_create();
  if (listenfd == INVALID_SOCKET) return -1;

  memset(&server_addr, 0, sizeof(struct sockaddr_in));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(opt_websocket_port);



  ret = bind(listenfd, (struct sockaddr *)(&server_addr), sizeof(server_addr));
  /*ret = bind(listenfd, addrlocal->ai_addr, addrlocal->ai_addrlen); */
  if( ret == SOCKET_ERROR) {
      printf("bind() failed: %d\n", WSAGetLastError());
      /*freeaddrinfo(addrlocal);*/
      return(-1);
  }

  ret = listen(listenfd, SOMAXCONN);
  if( ret == SOCKET_ERROR ) {
      printf("listen() failed: %d\n", WSAGetLastError());
      /*freeaddrinfo(addrlocal);*/
      return(-1);
  }
  /*freeaddrinfo(addrlocal);*/

 
  

  /* init data */
  data = (ws_accept_data *)iocp_malloc(sizeof(ws_accept_data));
  //memset(data, 0, sizeof(ws_accept_data));
  data->type = WS_TYPE_ACCEPT;
  data->fd = listenfd;
  data->proc_callback = ws_accept_proc;
  data->proc_close = ws_accept_close;
  data->server = svr;
  data->op = ClientIoAccept;
  data->wsabuf.buf  = data->data;
  data->wsabuf.len  = sizeof(data->data);
  data->accept_fd = INVALID_SOCKET;
  data->data_callback = ws_proc_data;
  if(cb)
      data->data_callback = cb;


  /* update iocp */
  iocp = CreateIoCompletionPort((HANDLE)listenfd, svr->iocp, (DWORD_PTR)data, 0);
  if(iocp == NULL) {
      printf("CreateIoCompletionPort() failed: %d\n", GetLastError());
      return(-1);
  }

  /* Load the AcceptEx extension function from the provider for this socket */
  ret = WSAIoctl(
      listenfd,
      SIO_GET_EXTENSION_FUNCTION_POINTER,
     &acceptex_guid,
      sizeof(acceptex_guid),
     &data->proc_accept,
      sizeof(data->proc_accept),
     &bytes,
      NULL,
      NULL
      );
  if (ret == SOCKET_ERROR)
  {
      printf("failed to load AcceptEx: %d\n", WSAGetLastError());
      return (-1);
  }

  ret = ws_accept(data);
  if(ret != 0) return ret;

  /* register */
  svr->iocp_register(svr, data);
  printf("fd %d accept fd %d\n", data->fd, data->accept_fd);

  return listenfd;
}

int ws_accept(ws_accept_data* data) {
    DWORD bytes = 0;
    int ret = 0;

    data->accept_fd = socket_create();
    if( data->accept_fd == INVALID_SOCKET) {
        printf("failed to create new accept socket\n");
        return(-1);
    }

    /* pay close attention to these parameters and buffer lengths */
    ret = data->proc_accept(data->fd, data->accept_fd,
                    (LPVOID)(data->data),
                    sizeof(data->data) - (2 * (sizeof(SOCKADDR_STORAGE) + 16)),
                    sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16,
                    &bytes,
                    (LPOVERLAPPED) &(data->overlapped));
    if( ret == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
        printf("AcceptEx() failed: %d\n", WSAGetLastError());
        return(-1);
    }
    return 0;
}

int ws_accept_proc(void* pdata, LPOVERLAPPED over, DWORD io_size) {
  ws_accept_data *data = (ws_accept_data *)(pdata);
  int ret = 0;
  ws_client_data *client_data = NULL;
  ws_io_context *ctx;
  DWORD bytes;

  iocp_server *svr = (iocp_server *)data->server;

  ret = setsockopt(
              data->accept_fd,
              SOL_SOCKET,
              SO_UPDATE_ACCEPT_CONTEXT,
              (char *)&data->fd,
              sizeof(data->fd)
              );
  if( ret == SOCKET_ERROR ) {
      printf("setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed to update accept socket\n");
      return(0);
  }

  /* init client data */
  client_data = (ws_client_data*)iocp_malloc(sizeof(ws_client_data));
  memset(client_data, 0, sizeof(ws_client_data));
  client_data->type = WS_TYPE_CLIENT;
  client_data->fd = data->accept_fd;
  client_data->server = svr;
  client_data->proc_callback = ws_client_proc;
  client_data->proc_close = ws_client_close;
  client_data->data_callback = data->data_callback;

  ctx = &client_data->recv_ctx;
  ctx->op = WS_IO_RECV;
  ctx->data_len = 0;

  ctx = &client_data->send_ctx;
  ctx->op = WS_IO_SEND;
  ctx->data_len = 0;

  /* renew accept fd */
  ret = ws_accept(data);
  printf("connect fd %d, renew fd %d accept fd %d\n",client_data->fd, data->fd, data->accept_fd);
  if( ret != 0) {
      printf("failed to create new accept socket\n");
      iocp_free(client_data);
      return(-1);
  }

  /* pay close attention to these parameters and buffer lengths */
  bytes = 0;
  ret = data->proc_accept(data->fd, data->accept_fd,
                  (LPVOID)(data->data),
                  sizeof(data->data) - (2 * (sizeof(SOCKADDR_STORAGE) + 16)),
                  sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16,
                  &bytes,
                  (LPOVERLAPPED) &(data->overlapped));
  if( ret == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
      printf("AcceptEx() failed: %d\n", WSAGetLastError());
      return(-1);
  }


  /* update iocp */
  svr->iocp = CreateIoCompletionPort((HANDLE)client_data->fd, svr->iocp, (DWORD_PTR)client_data, 0);
  if(svr->iocp == NULL) {
      printf("CreateIoCompletionPort() failed: %d\n", GetLastError());
      iocp_free(client_data);
      return(-1);
  }

  /* iocp register */
  svr->iocp_register(svr, client_data);


  if( io_size ) {
      /* move data from accept to client */
      ws_io_context* ctx = &client_data->recv_ctx;
      memcpy(ctx->data + ctx->data_len, data->data, io_size);
      ctx->op = WS_IO_RECV;

      /* proc data */
      return ws_client_proc(client_data, &ctx->overlapped, io_size);

  } else {

      DWORD nRet;
      WSABUF buffRecv;
      DWORD dwRecvNumBytes = 0;
      DWORD dwFlags = 0;
      ws_io_context* ctx = &client_data->recv_ctx;

      ctx->op = WS_IO_RECVING;

      buffRecv.buf = ctx->data + ctx->data_len;
      buffRecv.len = MAX_BUFF_SIZE - ctx->data_len;

      nRet = WSARecv(
                  data->fd,
                  &buffRecv,
                  1,
                  &dwRecvNumBytes,
                  &dwFlags,
                  &ctx->overlapped, NULL);
      if( nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
          printf ("WSARecv() failed: %d\n", WSAGetLastError());
          return(-1);
      }
  }
  

  return(0);
}
void ws_accept_close(void *ptr) {
    ws_accept_data* data =(ws_accept_data*)ptr;
    LINGER  lingerStruct;

    /* force the subsequent closesocket to be abortative. */
    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(data->fd, SOL_SOCKET, SO_LINGER,
                       (char *)&lingerStruct, sizeof(lingerStruct) );

    closesocket(data->fd);
    closesocket(data->accept_fd);
    printf("ws accept_close %d %d \n", data->fd, data->accept_fd);
    iocp_free(ptr);

    

}

int ws_client_proc(void *ptr, LPOVERLAPPED over, DWORD io_size) {
  int ret;
  ws_client_data *client = (ws_client_data *)(ptr);
  ws_io_context *ctx = (ws_io_context*)over;
  if(ctx->op == WS_IO_SENDING || ctx->op == WS_IO_SEND) {
      ret = ws_io_send(client, io_size);
  } else if(ctx->op == WS_IO_RECVING || ctx->op == WS_IO_RECV) {
    ret = ws_io_recv(client, io_size);
  }
  return ret;
}
void ws_client_close(void *ptr) {
    ws_client_data* data = (ws_client_data*)ptr;
    LINGER  lingerStruct;

    /* force the subsequent closesocket to be abortative. */
    lingerStruct.l_onoff = 1;
    lingerStruct.l_linger = 0;
    setsockopt(data->fd, SOL_SOCKET, SO_LINGER,
                       (char *)&lingerStruct, sizeof(lingerStruct) );

    closesocket(data->fd);
    

    printf("ws client_close %d\n", data->fd);
    iocp_free(ptr);
}

int ws_io_recv(ws_client_data *client, DWORD io_size) {
    ws_io_context *ctx;

    if (io_size == 0) {
        printf("recv failed\r\n");
        return(-1);
    }

    ctx = (ws_io_context*)&client->recv_ctx;
    ctx->op = WS_IO_RECV;
    ctx->data_len += io_size;
    ctx->data[ctx->data_len] = '\0';
    ctx->payload_len = 0;
    ctx->payload_pos = 0;
    if (client->status == WS_STATUS_0)
        return ws_handshake(client);
    else
        return ws_proc(client);
}

int ws_io_send(ws_client_data *client, DWORD io_size){
    ws_io_context *ctx;
    DWORD nRet;
    DWORD dwSendNumBytes = 0;
    DWORD dwFlags = 0;

    if(io_size == 0) {
        return(-1);
    }
    ctx = (ws_io_context*)&client->send_ctx;

    /* remove old message */
    ws_io_purge(ctx, io_size);

    if(ctx->data_len) {

      ctx->op = WS_IO_SENDING;
      ctx->wsabuf.buf = ctx->data;
      ctx->wsabuf.len = ctx->data_len;

      nRet = WSASend (
                  client->fd,
                  &ctx->wsabuf,
                  1,
                  &dwSendNumBytes,
                  dwFlags,
                  &(ctx->overlapped), NULL);
      if( nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
          printf ("WSASend() failed: %d\n", WSAGetLastError());
          return(-1);
      }
    } else {
        ctx->op = WS_IO_SEND;
    }
    return 0;

}

int ws_send(ws_client_data *client, const char *buf, int nbytes) {
  ws_io_context* ctx = &client->send_ctx;

  /* copy data to sending buffer */
  memcpy(ctx->data+ctx->data_len, buf, nbytes);
  ctx->data_len += nbytes;

  if(ctx->op == WS_IO_SEND) {
      DWORD nRet;
      WSABUF buffSend;
      DWORD dwSendNumBytes = 0;
      DWORD dwFlags = 0;

      ctx->op = WS_IO_SENDING;

      buffSend.buf = ctx->data;
      buffSend.len = ctx->data_len;
      nRet = WSASend (
                  client->fd,
                  &buffSend,
                  1,
                  &dwSendNumBytes,
                  dwFlags,
                  &(ctx->overlapped), NULL);
      if( nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
          printf ("WSASend() failed: %d\n", WSAGetLastError());
          return(-1);
      }
  }

  return 0;

}

int ws_recv(ws_client_data *client) {
    ws_io_context* ctx = &client->recv_ctx;

    if(ctx->op = WS_IO_RECV) {
        DWORD nRet;
        WSABUF buffRecv;
        DWORD dwRecvNumBytes = 0;
        DWORD dwFlags = 0;

        ctx->op = WS_IO_RECVING;

        buffRecv.buf = ctx->data + ctx->data_len;
        buffRecv.len = MAX_BUFF_SIZE - ctx->data_len;

        nRet = WSARecv(
                    client->fd,
                    &buffRecv,
                    1,
                    &dwRecvNumBytes,
                    &dwFlags,
                    &ctx->overlapped, NULL);
        if( nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError()) ) {
            printf ("WSARecv() failed: %d\n", WSAGetLastError());
            return(-1);
        }
    }
    return 0;
}

int ws_handshake(ws_client_data *data) {
  int line_end = 0;
  int line_begin = 0;
  int line_len = 0;
  int pos = 0;
  int remain_bytes = data->recv_ctx.data_len;
  char* buff;
  char *line;

  const char sec_websocket_key[] = "Sec-WebSocket-Key";
  unsigned char *sec_value = NULL;
  int sec_value_len = 0;

  char *response;
  int response_len;
  int request_len = 0;
  int ret;

  buff = data->recv_ctx.data;

  /* check request state, wait for incoming data */
  request_len = get_request_len(buff, remain_bytes);
  if( request_len < 0)
      return(0);

  // first line GET /address HTTP/1.1\r\n
  line_end = get_line_end(buff + line_begin, remain_bytes);
  if (line_end < 4) return -1;
  remain_bytes -= line_end;
  line_len = line_end - 1;
  line = buff + line_begin;

  if (strncmp(buff, "GET", 3) != 0) return -1;
  buff[line_end] = '\0';

  // header
  do {
    char *key;
    char *value;
    if (remain_bytes <= 0) break;

    line_begin += line_end + 1;
    line_end = get_line_end(buff + line_begin, remain_bytes);
    remain_bytes -= line_end;

    if (line_end < 2) break;
    line_len = line_end - 1;
    line = buff + line_begin;

    line[line_end] = '\0';
    // printf("header %d %s\n", line_len, line);

    // header key: value
    pos = get_space(line, line_len);
    key = line;
    value = line + pos + 1;
    if (memcmp(key, sec_websocket_key, sizeof(sec_websocket_key) - 1) == 0) {
      sec_value = (unsigned char *)value;
      sec_value_len = line_len - pos - 1;
      // printf("got sec_value, %d %s\n", sec_value_len, sec_value);
      /** only check sec_websocket_key */
      break;
    }

  } while (1);

  if (sec_value == NULL) return(-1);



  //  response_len = snprintf(response, 512,
  //                          "HTTP/1.1 101 Switching Protocols\r\n"
  //                          "Upgrade: websocket\r\n"
  //                          "Connection: upgrade\r\n"
  //                          "Sec-WebSocket-Accept: %s\r\n\r\n",
  //                          data->data + 512);

  /* send handshake response */
  {
      char buf[sizeof(WS_HANDSHAKE_0) + 28 + sizeof(WS_HANDSHAKE_1)+1];
      ws_io_context* ctx = &data->send_ctx;

      response_len = sizeof(WS_HANDSHAKE_0) -1 + 28 + sizeof(WS_HANDSHAKE_1)-1;
      response = buf;
      memcpy(response, WS_HANDSHAKE_0, sizeof(WS_HANDSHAKE_0)-1);
      ws_sec_key(sec_value, sec_value_len, response + sizeof(WS_HANDSHAKE_0)-1, 28);
      memcpy(response + sizeof(WS_HANDSHAKE_0)-1 + 28, WS_HANDSHAKE_1,
             sizeof(WS_HANDSHAKE_1)-1);

      response[response_len]='\0';


      ret = ws_send(data, response, response_len);
      printf("%s\n", response);
      printf("%s  %d\n", WS_HANDSHAKE_1, sizeof(WS_HANDSHAKE_1));
  }



  {
      ws_io_context* ctx = &data->recv_ctx;
      /* remove old message */
      ws_io_purge(ctx, request_len);

      /* recv data */
      ret = ws_recv(data);
  }

  // printf("response\n%s\n", response);
  data->status = WS_STATUS_DATA;

  return ret;
}

int ws_proc(ws_client_data *client) {
    ws_io_context * ctx;
    websocket1 *ws;
    char* data;
    char send_buf[128];
    int send_size = 128;
    int ret= 0;


    char *payload = NULL;
    char *mask_key = NULL;

    int pos = 0;
    int64_t payload_len = 0;
    int data_len = 0;
    // printf("%d%d\n", *(uint8_t *)(data->data), *(uint8_t *)(data->data + 1));
    // printf("ws: fin:%d rsv:%d opcode:%d mask:%d len:%d", ws->fin, ws->rsv,
    //      ws->opcode, ws->mask, ws->len);

    ctx = (ws_io_context*)&client->recv_ctx;
    data = ctx->data;
    ws = (websocket1 *)(data);

    data_len = ctx->data_len;

    // get payload len
    if (ws->len < WS_LEN2) {
        pos += 2;
        payload_len = ws->len;
    } else if (ws->len == WS_LEN2) {
        pos += 2 + 2;
        payload_len = *(int16_t *)(data + 2);
    } else if (ws->len == WS_LEN3) {
        pos += 2 + 8;
        payload_len = *(int64_t *)(data + 2);
    }

    // get mask
    if (ws->mask == 1) {
        mask_key = data + pos;
        pos += 4;

    }

    // check length
    if (data_len < payload_len + pos) {
        return(-1);
    };

    // unmask payload
    payload = data + pos;
    if (ws->mask == 1) {
        // unmask
        int i;
        for (i = 0; i < payload_len; ++i) {
            payload[i] = payload[i] ^ mask_key[i % 4];
        }
    }

    ctx->payload_len = payload_len;
    ctx->payload_pos = pos;


    // printf("opcode %d\n", ws->opcode);

    // switch opcode
    switch (ws->opcode) {
    case WS_PING:
        // send pong
        ws_send_pong(client);
        break;
    case WS_PONG:
        // do nothing
        break;
    case WS_CLOSE:
        // close socket
        client->status = WS_STATUS_CLOSE;
        return(-1);
        break;
    case WS_DATA1:
    case WS_DATA2:
        // ws data
        {
            char* pbuf = send_buf;
            send_size = sizeof(send_buf);

            ret = client->data_callback(&client->recv_ctx, &pbuf, &send_size);
            if(ret ==1) {
                iocp_server* svr = (iocp_server*)client->server;
                svr->opt_quit_flag = 1;
                return 0;
            } else if(ret!= 0) {
                return(ret);
            } else {
                if(send_size > 0) {
                    ret = ws_send_data(client, pbuf, send_size);
                }
                if(send_size > 128) {
                    iocp_free(pbuf);
                }
            }
        }
        break;
    }

    /* remove old message */
      ws_io_purge(ctx, payload_len + pos);



    /* recv data */
    ret = ws_recv(client);


    return ret;
}

int ws_proc_data(ws_io_context *ctx, char **sendbuf, int* size) {
  const char *message_ =
      "{"
      "\"command\":\"test\""
      "}";

  int ret = 0;



  printf("proc data[%d] %s\n", ctx->payload_len,
         ctx->data + ctx->payload_pos);
  if (ctx->payload_len >= 4) {
    if (strncmp(ctx->data + ctx->payload_pos, "quit", 4) == 0) {
        return(1);
    }
  }
  // printf("json %d %u\n", ret, pos_);

  *sendbuf = (char*)iocp_malloc( strlen(message_));
  memcpy(*sendbuf, message_, strlen(message_));
  *size = strlen(message_);
  //ret = ws_send_data(client, message_, strlen(message_) );
  // printf("send data %lu %d\r\n", strlen(message_), ret);

  return ret;
}

int ws_send_pong(ws_client_data* client) {
  char buf[2];
  websocket1 *ws = (websocket1 *)(buf);
  ws->fin = 1;
  ws->len = 0;
  ws->mask = 0;
  ws->opcode = WS_PONG;
  ws->rsv = 0;
  // send pong
  return ws_send(client, buf, 2);
}

int ws_send_data(ws_client_data* client, const char *data, int nbytes) {
  int ret = 0;
  if (nbytes < WS_LEN2) {
    websocket1 *ws;
    char *buf = (char *)malloc(nbytes + 2);
    memset(buf, 0, nbytes + 2);
    ws = (websocket1 *)(buf);
    ws->fin = 1;
    ws->len = nbytes;
    ws->mask = 0;
    ws->opcode = WS_DATA1;
    ws->rsv = 0;
    memcpy(buf + 2, data, nbytes);
    ret = ws_send(client, buf, nbytes + 2);
    free(buf);
  } else if (nbytes < 65535) {
    websocket2 *ws;
    char *buf = (char *)malloc(nbytes + 4);
    memset(buf, 0, nbytes + 4);
    ws = (websocket2 *)(buf);
    ws->fin = 1;
    ws->len = WS_LEN2;
    ws->len2 = htons(nbytes);
    ws->mask = 0;
    ws->opcode = WS_DATA1;
    ws->rsv = 0;
    memcpy(buf + 4, data, nbytes);
    ret = ws_send(client, buf, nbytes + 4);
    free(buf);
  } else {
    websocket3 *ws;
    char *buf = (char *)malloc(nbytes + 10);
    memset(buf, 0, nbytes + 10);
    ws = (websocket3 *)(buf);
    ws->fin = 1;
    ws->len = WS_LEN3;
    ws->len3 = htonl(nbytes);
    ws->mask = 0;
    ws->opcode = WS_DATA1;
    ws->rsv = 0;
    memcpy(buf + 10, data, nbytes);
    ret = ws_send(client, buf, nbytes + 10);
    free(buf);
  }
  return ret;
}

void ws_sec_key(const unsigned char *key, int klen, char *buf, int blen) {
  unsigned char sha1_key[24] = {0};

  const unsigned char ws_magic_key[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  SHA1_CTX ctx;

  SHA1Init(&ctx);
  SHA1Update(&ctx, key, klen);
  SHA1Update(&ctx, ws_magic_key, sizeof(ws_magic_key) - 1);
  SHA1Final(sha1_key, &ctx);

  base64_encrypt_text2(sha1_key, 20, buf, blen);
}

void ws_io_purge(ws_io_context* ctx, int pos) {
    int remain = ctx->data_len - pos;
    if(remain > 0) {
        memcpy(ctx->data, ctx->data + pos, remain);
    }
    ctx->data_len = remain;
}

