/** Copyright 2018, 2019 He Hao<hehaoslj@sina.com> */
#define WIN32_LEAN_AND_MEAN
#include "wsserver.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#include "base64.h"
#include "iocpserver.h"
#include "sha1.h"

#define MAX_LISTEN_SIZE 4
#define WS_LEN2 126
#define WS_LEN3 127

#define WS_HANDSHAKE_0                     \
    "HTTP/1.1 101 Switching Protocols\r\n" \
    "Upgrade: websocket\r\n"               \
    "Connection: upgrade\r\n"              \
    "Sec-WebSocket-Accept: "

#define WS_HANDSHAKE_1 "\r\n\r\n"

int opt_websocket_port = 12311;
#define OPT_WEBSOCKET_PORT "12311"

int  ws_accept_proc(void *ptr, LPOVERLAPPED over, DWORD io_size);
void ws_accept_close(void *ptr);
void ws_accept_timeout(void *ptr);

int  ws_client_proc(void *ptr, LPOVERLAPPED over, DWORD io_size);
void ws_client_close(void *ptr);
void ws_client_timeout(void *ptr);

int ws_io_recv(ws_client_data *client, DWORD io_size);
int ws_io_send(ws_client_data *client, DWORD io_size);

int ws_recv(ws_client_data *client);

int ws_accept(ws_accept_data *data);

int  ws_handshake(ws_client_data *data);
int  ws_proc(ws_client_data *data);
void ws_sec_key(const unsigned char *key, int klen, char *buf, int blen);

int ws_send_pong(ws_client_data *client);

/* default data callback proc */
int ws_proc_data(ws_io_context *, void *, char **, int *);

/* default accept callback proc */
int ws_proc_accept(const struct sockaddr_in *, ws_data_callback *, void **, ws_user_close *, ws_timeout_callback *);

static __inline void ws_io_purge(ws_io_context *ctx, int pos);

