#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libs/sms/sm4.h"
#include "iocpserver.h"
#include "wsserver.h"

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "../libs/jansson/src/jansson.h"

#ifdef _DEBUG
#pragma comment(lib, "../libs/jansson/src/jansson-mtd.lib")
#else
#pragma comment(lib, "../libs/jansson/src/jansson-mt.lib")
#endif
int my_proc_rpc(ws_io_context *data, void *u, char **buf, int *size);
int my_proc_accept(const struct sockaddr_in *, ws_data_callback *, void **u,
                   ws_user_close *, ws_timeout_callback *);
void my_user_close(void *user);

void my_proc_timeout(void *ptr);

typedef struct _my_usert {
  int id;
} my_user;

static void print_u16(const unsigned char *p) {
  int i;
  for (i = 0; i < 16; ++i) {
    printf("0x%x ", *(p + i));
    if (i == 7)
      printf("\n");
  }
  printf("\n");
}

int main(void) {

#ifdef _DEBUG
  _CrtMemState state;
  _CrtMemCheckpoint(&state);
#endif
#if 0
    {
        
        const char* cdata=
                "{"
                "\"command\":\"test\""
                "}";
        json_t* root;
        json_error_t err;
        root = json_loads(cdata, 0, &err);
        if(!root) {
            printf("load failed\n");
        }
        printf("root %p\n", root);
        
        {
            char *buf;
            int sz;
            proc_rpc(NULL,&buf, &sz);
        }
        getch();
        return 0;

    }
#endif

  {
    /* test sm4 */
    char user_key[17] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe,
                         0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0x0};

    const unsigned char plaintext[16] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
    };

    const unsigned char ciphertext1[16] = {
        0x68, 0x1e, 0xdf, 0x34, 0xd2, 0x06, 0x96, 0x5e,
        0x86, 0xb3, 0xe9, 0x4f, 0x53, 0x6e, 0x42, 0x46,
    };

    unsigned char buf[16];
    unsigned char buf2[16];
    int out_size;

    const char p[] = "wel to CN!";
    const char k[] = "13010388100";

    sm4_encrypt(plaintext, buf, 16, user_key, 16);

    if (memcmp(ciphertext1, buf, 16) == 0) {
      printf("sms4 encrypt pass!\n");
    } else {
      printf("sms4 encrypt not pass!\n");
    }

    sm4_decrypt(buf, buf2, 16, user_key, 16);
    if (memcmp(plaintext, buf2, 16) == 0) {
      printf("sms4 decrypt pass!\n");
    } else {
      printf("sms4 decrypt not pass!\n");
      printf("ori:\n");
      print_u16(plaintext);
      printf("rst:\n");
      print_u16(buf);
    }

    memset(buf, 0, 16);
    memset(buf2, 0, 16);
    out_size = sm4_size(strlen(p));
    printf("p %s %d  k %s %d, outsize:%d\n", p, strlen(p), k, strlen(k),
           out_size);
    sm4_encrypt(p, buf, strlen(p), k, strlen(k));
    printf("rst:\n");
    print_u16(buf);

    sm4_decrypt(buf, buf2, 16, k, strlen(k));
    printf("rst:\n");
    print_u16(buf2);
    printf("%s\n", (const char *)buf2);
    return 0;
  }

  {
    iocp_server *server = NULL;

    server = iocp_server_start(5000);

    wsserver_start(server, my_proc_accept);

    iocp_server_join(server);

    iocp_server_stop(server);
  }

#ifdef _DEBUG
  printf("dump leaks\n");
  _CrtMemDumpStatistics(&state);
#endif
  return 0;
}

int my_proc_accept(const struct sockaddr_in *remote, ws_data_callback *dc,
                   void **u, ws_user_close *uc, ws_timeout_callback *utc) {
  my_user *user;
  int ret = 0;
  unsigned int ip = remote->sin_addr.s_addr;
  unsigned short port = ntohs(remote->sin_port);
  printf("accept %3d.%3d.%3d.%3d:%5d\tuid=%d\n", (ip & 0xff),
         ((ip & 0xff00) >> 8), ((ip & 0xff0000) >> 16),
         ((ip & 0xff000000) >> 24), port, ip);

  *dc = my_proc_rpc;
  *u = NULL;
  *uc = NULL;
  *utc = NULL;

  user = (my_user *)malloc(sizeof(my_user));
  user->id = ip;
  *u = user;
  *uc = my_user_close;
  *utc = my_proc_timeout;

  return ret;
}

