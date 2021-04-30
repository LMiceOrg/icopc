/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_SERVER_H_
#define OPCDA2_SERVER_H_

/*
4.5.1.8 Reading and Writing Data
There are basically three ways to get data into a client (ignoring the 'old' IDataObject/IAdviseSink).
• IOPCSyncIO::Read (from cache or device)
• IOPCAsyncIO2::Read (from device)
• IOPCCallback::OnDataChange() (exception based) which can also be triggered by IOPCAsyncIO2::Refresh.
In general the three methods operate independently without ‘side effects’ on each other.

There are two ways to write data out:
• IOPCSyncIO::Write
• IOPCAsyncIO2::AsyncWrite
*/
#include "opcda2_all.h"
#include "group.h"
#include "callback.h"
#include "util_list.h"



#define OPCDA2_SERVER_GROUP_MAX 16
#define OPCDA2_CONNECT_MAX 16


typedef struct server_info {
    LPOLESTR  prog_id;
    LPOLESTR  user_type;
} server_info;

#define OPCDA2_ITEM_ID_LEN 32
#define OPCDA2_ITEM_ID_SIZE (sizeof(wchar_t)*OPCDA2_ITEM_ID_LEN)

typedef struct item_info {
    wchar_t id[OPCDA2_ITEM_ID_SIZE];
} item_info;

/* OPC服务器连接 */
typedef struct server_connect {
    ICLIST_DATAHEAD
    DWORD       cookie; /* IOPCShutdown OPCDA2.0 support */
    IOPCServer* svr;    /* 需要Release */
    IUnknown * callback; /* 需要Release */

    data_list* datas;       /* 数据表 */
    wchar_t    host[32];    /* 网络地址 (ip 或者 名称) */
    wchar_t    prog_id[32]; /* OPC Server 名称 */

    int        item_size;
    item_info* item_list;
    int        grp_size; /* 现在的group数量 */
    group      grp[OPCDA2_SERVER_GROUP_MAX];
} server_connect;

/* 服务器连接链表 */
iclist_def_prototype(connect_list, server_connect, OPCDA2_CONNECT_MAX, 8);

int opcda2_alloc(connect_list** cl);
void opcda2_free(connect_list* cl);

/** 操作 OPCServerList **/

/* 获取可用OPCServer列表 */
int opcda2_serverlist_fetch(const wchar_t *host, int* size, server_info **info_list);
void opcda2_serverlist_clear(int size,  server_info *info_list);

/** 操作 OPCServer 连接 **/

/* 初始化connect, 成功返回0 */
int opcda2_connect_init(server_connect *c, const wchar_t* host, const wchar_t* prog_id,
                        data_list* items);

/* 连接 OPCServer */
int opcda2_connect_add(connect_list* clist, const wchar_t* host, const wchar_t* prog_id, data_list* items,
                          server_connect** conn);
void opcda2_connect_del(connect_list* clist, server_connect* conn);

/** 操作通知 提供：IOPCShutdown IOPCDataCallback 接口 **/

/*  增加通知，包含OPCServer:IOPCShutdown OPCGroup:IOPCDataCallback */
int opcda2_server_advise(server_connect *conn, IUnknown* cb);

/* 取消通知，包含OPCServer:IOPCShutdown OPCGroup:IOPCDataCallback */
void opcda2_server_unadvise(server_connect*conn);

/* 增加通知 OPCServer:IOPCShutdown */
int opcda2_server_advise_shutdown(server_connect* conn, IUnknown* sd);
/* 取消通知 OPCServer:IOPCShutdown */
void opcda2_server_unadvise_shutdown(server_connect *conn);

/** 操作 OPCGroup */

/* 增加组 */
int opcda2_group_add(server_connect* conn, const wchar_t *name, int active, int rate, group **grp);
/* 删除组 */
void opcda2_group_del(server_connect* conn, group *grp);

#endif  /**  OPCDA2_SERVER_H_ */