/** helper */
static __inline int get_request_len(char *buf, int len) {
    int          i;
    int          pos         = -1;
    unsigned int request_end = 0x0a0d0a0d;
    for (i = 0; i < len - 3; ++i) {
        unsigned int req = *((unsigned int *) (buf + i));
        if (req == request_end) {
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
    int    nRet     = 0;
    int    nZero    = 0;
    SOCKET sdSocket = INVALID_SOCKET;

    sdSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (sdSocket == INVALID_SOCKET) {
        printf("WSASocket(sdSocket) failed: %d\n", WSAGetLastError());
        return (sdSocket);
    }

    /* Disable send buffering on the socket.*/
    nZero = 0;
    nRet  = setsockopt(sdSocket, SOL_SOCKET, SO_SNDBUF, (char *) &nZero, sizeof(nZero));
    if (nRet == SOCKET_ERROR) {
        printf("setsockopt(SNDBUF) failed: %d\n", WSAGetLastError());
        /* return(sdSocket); */
    }

    return (sdSocket);
}

int wsserver_start(void *ptr, ws_accept_callback acb, uint16_t listen_port) {
    int                ret;
    SOCKET             listenfd;
    HANDLE             iocp;
    struct sockaddr_in server_addr;
    ws_accept_data    *data;
    iocp_server       *svr                       = (iocp_server *) ptr;
    DWORD              bytes                     = 0;
    GUID               acceptex_guid             = WSAID_ACCEPTEX;
    GUID               getacceptexsockaddrs_guid = WSAID_GETACCEPTEXSOCKADDRS;

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
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (listen_port == 0)
        server_addr.sin_port = htons((uint16_t) opt_websocket_port);
    else
        server_addr.sin_port = htons((uint16_t) listen_port);

    ret = bind(listenfd, (struct sockaddr *) (&server_addr), sizeof(server_addr));
    /*ret = bind(listenfd, addrlocal->ai_addr, addrlocal->ai_addrlen); */
    if (ret == SOCKET_ERROR) {
        printf("bind() failed: %d\n", WSAGetLastError());
        /*freeaddrinfo(addrlocal);*/
        return (-1);
    }

    ret = listen(listenfd, SOMAXCONN);
    if (ret == SOCKET_ERROR) {
        printf("listen() failed: %d\n", WSAGetLastError());
        /*freeaddrinfo(addrlocal);*/
        return (-1);
    }
    /*freeaddrinfo(addrlocal);*/

    /* init data */
    data = (ws_accept_data *) iocp_malloc(sizeof(ws_accept_data));
    /* memset(data, 0, sizeof(ws_accept_data)); */
    data->type          = WS_TYPE_ACCEPT;
    data->fd            = listenfd;
    data->proc_callback = ws_accept_proc;
    data->proc_close    = ws_accept_close;
    data->proc_timeout  = ws_accept_timeout;
    data->server        = svr;
    data->op            = WS_IO_ACCEPT;
    data->wsabuf.buf    = data->data;
    data->wsabuf.len    = sizeof(data->data);
    data->accept_fd     = INVALID_SOCKET;

    data->accept_callback = ws_proc_accept;
    if (acb) data->accept_callback = acb;

    /* update iocp */
    iocp = CreateIoCompletionPort((HANDLE) listenfd, svr->iocp, (DWORD_PTR) data, 0);
    if (iocp == NULL) {
        printf("CreateIoCompletionPort() failed: %ld\n", GetLastError());
        return (-1);
    }

    /* Load the AcceptEx extension function from the provider for this socket */
    ret = WSAIoctl(listenfd, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid),
                   &data->proc_accept, sizeof(data->proc_accept), &bytes, NULL, NULL);
    if (ret == SOCKET_ERROR) {
        printf("failed to load AcceptEx: %d\n", WSAGetLastError());
        return (-1);
    }

    /* Load the GetAcceptExSockaddrs */
    ret = WSAIoctl(listenfd, SIO_GET_EXTENSION_FUNCTION_POINTER, &getacceptexsockaddrs_guid,
                   sizeof(getacceptexsockaddrs_guid), &data->proc_getsockaddrs, sizeof(data->proc_getsockaddrs), &bytes,
                   NULL, NULL);
    if (ret == SOCKET_ERROR) {
        printf("failed to load GetAcceptExSockaddrs: %d\n", WSAGetLastError());
        return (-1);
    }

    ret = ws_accept(data);
    if (ret != 0) return ret;

    /* register */
    svr->iocp_register(svr, (iocp_data *) data);
    printf("fd %d accept fd %lld\n", data->fd, data->accept_fd);

    return listenfd;
}

int ws_accept(ws_accept_data *data) {
    DWORD bytes = 0;
    int   ret   = 0;

    data->accept_fd = socket_create();
    if (data->accept_fd == INVALID_SOCKET) {
        printf("failed to create new accept socket\n");
        return (-1);
    }

    /* pay close attention to these parameters and buffer lengths */
    ret = data->proc_accept(data->fd, data->accept_fd, (LPVOID) (data->data),
                            sizeof(data->data) - (2 * (sizeof(SOCKADDR_STORAGE) + 16)), sizeof(SOCKADDR_STORAGE) + 16,
                            sizeof(SOCKADDR_STORAGE) + 16, &bytes, (LPOVERLAPPED) & (data->overlapped));
    if (ret == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
        printf("AcceptEx() failed: %d\n", WSAGetLastError());
        return (-1);
    }
    return 0;
}

int ws_accept_proc(void *pdata, LPOVERLAPPED over, DWORD io_size) {
    ws_accept_data *data        = (ws_accept_data *) (pdata);
    int             ret         = 0;
    ws_client_data *client_data = NULL;
    ws_io_context  *ctx;
    DWORD           bytes;
    LPSOCKADDR      local;
    LPSOCKADDR      remote;
    int             local_len;
    int             remote_len;

    iocp_server *svr = (iocp_server *) data->server;

    ret = setsockopt(data->accept_fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *) &data->fd, sizeof(data->fd));
    if (ret == SOCKET_ERROR) {
        printf(
            "setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed to update accept "
            "socket\n");
        return (0);
    }

    data->proc_getsockaddrs(data->data, sizeof(data->data) - (2 * (sizeof(SOCKADDR_STORAGE) + 16)),
                            sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16, &local, &local_len, &remote,
                            &remote_len);

    /* init client data */
    client_data = (ws_client_data *) iocp_malloc(sizeof(ws_client_data));
    memset(client_data, 0, sizeof(ws_client_data));
    client_data->type          = WS_TYPE_CLIENT;
    client_data->fd            = data->accept_fd;
    client_data->server        = svr;
    client_data->proc_callback = ws_client_proc;
    client_data->proc_close    = ws_client_close;
    client_data->proc_timeout  = ws_client_timeout;

    ctx           = &client_data->recv_ctx;
    ctx->op       = WS_IO_RECV;
    ctx->data_len = 0;

    ctx           = &client_data->send_ctx;
    ctx->op       = WS_IO_SEND;
    ctx->data_len = 0;

    /* user accept callback */
    data->accept_callback((const struct sockaddr_in *) remote, &client_data->data_callback, &client_data->user,
                          &client_data->user_close, &client_data->timeout_callback);

    /* renew accept fd */
    ret = ws_accept(data);
    printf("connect fd %d, renew fd %d accept fd %d\n", client_data->fd, data->fd, data->accept_fd);
    if (ret != 0) {
        printf("failed to create new accept socket\n");
        iocp_free(client_data);
        return (-1);
    }

    /* pay close attention to these parameters and buffer lengths */
    bytes = 0;
    ret   = data->proc_accept(data->fd, data->accept_fd, (LPVOID) (data->data),
                              sizeof(data->data) - (2 * (sizeof(SOCKADDR_STORAGE) + 16)), sizeof(SOCKADDR_STORAGE) + 16,
                              sizeof(SOCKADDR_STORAGE) + 16, &bytes, (LPOVERLAPPED) & (data->overlapped));
    if (ret == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
        printf("AcceptEx() failed: %d\n", WSAGetLastError());
        return (-1);
    }

    /* update iocp */
    svr->iocp = CreateIoCompletionPort((HANDLE) client_data->fd, svr->iocp, (DWORD_PTR) client_data, 0);
    if (svr->iocp == NULL) {
        printf("CreateIoCompletionPort() failed: %d\n", GetLastError());
        iocp_free(client_data);
        return (-1);
    }

    /* iocp register */
    svr->iocp_register(svr, (iocp_data *) client_data);

    if (io_size) {
        /* move data from accept to client */
        ws_io_context *ctx = &client_data->recv_ctx;
        memcpy(ctx->data + ctx->data_len, data->data, io_size);
        ctx->op = WS_IO_RECV;

        /* proc data */
        return ws_client_proc(client_data, &ctx->overlapped, io_size);

    } else {
        DWORD          nRet;
        WSABUF         buffRecv;
        DWORD          dwRecvNumBytes = 0;
        DWORD          dwFlags        = 0;
        ws_io_context *ctx            = &client_data->recv_ctx;

        ctx->op = WS_IO_RECVING;

        buffRecv.buf = ctx->data + ctx->data_len;
        buffRecv.len = MAX_BUFF_SIZE - ctx->data_len;

        nRet = WSARecv(data->fd, &buffRecv, 1, &dwRecvNumBytes, &dwFlags, &ctx->overlapped, NULL);
        if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            printf("WSARecv() failed: %d\n", WSAGetLastError());
            return (-1);
        }
    }

    return (0);
}
void ws_accept_close(void *ptr) {
    ws_accept_data *data = (ws_accept_data *) ptr;
    LINGER          lingerStruct;

    /* force the subsequent closesocket to be abortative. */
    lingerStruct.l_onoff  = 1;
    lingerStruct.l_linger = 0;
    setsockopt(data->fd, SOL_SOCKET, SO_LINGER, (char *) &lingerStruct, sizeof(lingerStruct));

    closesocket(data->fd);
    closesocket(data->accept_fd);
    printf("ws accept_close %d %d \n", data->fd, data->accept_fd);
    iocp_free(ptr);
}

