/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include "../opcda2/util_trace.h"
#include "lmice.h"
#include "lmice_private.h"
#include "publish.h"

#include "eal/lmice_eal_util.h"

static mipub_client* pub_create(mispi* spi, const mistring t_name, const char* o_name, miu32 size, unsigned int mode);

static void pub_release(mipub_client* pub);

static int pub_data(mipub_client* pub, const void* ptr, miu32 size);

/* FIXME: 完成spi type 支持， proxy-stub 机制实现 */
static int get_server_board(mihandle h, miu32 type, miboard** pboard, HANDLE* pevent);

static int fire_event(mispi* spi, const mispi_event* ev);

static void __stdcall wait_callback(void* ptr, BOOLEAN timer_or_wait);

eal_noexport eal_inline void name_append_u32(char* name, miu32 id) {
    int         i;
    const char* hval = "0123456789ABCDEF";
    for (i = 0; i < 8; ++i) {
        miu32 pos = id % 0x10;
        *name     = hval[pos];
        name++;
        id = id >> 4;
    }
}

static __inline int gen_name(char* name, const char* pre, size_t pre_len, miu32 id, const char* user) {
    size_t pos;
    size_t sz;
    memcpy(name, pre, pre_len);
    pos = pre_len;

    sz = strlen(user);
    memcpy(name + pos, user, sz);
    pos += sz;

    eal_int_str(name + pos, id);
    return pos + 8;
}

static int __inline gen_event_name(char* ename, miu32 id, const char* user) {
    return gen_name(ename, CLIBRD_EVENT_NAME, sizeof(CLIBRD_EVENT_NAME) - 1, id, user);
}

mihandle spi_create(miu32 type, const mistring name) {
    mispi*   spi;
    mihandle handle;
    size_t   nlen;
    int      ret;

    if (type != 0) {
        trace_debug("FIXME: SPI don't support type %X\n", type);
    }
    nlen = strlen(name);
    if (nlen >= MINAMESIZE) {
        trace_debug("spi_create name too long[%l]\n", nlen);
        nlen = MINAMESIZE - 1;
    }

    spi = (mispi*) heap_malloc(sizeof(mispi));

    spi->wait_callback = wait_callback;
    spi->fire_event    = fire_event;

    spi->pub_create  = pub_create;
    spi->pub_release = pub_release;
    spi->pub_data    = pub_data;

    spi->type = type;
    spi->heap = miheap_create(0);

    ret = get_server_board(spi->heap, spi->type, &spi->server, &spi->hserver);
    if (ret != 0) {
        trace_debug("get_server_board failed %d\n", ret);
    }

    /* 创建board */
    spi->board = (miboard*) miheap_malloc(spi->heap, sizeof(miboard));
    memset(spi->board, 0, sizeof(miboard));

    spi->board->id = eal_atomic_xadd(&spi->server->id, 1);
    gen_event_name(spi->board->ename, spi->board->id, name);

    spi->hboard = CreateEvent(NULL, FALSE, FALSE, spi->board->ename);

    /* 注册SPI事件回调 */
    RegisterWaitForSingleObject(&spi->hwait, spi->hboard, spi->wait_callback, (void*) spi, INFINITE, WT_EXECUTEDEFAULT);

    /* 发起创建客户端事件 */
    spi->ev.size       = 1;
    spi->ev.type       = MICLIENTCREATE;
    spi->ev.handle.ptr = spi->board;
    spi->fire_event(spi, &spi->ev);

    handle.ptr = spi;
    return handle;
}

int fire_event(mispi* spi, const mispi_event* ev) {
    miboard* board = spi->server;
    int      ret   = 0;
    EAL_data_lock(board);
    MISPI_event_set(ret, board, ev);
    EAL_data_unlock(board);
    /* 通知服务端处理 */
    if (ret == 0) SetEvent(spi->hserver);
    return ret;
}

void __stdcall wait_callback(void* ptr, BOOLEAN timer_or_wait) {
    mispi*       spi   = (mispi*) ptr;
    miboard*     board = (miboard*) spi->board;
    miu32        i;
    miu32        size;
    mispi_event  event[512];
    mispi_event* ev;

    /* timeout */
    if (timer_or_wait) return;

    /* 获取SPI事件列表 */
    EAL_data_lock(board);
    size = board->size;
    if (size > 0 && size <= BOARD_SPIEVENT_SIZE) {
        memcpy(event, board->event, board->size * sizeof(mispi_event));
    } else {
        size = 0;
    }
    board->size = 0;
    EAL_data_unlock(board);

    /* 处理SPI事件服务端处理状态 */
    ev = event;
    for (i = 0; i < size; i += ev->size) {
        ev = event + i;
        switch (ev->type) {
            case MICLIENTCREATE:
                if (ev->handle.i32 == MIOK) {
                    spi->status = MIRUNNING;
                }
                break;
            case MICLIENTRELEASE:
                if (ev->handle.i32 == MIOK) {
                    spi->status = MICLOSED;
                }
                break;
            case MICLIENTPUBCREATE:
                if (ev->handle.i32 == MIOK) {
                }
                break;
        }
        i += ev->size;
    }
}

static mipub_client* pub_create(mispi* spi, const mistring t_name, const mistring o_name, miu32 size, miu32 mode) {
    mipub_client* pub  = NULL;
    miu8*         data = NULL;
    node_head*    head = NULL;
    size_t        nlen;
    mitype        tid;
    miinst        oid;

    nlen = strlen(t_name);
    tid  = eal_hash64_fnv1a(t_name, nlen);

    nlen = strlen(o_name);
    oid  = eal_hash64_fnv1a(o_name, nlen);

    /* 资源是可变长: 更新长度 */
    if (size == MIRESVARLENGTH) size = MIRESSIZE;

    /* 分配发布数据区 */
    data = miheap_malloc(spi->heap, size);
    if (data == NULL) return pub;

    head = miheap_data_head(data);

    pub = (mipub_client*) heap_malloc(sizeof(mipub_client));
    if (pub == NULL) {
        miheap_free(spi->heap, data);
        return pub;
    }

    pub->spi                    = spi;
    pub->data                   = data;
    pub->ev.size                = mipub_event_size;
    pub->ev.type                = MICLIENTPUBCREATE;
    pub->ev.handle.heap.heap_id = head->heap_id;
    pub->ev.handle.heap.node_id = head->id;
    pub->size                   = head->size;
    pub->mode                   = mode;
    pub->tid                    = tid;
    pub->oid                    = oid;

    /* 创建事件 */
    spi->fire_event(spi, (mispi_event*) mipub_to_event(pub));

    return pub;
}
