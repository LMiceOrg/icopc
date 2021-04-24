/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_SERVER_H_
#define OPCDA2_SERVER_H_

/*
4.5.1.8 Reading and Writing Data
There are basically three ways to get data into a client (ignoring the 'old' IDataObject/IAdviseSink).
• IOPCSyncIO::Read (from cache or device)
• IOPCAsyncIO2::Read (from device)
• IOPCCallback::OnDataChange() (exception based) which can also be triggered by IOPCAsyncIO2::Refresh.
In general the three methods operate independently without ‘side effects’ on each other. There are two ways to write data out:
• IOPCSyncIO::Write
• IOPCAsyncIO2::AsyncWrite
*/
#include "opcda2_all.h"
#include "group.h"
#include "callback.h"

#define OPCDA2_SERVER_GROUP_MAX 16

typedef struct server_info {
    LPOLESTR  prog_id;
    LPOLESTR  user_type;
} server_info;

typedef struct item_info {
    wchar_t id[32];
} item_info;

typedef struct server_connect {
    DWORD       cookie; /* IOPCShutdown OPCDA2.0 support */
    IOPCServer* svr;    /* 需要Release */

    data_list* datas;       /* 数据表 */
    unsigned int hash;
    wchar_t    host[32];    /* 网络地址 (ip 或者 名称) */
    wchar_t    prog_id[32]; /* OPC Server 名称 */

    int        item_size;
    item_info* item_list;
    int        grp_size; /* 现在的group数量 */
    group      grp[OPCDA2_SERVER_GROUP_MAX];
} server_connect;

int opcda2_server_info_fetch(const wchar_t *host, int* size, server_info **info_list);
void opcda2_server_info_clear(int size,  server_info **info_list);

int opcda2_server_connect(const wchar_t *host, const wchar_t* prog_id, data_list* items, server_connect** conn);
void opcda2_server_disconnect(server_connect** conn);

/** 读取数据 */

/* 通知 IOPCShutdown */
int opcda2_server_advise_shutdown(server_connect* conn, IUnknown* sd);

/** 组操作 */
int opcda2_group_add(server_connect* conn, const wchar_t *name, int active, int rate, group **grp);
int opcda2_group_del(server_connect* conn, group **grp);

#endif  /**  OPCDA2_SERVER_H_ */