void ws_accept_timeout(void *ptr) {}

int ws_client_proc(void *ptr, LPOVERLAPPED over, DWORD io_size) {
    int             ret;
    ws_client_data *client = (ws_client_data *) (ptr);
    ws_io_context  *ctx    = (ws_io_context *) over;
    if (ctx->op == WS_IO_SENDING || ctx->op == WS_IO_SEND) {
        ret = ws_io_send(client, io_size);
    } else if (ctx->op == WS_IO_RECVING || ctx->op == WS_IO_RECV) {
        ret = ws_io_recv(client, io_size);
    }
    return ret;
}
void ws_client_close(void *ptr) {
    ws_client_data *data = (ws_client_data *) ptr;
    LINGER          lingerStruct;

    /* force the subsequent closesocket to be abortative. */
    lingerStruct.l_onoff  = 1;
    lingerStruct.l_linger = 0;
    setsockopt(data->fd, SOL_SOCKET, SO_LINGER, (char *) &lingerStruct, sizeof(lingerStruct));

    closesocket(data->fd);

    /* close user */
    data->user_close(data->user);

    printf("ws client_close %d\n", data->fd);
    iocp_free(ptr);
}

void ws_client_timeout(void *ptr) {
    DWORD           now  = 0;
    ws_client_data *data = (ws_client_data *) ptr;
    iocp_server    *svr  = (iocp_server *) data->server;
    now                  = GetTickCount();
    if ((now - data->tick) >= svr->timeout_period) {
        data->tick = now;
        data->timeout_callback(ptr);
    }
}

