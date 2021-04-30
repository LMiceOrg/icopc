/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_UTIL_LIST_H_
#define OPCDA2_UTIL_LIST_H_

#include "util_hash.h"
#include "util_ticketlock.h"
#include "util_trace.h"

#define ICLIST_INVALID_HANDLE ((unsigned int) 0xFFFFFFFF)
#define ICLIST_NOTUSE 0
#define ICLIST_USING 1
#define ICLIST_HASCHILD 2
#define ICLIST_LEAFNODE 0

#define ICLIST_DATAHEAD                                                                 \
    int          used;   /* 使用与否 */                                             \
    unsigned int handle; /* 所在列表位置 */                                       \
    unsigned int hash;   /* item 散列值 */                                           \
    int          prev;   /* 相同hash下 上一条记录的数据位置，0: 没有 */ \
    int          next;   /* 相同hash下 下一条记录的数据位置, 0: 没有 */  \
    ticketlock_t lock;   /* 自旋锁 */

typedef struct iclist_head {
    ICLIST_DATAHEAD
} iclist_head;

/* 散列表 */
#define iclist_def_prototype(_type, _item, data_size, extra_size)                                \
    struct _type;                                                                                \
    typedef void (*_type##_release_callback)(struct _type * pt, _item * dt);                     \
    typedef struct _type {                                                                       \
        int                      size;                                                           \
        ticketlock_t             lock;                                                           \
        _type##_release_callback release;                                                        \
        _item                    data[data_size];                                                \
        _item                    extra[extra_size];                                              \
    } _type;                                                                                     \
    typedef enum _type##_enum{_type##_item_size = sizeof(_item), _type##_data_size = data_size,  \
                              _type##_extra_size = extra_size,                                   \
                              _type##_capacity   = (data_size + extra_size)} _type##_enum;         \
    extern unsigned int iclist_##_type##_add(_type* pt, unsigned int hash);                      \
    extern void         iclist_##_type##_del(_type* pt, unsigned int handle);                    \
    extern _type*       iclist_##_type##_alloc(_type##_release_callback cb);                     \
    extern void         iclist_##_type##_free(_type* pt);                                        \
    extern unsigned int iclist_##_type##_hash(const void* dt, unsigned int len, unsigned int h); \
    extern void         iclist_##_type##_clear(_type* pt)

/* 散列表函数 */
#define iclist_def_function(_type, _item)                                                         \
    unsigned int iclist_##_type##_add(_type* pt, unsigned int hash) {                             \
        _item*       data      = iclist_data(pt, hash);                                           \
        unsigned int handle    = ICLIST_INVALID_HANDLE;                                           \
        int          extra_pos = 0;                                                               \
        _item*       parent    = NULL;                                                            \
        /* 锁表 */                                                                              \
        iclist_lock(pt);                                                                          \
        if (data->used == ICLIST_NOTUSE || data->used == ICLIST_HASCHILD) {                       \
            data->used   = ICLIST_USING;                                                          \
            data->handle = hash;                                                                  \
            data->hash   = hash;                                                                  \
            handle       = hash;                                                                  \
        } else if (data->used == ICLIST_USING) {                                                  \
            /* 正在使用，循环查找叶节点 */                                            \
            for (extra_pos = 0; extra_pos < _type##_extra_size; ++extra_pos) {                    \
                /* 找到叶节点，退出查找 */                                              \
                if (parent->next == ICLIST_LEAFNODE) break;                                       \
                parent = pt->data + parent->next;                                                 \
            }                                                                                     \
            /* 循环深度达到最大值，意味数据表被破坏，出现死循环 */        \
            if (extra_pos == _type##_extra_size) {                                                \
                trace_debug("iclist_##_type##add error:table chain status incorrect\n");          \
                handle = ICLIST_INVALID_HANDLE;                                                   \
                goto err_occured;                                                                 \
            }                                                                                     \
            /* 2. 在extra查找 在未使用的位置插入 */                                   \
            for (extra_pos = _type##_data_size; extra_pos < _type##_capacity; ++extra_pos) {      \
                _item* extra = pt->data + extra_pos;                                              \
                if (extra->used == ICLIST_NOTUSE) {                                               \
                    /* 此时 parent:父节点 extra:叶节点 */                                 \
                    parent->next  = extra_pos;      /* 修改父节点的next */                  \
                    extra->next   = 0;              /* 修改叶节点的next(0) */               \
                    extra->prev   = parent->handle; /* 修改叶节点的prev */                  \
                    extra->used   = ICLIST_USING;   /* 修改叶节点状态 正在使用 */      \
                    handle        = extra_pos;      /* 要返回的插入位置 */                \
                    extra->handle = extra_pos;                                                    \
                    extra->hash   = hash;                                                         \
                    break;                                                                        \
                }                                                                                 \
            }                                                                                     \
            /* 循环深度达到最大值，意味数据表extra已满 */                        \
            if (extra_pos == _type##_extra_size) {                                                \
                trace_debug("iclist_##_type##add error:table extra full\n");                      \
                handle = ICLIST_INVALID_HANDLE;                                                   \
                goto err_occured;                                                                 \
            }                                                                                     \
        } else {                                                                                  \
            trace_debug("iclist_##_type##add error:table used status incorrect\n");               \
            handle = ICLIST_INVALID_HANDLE;                                                       \
        }                                                                                         \
        pt->size++;                                                                               \
    err_occured:                                                                                  \
        iclist_unlock(pt);                                                                        \
        return handle;                                                                            \
    }                                                                                             \
    extern void iclist_##_type##_del(_type* pt, unsigned int handle) {                            \
        _item* data = pt->data + handle;                                                          \
        iclist_lock(pt);                                                                          \
        /* 1. data正在使用 */                                                                 \
        if (data->used == ICLIST_USING) {                                                         \
            /* 双向链表 操作删除data节点 */                                             \
            if (data->prev > 0) {                                                                 \
                /* data在链表中间, 更新父节点next=data.next, 子节点prev=data.prev */ \
                _item* parent = pt->data + data->prev;                                            \
                _item* child  = pt->data + data->next;                                            \
                parent->next  = data->next;                                                       \
                child->prev   = data->prev;                                                       \
                data->used    = ICLIST_NOTUSE;                                                    \
            } else {                                                                              \
                /* data是根节点 */                                                            \
                if (data->next == 0) {                                                            \
                    /* 没有子节点 设置 未使用*/                                         \
                    data->used = ICLIST_NOTUSE;                                                   \
                } else {                                                                          \
                    /* 有子节点, 设置 haschild */                                           \
                    data->used = ICLIST_HASCHILD;                                                 \
                }                                                                                 \
            }                                                                                     \
            pt->size--;                                                                           \
        }                                                                                         \
        iclist_unlock(pt);                                                                        \
    }                                                                                             \
    _type* iclist_##_type##_alloc(_type##_release_callback cb) {                                  \
        _type* pt = (_type*) CoTaskMemAlloc(sizeof(_type));                                       \
        memset(pt, 0, sizeof(_type));                                                             \
        pt->release = cb;                                                                         \
        return pt;                                                                                \
    }                                                                                             \
    void iclist_##_type##_free(_type* pt) {                                                       \
        iclist_##_type##_clear(pt);                                                               \
        CoTaskMemFree(pt);                                                                        \
    }                                                                                             \
    unsigned int iclist_##_type##_hash(const void* data, unsigned int len, unsigned int hval) {   \
        unsigned int hash;                                                                        \
        if (hval == 0)                                                                            \
            hval = eal_hash32_fnv1a(data, len);                                                   \
        else                                                                                      \
            hval = eal_hash32_fnv1a_more(data, len, hval);                                        \
        hash = eal_hash32_ton(hval, _type##_data_size);                                           \
        return hash;                                                                              \
    }                                                                                             \
    void iclist_##_type##_clear(_type* pt) {                                                      \
        unsigned int handle;                                                                      \
        for (handle = 0; handle < _type##_capacity; ++handle) {                                   \
            _item* data = iclist_data(pt, handle);                                                \
            iclist_##_type##_del(pt, handle);                                                     \
            /* 调用释放资源回调函数 */                                                  \
            if (pt->release) pt->release(pt, data);                                               \
        }                                                                                         \
    }

#define iclist_alloc(_type, pt, cb)                \
    (pt) = (_type*) CoTaskMemAlloc(sizeof(_type)); \
    memset((pt), 0, sizeof(_type));                \
    pt->release = cb;

#define iclist_free(_type, pt) \
    iclist_clear(_type, pt);   \
    CoTaskMemFree(pt)

#define iclist_lock(pt) eal_ticket_lock(&pt->lock)
#define iclist_trylock(pt) eal_ticket_trylock(&pt->lock)
#define iclist_unlock(pt) eal_ticket_unlock(&pt->lock)

#define iclist_clear(_type, pt)                                 \
    do {                                                        \
        unsigned int handle;                                    \
        for (handle = 0; handle < _type##_capacity; ++handle) { \
            _item* data = iclist_data(pt, handle);              \
            iclist_##_type##_del(pt, handle);                   \
            /* 调用释放资源回调函数 */                \
            if (pt->release) pt->release(pt, data);             \
        }                                                       \
    } while (0)

#define iclist_size(pt) ((pt)->size)
#define iclist_data(pt, hdl) ((pt)->data + (hdl))

#endif /** OPCDA2_UTIL_LIST_H_ */