void my_proc_timeout(void *ptr) {
  ws_client_data *client = (ws_client_data *)(ptr);
  unsigned int ip = ((my_user *)client->user)->id;
  printf("client %3d.%3d.%3d.%3d timeout\n", (ip & 0xff), ((ip & 0xff00) >> 8),
         ((ip & 0xff0000) >> 16), ((ip & 0xff000000) >> 24));
}

void u2w(const char *us, int us_len, wchar_t *ws, int ws_len) {
  int ret = MultiByteToWideChar(CP_UTF8, 0, us, us_len, ws, ws_len);
  ret;
}

void w2u(const wchar_t *ws, int ws_len, char *us, int us_len) {
  WideCharToMultiByte(CP_UTF8, 0, ws, ws_len, us, us_len, 0, 0);
}

json_t *get_jstr(const wchar_t *ws) {
  char str[256];
  json_t *value;
  int ws_len;

  memset(str, 0, 256);
  ws_len = wcslen(ws);
  w2u(ws, ws_len, str, 256);
  value = json_string(str);

  return value;
}

void get_wstr(const json_t *value, wchar_t *ws, int ws_len) {
  if (json_is_string(value)) {
    const char *us = json_string_value(value);
    int us_len = strlen(us);
    u2w(us, us_len, ws, ws_len);
  }
}

static __inline int gen_rpc_ok(char *buf, const char *method, int id) {
  const char *cmd_1 = "{"
                      "\"id\":%d,"
                      "\"method\":\"%s\","
                      "\"returns\":[],"
                      "\"error\":{"
                      "\"code\":0,"
                      "\"message\":\"OK\""
                      "} }";
  return sprintf(buf, cmd_1, id, method);
}

static __inline int gen_rpc_bad(char *buf, const char *method, int id) {
  const char *cmd_1 = "{"
                      "\"id\":%d,"
                      "\"method\":\"%s\","
                      "\"returns\":[],"
                      "\"error\":{"
                      "\"code\":1,"
                      "\"message\":\"Unknown method\""
                      "} }";
  return sprintf(buf, cmd_1, id, method);
}

int my_proc_rpc(ws_io_context *ctx, void *u, char **buf, int *size) {

  json_t *value;
  json_t *args;
  const char *method;
  int id = 0;
  json_t *root = NULL;
  json_error_t err;
  my_user *user = (my_user *)u;

  root = json_loads(ctx->data + ctx->payload_pos, 0, &err);
  if (!root) {
    printf("request is not json format\n");
    return (0);
  }

  /*id */
  value = json_object_get(root, "id");
  id = json_integer_value(value);

  /* method */
  value = json_object_get(root, "method");
  method = json_string_value(value);
  if (!method) {
    printf("request no method\n");
    return (0);
  }

  /* parse method */
  if (strcmp(method, "set_rpc_version") == 0) {
    const char *version = NULL;
    args = json_object_get(root, "args");
    value = json_object_get(args, "version");
    version = json_string_value(value);

    printf("set version %s\n", version);
    *size = gen_rpc_ok(*buf, method, id);

  } else if (strcmp(method, "set_rpc_encrypt") == 0) {
    /*encrypt key and state*/
    int enable = 0;
    const char *key = NULL;

    args = json_object_get(root, "args");
    value = json_object_get(args, "enable");
    enable = json_integer_value(value);

    value = json_object_get(args, "key");
    key = json_string_value(value);

    if (enable) {
      printf("enable encrypt key: %s\n", key);
    } else {
      printf("disable encrypt\n");
    }

    *size = gen_rpc_ok(*buf, method, id);

  } else if (strcmp(method, "quit") == 0) {
    /* test only */
    return 1;
  } else {
    printf("bad method %s\n", method);
    *size = gen_rpc_bad(*buf, method, id);
  }

  json_decref(root);

  return 0;
}

void my_user_close(void *u) {
  my_user *user = (my_user *)u;
  printf("close user %d\n", user->id);
  free(user);
}
