#define WIN32_LEAN_AND_MEAN



#include <winsock2.h>
#include <Ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>


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
int proc_rpc(ws_io_context * data, char ** buf, int* size);


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
        iocp_server* server = NULL;

        server = iocp_server_start();

        wsserver_start(server, proc_rpc);

        iocp_server_join(server);

        iocp_server_stop(server);
    }
    
#ifdef _DEBUG
    printf("dump leaks\n");
    _CrtMemDumpStatistics(&state);
#endif
    return 0;
}


void u2w(const char* us, int us_len, wchar_t* ws, int ws_len) {
    int ret = MultiByteToWideChar(CP_UTF8, 0, us, us_len, ws, ws_len);
    ret;
}

void w2u(const wchar_t* ws, int ws_len, char* us, int us_len) {
    WideCharToMultiByte(CP_UTF8, 0, ws, ws_len, us, us_len, 0, 0);
}

json_t* get_jstr(const wchar_t* ws) {
    char str[256];
    json_t* value;
    int ws_len;

    memset(str, 0, 256);
    ws_len = wcslen(ws);
    w2u(ws, ws_len, str, 256);
    value = json_string(str);

    return value;
}

void get_wstr(const json_t* value, wchar_t* ws, int ws_len) {
    if(json_is_string(value)) {
        const char* us = json_string_value(value);
        int us_len = strlen(us);
        u2w(us, us_len, ws, ws_len);
    }
}

static __inline int gen_rpc_ok(char* buf, const char* method, int id) {
    const char* cmd_1 ="{"
            "\"id\":%d,"
            "\"method\":\"%s\","
            "\"returns\":[],"
            "\"error\":{"
            "\"code\":0,"
            "\"message\":\"OK\""
            "} }";
    return sprintf(buf, cmd_1, id, method);
}

static __inline int gen_rpc_bad(char* buf, const char* method, int id) {
    const char* cmd_1 ="{"
            "\"id\":%d,"
            "\"method\":\"%s\","
            "\"returns\":[],"
            "\"error\":{"
            "\"code\":1,"
            "\"message\":\"Unknown method\""
            "} }";
    return sprintf(buf, cmd_1, id, method);
}

int proc_rpc(ws_io_context * ctx, char ** buf, int* size) {

    json_t* value;
    json_t *args;
    const char* method;
    int id = 0;
    json_t* root = NULL;
    json_error_t err;
    
    root = json_loads(ctx->data+ctx->payload_pos, 0, &err);
    if(!root) {
        printf("request is not json format\n");
        return(0);
    }

    /*id */
    value = json_object_get(root, "id");
    id = json_integer_value(value);


    /* method */
    value = json_object_get(root, "method");
    method = json_string_value(value);
    if(!method) {
        printf("request no method\n");
        return(0);
    }

    /* parse method */
    if(strcmp(method, "set_rpc_version") == 0) {
        const char* version = NULL;
        args = json_object_get(root, "args");
        value = json_object_get(args, "version");
        version = json_string_value(value);

        printf("set version %s\n", version);
        *size = gen_rpc_ok(*buf, method, id);

    } else if(strcmp(method, "set_rpc_encrypt") == 0) {
        /*encrypt key and state*/
        int enable = 0;
        const char* key = NULL;

        args = json_object_get(root, "args");
        value = json_object_get(args, "enable");
        enable = json_integer_value(value);

        value = json_object_get(args, "key");
        key = json_string_value(value);

        if(enable) {
            printf("enable encrypt key: %s\n", key);
        } else {
            printf("disable encrypt\n");
        }

        *size = gen_rpc_ok(*buf, method, id);


    } else if(strcmp(method, "quit") == 0) {
        /* test only */
        return 1;
    } else {
        printf("bad method %s\n", method);
        *size = gen_rpc_bad(*buf, method, id);
    }


    
    json_decref(root);

    return 0;
}
