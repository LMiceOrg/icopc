/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */

#include "publish.h"
#include "lmice_private.h"

#include "iocpserver.h"
#if 0
typedef struct {
    int (*proc_callback)(void* pdata, LPOVERLAPPED over, DWORD io_len);
    void (*proc_close)(void* ptr);
    void (*proc_timeout)(void* ptr);

    void*  server;
    HANDLE event;
    miu32  heap_id;
    miu32  node_id;
    char   name[32];
    miu64  hval;

} mipub_server;

mihandle spi_pub_create(mihandle s, const mistring t_name, const char* o_name, miu32 size, miu32 mode) {
    mispi*        spi = (mispi*) s.ptr;
    mipub_client* pub;
    mihandle      handle;
    miu8*         ptr;
    node_head*    head;
    char          ename[32];
    miu32         id_pos = sizeof(EVENT_NAME) - 1;
    miu32         heap_id;
    miu32         node_id;
    miu32         nlen = strlen(t_name);

    handle.i64 = 0;

    if (nlen >= 32) return handle;

    ptr = miheap_malloc(spi->heap, size);
    if (ptr == NULL) return handle;

    head = (node_head*) (ptr - sizeof(node_head));

    pub = (mipub_client*) heap_malloc(sizeof(mipub_client));
    if (pub == NULL) {
        miheap_free(spi->heap, ptr);
        return handle;
    }

    pub->spi     = spi;
    pub->data    = ptr;
    pub->heap_id = head->heap_id;
    pub->node_id = head->id;
    pub->size    = head->size;
    memcpy(pub->name, t_name, nlen);

    heap_id = pub->heap_id;
    node_id = pub->node_id;

    memset(ename, 0, sizeof(ename));
    memcpy(ename, EVENT_NAME, sizeof(EVENT_NAME) - 1);
    ename[id_pos + 0] = '0' + (heap_id / 10000) % 10;
    ename[id_pos + 1] = '0' + (heap_id / 1000) % 10;
    ename[id_pos + 2] = '0' + (heap_id / 100) % 10;
    ename[id_pos + 3] = '0' + heap_id / 10;
    ename[id_pos + 4] = '0' + heap_id % 10;

    ename[id_pos + 5]  = 'A' + (node_id / 0x10000000) % 0x10;
    ename[id_pos + 6]  = 'A' + (node_id / 0x1000000) % 0x10;
    ename[id_pos + 7]  = 'A' + (node_id / 0x100000) % 0x10;
    ename[id_pos + 8]  = 'A' + (node_id / 0x10000) % 0x10;
    ename[id_pos + 9]  = 'A' + (node_id / 0x1000) % 0x10;
    ename[id_pos + 10] = 'A' + (node_id / 0x100) % 0x10;
    ename[id_pos + 11] = 'A' + (node_id / 0x10) % 0x10;
    ename[id_pos + 12] = 'A' + (node_id / 0x1) % 0x10;

    pub->event = CreateEventA(NULL, FALSE, FALSE, ename);

    handle.ptr = pub;

    /* fire spi event */

    return handle;
}

void spi_pub_release(mihandle p) {
    mipub_client* pub = (mipub_client*) p.ptr;
    mispi*        spi = (mispi*) pub->spi;

    /* fire spi event */
}

int spi_pub_data(mihandle p, const void* ptr, miu32 size) {
    mipub_client* pub = (mipub_client*) p.ptr;
    int           ret = 0;
    node_head*    head;

    head = (node_head*) (pub->data - sizeof(node_head));

    if (size > pub->size) {
        ret = 1;
    } else {
        EAL_ticket_lock(&head->lock);
        memcpy(pub->data, ptr, size);
        EAL_ticket_unlock(&head->lock);
        SetEvent(pub->event);
    }
    return ret;
}
#endif