int ws_io_recv(ws_client_data *client, DWORD io_size) {
    ws_io_context *ctx;

    if (io_size == 0) {
        printf("recv failed\r\n");
        return (-1);
    }

    ctx     = (ws_io_context *) &client->recv_ctx;
    ctx->op = WS_IO_RECV;
    ctx->data_len += io_size;
    ctx->data[ctx->data_len] = '\0';
    ctx->payload_len         = 0;
    ctx->payload_pos         = 0;
    if (client->status == WS_STATUS_0)
        return ws_handshake(client);
    else
        return ws_proc(client);
}

int ws_io_send(ws_client_data *client, DWORD io_size) {
    ws_io_context *ctx;
    DWORD          nRet;
    DWORD          dwSendNumBytes = 0;
    DWORD          dwFlags        = 0;

    if (io_size == 0) {
        return (-1);
    }
    ctx = (ws_io_context *) &client->send_ctx;

    /* remove old message */
    ws_io_purge(ctx, io_size);

    if (ctx->data_len) {
        ctx->op         = WS_IO_SENDING;
        ctx->wsabuf.buf = ctx->data;
        ctx->wsabuf.len = ctx->data_len;

        nRet = WSASend(client->fd, &ctx->wsabuf, 1, &dwSendNumBytes, dwFlags, &(ctx->overlapped), NULL);
        if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            printf("WSASend() failed: %d\n", WSAGetLastError());
            return (-1);
        }
    } else {
        ctx->op = WS_IO_SEND;
    }
    return 0;
}

int ws_send(ws_client_data *client, const char *buf, int nbytes) {
    ws_io_context *ctx = &client->send_ctx;

    /* copy data to sending buffer */
    memcpy(ctx->data + ctx->data_len, buf, nbytes);
    ctx->data_len += nbytes;

    if (ctx->op == WS_IO_SEND) {
        DWORD  nRet;
        WSABUF buffSend;
        DWORD  dwSendNumBytes = 0;
        DWORD  dwFlags        = 0;

        ctx->op = WS_IO_SENDING;

        buffSend.buf = ctx->data;
        buffSend.len = ctx->data_len;
        nRet         = WSASend(client->fd, &buffSend, 1, &dwSendNumBytes, dwFlags, &(ctx->overlapped), NULL);
        if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            printf("WSASend() failed: %d\n", WSAGetLastError());
            return (-1);
        }
    }

    return 0;
}

int ws_recv(ws_client_data *client) {
    ws_io_context *ctx = &client->recv_ctx;

    if (ctx->op = WS_IO_RECV) {
        DWORD  nRet;
        WSABUF buffRecv;
        DWORD  dwRecvNumBytes = 0;
        DWORD  dwFlags        = 0;

        ctx->op = WS_IO_RECVING;

        buffRecv.buf = ctx->data + ctx->data_len;
        buffRecv.len = MAX_BUFF_SIZE - ctx->data_len;

        nRet = WSARecv(client->fd, &buffRecv, 1, &dwRecvNumBytes, &dwFlags, &ctx->overlapped, NULL);
        if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            printf("WSARecv() failed: %d\n", WSAGetLastError());
            return (-1);
        }
    }
    return 0;
}

