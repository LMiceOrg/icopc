/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef LMICE_PRIVATE_H
#define LMICE_PRIVATE_H

#include <stddef.h>

#include "heap.h"
#include "iocpserver.h"
#include "lmice.h"

#include "eal/lmice_eal_common.h"
#include "eal/lmice_eal_hash.h"
#include "../opcda2/util_ticketlock.h"

#define heap_malloc(x) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (x))
#define heap_free(ptr) HeapFree(GetProcessHeap(), 0, ptr)

/** 堆内存 名称模版 heap_name + heap_id<oct 6> */
#define HEAP_NAME "Global\\smLMiced"

#define HEAP_COUNT 8
#define HEAP_ALIGN (2 * 1024 * 1024)
#define HEAP_MAX_SIZE (1024 * 1024 * 1024)
#define HEAP_DEFAULT_SIZE (10 * 1024 * 1024)

#define HEAP_DATA_ALIGN 16

typedef enum {
    FIXLENGTH = 0, /**< 固定长度资源类型 */
    VARLENGTH = 1, /**< 可变长度资源类型 */
    READMODE  = 1, /**< 读取权限 */
    WRITEMODE = 2, /**< 写入权限 */
    EXECMODE  = 4  /**< 执行权限 */
} NODEMODE;

/** 事件名称 event_name + heap_id<oct 6> + node_id<hex 8> */
#define EVENT_NAME "Global\\evLMiced"

#define SVRBRD_EVENT_NAME (EVENT_NAME "svrbrd")

#define CLIBRD_EVENT_NAME (EVENT_NAME "clibrd")

#define BOARD_SPIEVENT_SIZE 32

/*
 * SPI 以IPC服务调用接口访问本地服务端事件处理流程
 1. IPC: LOCAL SPI以IPC访问本机服务端
 << client --> |board| --> server >>
            ** IPC **

 * SPI 以RPC服务调用接口访问本地/远程服务端事件处理流程
 2. WS/TCP/UDS: LOCAL/REMOTE SPI以IPC访问本机代理，代理以网络协议访问本地/远程服务端
 << client -->|board| --> proxy >> <-- WS/TCP/UDS --> << server --> stub --> |board| --> server >>
           **  IPC **                  ** TCP/IP **                 ** IPC **


 * 非SPI 以网络直接访问服务端事件处理流程
 3. WS/TCP/UDS: LOCAL/REMOTE 非SPI访问本地/远程主机，访问通过网关传递处理请求
 << client >> <-- WS/TCP/UDS --> << server --> stub --> |board| --> server >>
                ** TCP/IP **                ** IPC **
*/

/** SPI事件：服务端-客户端事件 */
typedef struct mispi_event {
    miu32    size;   /**<  事件长度 */
    miu32    type;   /**<  事件类型 */
    mihandle handle; /**< 事件附属资源 */
} mispi_event;

/** 公告板： 客户端与服务端之间通过SPI事件通讯 */
typedef struct miboard {
    miu32       id;                         /**< ID  */
    miu32       size;                       /**< SPI事件数量 */
    mispi_event event[BOARD_SPIEVENT_SIZE]; /**< SPI事件列表 */
    mii8        ename[MINAMESIZE * 2];      /**< 公告板事件名称 */
} miboard;

typedef struct mipub_event {
    mispi_event ev;
    miu32       size;
    miu32       mode;
    mitype      tid;
    miinst      oid;
} mipub_event;

#define mipub_event_size ((sizeof(mipub_event) + sizeof(mispi_event) - 1) / sizeof(mispi_event))
#define mipub_to_event(_pc) ((mipub_event*) ((miu8*) _pc + offsetof(mipub_client, ev)))

typedef struct mipub_client {
    struct mispi* spi;
    miu8*         data;

    mispi_event ev;
    miu32       size;
    miu32       mode;
    mitype      tid;
    miinst      oid;

    miu32 status; /* 发布资源状态 */

    HANDLE event;
    HANDLE hwait;
} mipub_client;

typedef struct mispi {
    /* 等待事件回调处理函数 */
    void(__stdcall* wait_callback)(void* ptr, BOOLEAN timer_or_wait);
    /* 发起服务端事件 */
    int (*fire_event)(struct mispi* spi, const mispi_event* ev);

    /* 资源发布 SPI */
    void(__stdcall* pub_callback)(void* ptr, BOOLEAN timer_or_wait);
    struct mipub_client* (*pub_create)(struct mispi* spi, const mistring t_name, const mistring o_name, miu32 size,
                                       miu32 mode);
    void (*pub_release)(struct mipub_client* pub);
    int (*pub_data)(struct mipub_client* pub, const void* ptr, miu32 size);

    miu32    status; /* 当前状态 */
    miu32    lock;
    miu32    type; /* SPI类型 */
    mihandle heap;

    miboard* server;  /* 服务端公告板 */
    HANDLE   hserver; /* 服务端公告板事件 */

    miboard* board;  /* 客户端公告板 */
    HANDLE   hboard; /* 客户端公告板事件 */
    HANDLE   hwait;  /* 等待事件 */

    mispi_event ev;
} mispi;

typedef struct node_head {
    /* 堆属性 */
    miu32 heap_id; /* 堆 id */
    miu32 id;      /* 资源id */
    miu32 size;    /* 对齐数据长度 */
    miu32 lock;    /* 锁 */

    /* 对象属性 */
    miu8   o_type;   /**< 资源类型 */
    miu8   o_mode;   /**< 访问权限 */
    miu16  o_owner;  /**< 所有者id */
    mitick o_create; /**< 创建时间 */
    mitick o_access; /**< 访问时间 */

    /* 数据属性 */
    miu32  d_size;
    miu32  d_session;
    mitype d_type;
    miinst d_inst; /**<   */
    mitick d_tick; /**< 数据（修改）时间 */
} node_head;

#define EAL_data_lock(ptr) EAL_ticket_lock(&((node_head*) ((miu8*) ptr - sizeof(node_head)))->lock)
#define EAL_data_unlock(ptr) EAL_ticket_unlock(&((node_head*) ((miu8*) ptr - sizeof(node_head)))->lock)

#define miheap_data_handle(_hdr, _p) _hdr.ptr = ((void*) ((miu8*) _p - sizeof(node_head)))
#define miheap_data_head(_p) ((node_head*) ((miu8*) _p - sizeof(node_head)))

#define MISPI_event_set(_ret, _b, _ptr)                   \
    do {                                                  \
        miu32 _sz = _ptr->size;                           \
        _ret      = 0;                                    \
        if (_b->size + _sz < BOARD_SPIEVENT_SIZE) {       \
            mispi_event* _ec = _b->event + _b->size;      \
            memcpy(_ec, _ptr, sizeof(mispi_event) * _sz); \
            _b->size += _sz;                              \
        } else {                                          \
            _ret = 1;                                     \
        }                                                 \
    } while (0)

#endif /** LMICE_PRIVATE_H */