int ws_handshake(ws_client_data *data) {
    int   line_end     = 0;
    int   line_begin   = 0;
    int   line_len     = 0;
    int   pos          = 0;
    int   remain_bytes = data->recv_ctx.data_len;
    char *buff;
    char *line;

    const char     sec_websocket_key[] = "Sec-WebSocket-Key";
    unsigned char *sec_value           = NULL;
    int            sec_value_len       = 0;

    char *response;
    int   response_len;
    int   request_len = 0;
    int   ret;

    buff = data->recv_ctx.data;

    /* check request state, wait for incoming data */
    request_len = get_request_len(buff, remain_bytes);
    if (request_len < 0) return (0);

    /* first line GET /address HTTP/1.1\r\n */
    line_end = get_line_end(buff + line_begin, remain_bytes);
    if (line_end < 4) return -1;
    remain_bytes -= line_end;
    line_len = line_end - 1;
    line     = buff + line_begin;

    if (strncmp(buff, "GET", 3) != 0) return -1;
    buff[line_end] = '\0';

    /* header */
    do {
        char *key;
        char *value;
        if (remain_bytes <= 0) break;

        line_begin += line_end + 1;
        line_end = get_line_end(buff + line_begin, remain_bytes);
        remain_bytes -= line_end;

        if (line_end < 2) break;
        line_len = line_end - 1;
        line     = buff + line_begin;

        line[line_end] = '\0';

        /* header key: value */
        pos   = get_space(line, line_len);
        key   = line;
        value = line + pos + 1;
        if (memcmp(key, sec_websocket_key, sizeof(sec_websocket_key) - 1) == 0) {
            sec_value     = (unsigned char *) value;
            sec_value_len = line_len - pos - 1;
            /* printf("got sec_value, %d %s\n", sec_value_len, sec_value); */
            /** only check sec_websocket_key */
            break;
        }
    } while (1);

    if (sec_value == NULL) return (-1);

#if 0
    //  response_len = snprintf(response, 512,
    //                          "HTTP/1.1 101 Switching Protocols\r\n"
    //                          "Upgrade: websocket\r\n"
    //                          "Connection: upgrade\r\n"
    //                          "Sec-WebSocket-Accept: %s\r\n\r\n",
    //                          data->data + 512);
#endif
    /* send handshake response */
    {
        char           buf[sizeof(WS_HANDSHAKE_0) + 28 + sizeof(WS_HANDSHAKE_1) + 1];
        ws_io_context *ctx = &data->send_ctx;

        response_len = sizeof(WS_HANDSHAKE_0) - 1 + 28 + sizeof(WS_HANDSHAKE_1) - 1;
        response     = buf;
        memcpy(response, WS_HANDSHAKE_0, sizeof(WS_HANDSHAKE_0) - 1);
        ws_sec_key(sec_value, sec_value_len, response + sizeof(WS_HANDSHAKE_0) - 1, 28);
        memcpy(response + sizeof(WS_HANDSHAKE_0) - 1 + 28, WS_HANDSHAKE_1, sizeof(WS_HANDSHAKE_1) - 1);

        response[response_len] = '\0';

        ret = ws_send(data, response, response_len);
        printf("%s\n", response);
        printf("%s  %d\n", WS_HANDSHAKE_1, sizeof(WS_HANDSHAKE_1));
    }

    {
        ws_io_context *ctx = &data->recv_ctx;
        /* remove old message */
        ws_io_purge(ctx, request_len);

        /* recv data */
        ret = ws_recv(data);
    }

    data->status = WS_STATUS_DATA;

    return ret;
}

int ws_proc(ws_client_data *client) {
    ws_io_context *ctx;
    websocket1    *ws;
    char          *data;
    char           send_buf[128];
    int            send_size = 128;
    int            ret       = 0;

    char *payload  = NULL;
    char *mask_key = NULL;

    int     pos         = 0;
    int64_t payload_len = 0;
    int     data_len    = 0;

    ctx  = (ws_io_context *) &client->recv_ctx;
    data = ctx->data;
    ws   = (websocket1 *) (data);

    data_len = ctx->data_len;

    /* get payload len */
    if (ws->len < WS_LEN2) {
        pos += 2;
        payload_len = ws->len;
    } else if (ws->len == WS_LEN2) {
        pos += 2 + 2;
        payload_len = *(int16_t *) (data + 2);
    } else if (ws->len == WS_LEN3) {
        pos += 2 + 8;
        payload_len = *(int64_t *) (data + 2);
    }

    /* get mask */
    if (ws->mask == 1) {
        mask_key = data + pos;
        pos += 4;
    }

    /* check length */
    if (data_len < payload_len + pos) {
        return (-1);
    }

    /* unmask payload */
    payload = data + pos;
    if (ws->mask == 1) {
        /* unmask */
        int i;
        for (i = 0; i < payload_len; ++i) {
            payload[i] = payload[i] ^ mask_key[i % 4];
        }
    }

    ctx->payload_len = (int) payload_len;
    ctx->payload_pos = pos;

    /* switch opcode */
    switch (ws->opcode) {
        case WS_PING:
            /* send pong */
            ws_send_pong(client);
            break;
        case WS_PONG:
            /* do nothing */
            break;
        case WS_CLOSE:
            /* close socket */
            client->status = WS_STATUS_CLOSE;
            return (-1);
            break;
        case WS_DATA1:
        case WS_DATA2:
            /* ws data */
            {
                wspkt *pkt  = (wspkt *) send_buf;
                char  *pbuf = pkt->data;
                send_size   = sizeof(send_buf) - sizeof(wshdr);

                ret = client->data_callback(&client->recv_ctx, client->user, &pbuf, &send_size);
                if (ret == 1) {
                    iocp_server *svr   = (iocp_server *) client->server;
                    svr->opt_quit_flag = 1;
                    return 0;
                } else if (ret != 0) {
                    return (ret);
                } else {
                    /* send data */
                    if (send_size > 0) {
                        pkt = (wspkt *) (pbuf - sizeof(wshdr));
                        if (send_size < WS_LEN2) {
                            pkt->hdr.ws1.fin    = 1;
                            pkt->hdr.ws1.len    = send_size;
                            pkt->hdr.ws1.mask   = 0;
                            pkt->hdr.ws1.opcode = WS_DATA1;
                            pkt->hdr.ws1.rsv    = 0;
                            ret                 = ws_send(client, (char *) ws1pkt(pkt), send_size + 2);
                        } else if (send_size < 65536) {
                            pkt->hdr.ws2.fin    = 1;
                            pkt->hdr.ws2.len    = WS_LEN2;
                            pkt->hdr.ws2.len2   = htons((uint16_t) send_size);
                            pkt->hdr.ws2.mask   = 0;
                            pkt->hdr.ws2.opcode = WS_DATA1;
                            pkt->hdr.ws2.rsv    = 0;
                            ret                 = ws_send(client, (char *) ws2pkt(pkt), send_size + 4);
                        } else {
                            pkt->hdr.ws3.fin       = 1;
                            pkt->hdr.ws3.len       = WS_LEN3;
                            pkt->hdr.ws3.len3_high = 0;
                            pkt->hdr.ws3.len3_low  = htonl(send_size);
                            pkt->hdr.ws3.mask      = 0;
                            pkt->hdr.ws3.opcode    = WS_DATA1;
                            pkt->hdr.ws3.rsv       = 0;
                            ret                    = ws_send(client, (char *) ws3pkt(pkt), send_size + 10);
                        }
                    }
                    /* free buf */
                    if ((char *) pkt != send_buf) {
                        iocp_free(pkt);
                    }
                }
            }
            break;
    }

    /* remove old message */
    ws_io_purge(ctx, ctx->payload_len + ctx->payload_pos);

    /* recv data */
    ret = ws_recv(client);

    return ret;
}

int ws_proc_data(ws_io_context *ctx, void *user, char **sendbuf, int *size) {
    const char *message_ =
        "{"
        "\"command\":\"test\""
        "}";

    int ret = 0;

    printf("proc data[%d] %s\n", ctx->payload_len, ctx->data + ctx->payload_pos);
    if (ctx->payload_len >= 4) {
        if (strncmp(ctx->data + ctx->payload_pos, "quit", 4) == 0) {
            return (1);
        }
    }

    *sendbuf = (char *) iocp_malloc(strlen(message_));
    memcpy(*sendbuf, message_, strlen(message_));
    *size = strlen(message_);

    return ret;
}

int ws_proc_accept(const struct sockaddr_in *remote, ws_data_callback *dc, void **user, ws_user_close *uc,
                   ws_timeout_callback *tc) {
    int            ret  = 0;
    unsigned int   ip   = remote->sin_addr.s_addr;
    unsigned short port = ntohs(remote->sin_port);
    printf("accept %3d.%3d.%3d.%3d:%5d\n", (ip & 0xff), ((ip & 0xff00) >> 8), ((ip & 0xff0000) >> 16),
           ((ip & 0xff000000) >> 24), port);

    *dc   = ws_proc_data;
    *user = NULL;
    *uc   = NULL;
    *tc   = NULL;

    return ret;
}

int ws_send_pong(ws_client_data *client) {
    char        buf[2];
    websocket1 *ws = (websocket1 *) (buf);
    ws->fin        = 1;
    ws->len        = 0;
    ws->mask       = 0;
    ws->opcode     = WS_PONG;
    ws->rsv        = 0;
    /* send pong */
    return ws_send(client, buf, 2);
}

int ws_send_packet(ws_client_data *client, wspkt *pkt, int nbytes) {
    int ret = 0;

    if (nbytes < WS_LEN2) {
        websocket1 *ws;
        ws         = &pkt->hdr.ws1;
        ws->fin    = 1;
        ws->len    = nbytes;
        ws->mask   = 0;
        ws->opcode = WS_DATA1;
        ws->rsv    = 0;

        ret = ws_send(client, (const char *) ws1pkt(pkt), nbytes + 2);

    } else if (nbytes < 65536) {
        websocket2 *ws;

        ws         = &pkt->hdr.ws2;
        ws->fin    = 1;
        ws->len    = WS_LEN2;
        ws->len2   = htons((uint16_t) nbytes);
        ws->mask   = 0;
        ws->opcode = WS_DATA1;
        ws->rsv    = 0;

        ret = ws_send(client, (const char *) ws2pkt(pkt), nbytes + 4);

    } else {
        websocket3 *ws;

        ws            = &pkt->hdr.ws3;
        ws->fin       = 1;
        ws->len       = WS_LEN3;
        ws->len3_high = 0;
        ws->len3_low  = htonl(nbytes);
        ws->mask      = 0;
        ws->opcode    = WS_DATA1;
        ws->rsv       = 0;

        ret = ws_send(client, (const char *) ws3pkt(pkt), nbytes + 10);
    }

    return ret;
}

int ws_send_data(ws_client_data *client, const char *data, int nbytes) {
    int ret = 0;
    if (nbytes < WS_LEN2) {
        websocket1 *ws;
        char       *buf = (char *) malloc(nbytes + 2);
        memset(buf, 0, nbytes + 2);
        ws         = (websocket1 *) (buf);
        ws->fin    = 1;
        ws->len    = nbytes;
        ws->mask   = 0;
        ws->opcode = WS_DATA1;
        ws->rsv    = 0;
        memcpy(buf + 2, data, nbytes);
        ret = ws_send(client, buf, nbytes + 2);
        free(buf);
    } else if (nbytes < 65535) {
        websocket2 *ws;
        char       *buf = (char *) malloc(nbytes + 4);
        memset(buf, 0, nbytes + 4);
        ws         = (websocket2 *) (buf);
        ws->fin    = 1;
        ws->len    = WS_LEN2;
        ws->len2   = htons((uint16_t) nbytes);
        ws->mask   = 0;
        ws->opcode = WS_DATA1;
        ws->rsv    = 0;
        memcpy(buf + 4, data, nbytes);
        ret = ws_send(client, buf, nbytes + 4);
        free(buf);
    } else {
        websocket3 *ws;
        char       *buf = (char *) malloc(nbytes + 10);
        memset(buf, 0, nbytes + 10);
        ws         = (websocket3 *) (buf);
        ws->fin    = 1;
        ws->len    = WS_LEN3;
        ws->len3   = htonl(nbytes);
        ws->mask   = 0;
        ws->opcode = WS_DATA1;
        ws->rsv    = 0;
        memcpy(buf + 10, data, nbytes);
        ret = ws_send(client, buf, nbytes + 10);
        free(buf);
    }
    return ret;
}

void ws_sec_key(const unsigned char *key, int klen, char *buf, int blen) {
    unsigned char sha1_key[24] = {0};

    const unsigned char ws_magic_key[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    SHA1_CTX            ctx;

    SHA1Init(&ctx);
    SHA1Update(&ctx, key, klen);
    SHA1Update(&ctx, ws_magic_key, sizeof(ws_magic_key) - 1);
    SHA1Final(sha1_key, &ctx);

    base64_encrypt_text2(sha1_key, 20, buf, blen);
}

void ws_io_purge(ws_io_context *ctx, int pos) {
    int remain = ctx->data_len - pos;
    if (remain > 0) {
        memcpy(ctx->data, ctx->data + pos, remain);
    }
    ctx->data_len = remain;
}
