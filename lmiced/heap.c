/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com>
 * \file heap.c
 * \brief 堆内存分配
 * \details 堆内存按照块进行分配； 资源分配字节对齐
 * \author He Hao
 * \version 1.0
 * \date 2021-05-11
 * \since
 * \pre 无先决条件
 * \bug 未发现
 * \warning 无警告
 */
#include "heap.h"

#include <windows.h>

#include "lmice_private.h"

/* 自旋锁 */
#include "../opcda2/util_atomic.h"
#include "../opcda2/util_ticketlock.h"
#include "../opcda2/util_trace.h"

#define FNODE_NONE 0

typedef struct heap_desc {
    miu32             lock;
    miu32             size[HEAP_COUNT];
    miu8*             ptr[HEAP_COUNT];
    HANDLE            hfile[HEAP_COUNT];
    struct heap_desc* next;
} heap_desc;

/** free_node
 * \brief bidirected link free node list
 *
 * |0 -- heap size bytes -- size|
 * | heap summary | free 0 | node 0 | node 1 | free 1 | node 2 | free 3 |
 */
typedef struct free_node {
    miu32 prev_pos; /* 上一条可分配空间 */
    miu32 next_pos; /* 下一条可分配空间 */
    miu32 heap_pos; /* 堆中相对位置（字节数） */
    miu32 size;     /* 可分配空间（字节数） */
} free_node;

/** heap summary */
typedef struct heap_summary {
    miu32 id; /* heap id */
    miu32 size;
    miu32 lock;
    miu32 fnode; /* free_node 根节点 */
} heap_summary;

static __inline void  node_free_raw(heap_summary* hs, miu32 heap_pos, miu32 aligned_size);
static __inline miu8* node_malloc_raw(heap_summary* hs, miu32 aligned_size);
static __inline miu8* node_realloc_raw(heap_summary* hs, miu32 heap_pos, miu32 aligned_size, node_head* head,
                                       miu32 n_size);

static __inline void  miheap_report_raw(heap_desc* desc);
static __inline void* miheap_malloc_raw(heap_desc* desc, miu32 data_size);
static __inline void  miheap_free_raw(heap_desc* desc, void* ptr);
static __inline void* miheap_realloc_raw(heap_desc* desc, void* ptr, miu32 data_size);

static __inline int create_mapview(miu32 id, miu32 size, HANDLE* pfile, miu8** pptr, miu32* psize) {
    HANDLE      hfile        = NULL;
    miu8*       ptr          = NULL;
    char        name[32]     = {0};
    const miu32 id_pos       = sizeof(HEAP_NAME) - 1;
    int         ret          = 0;
    miu32       aligned_size = 0;

    /* 初始化 */
    *pfile = NULL;
    *pptr  = NULL;
    *psize = 0;

    if (size <= HEAP_DEFAULT_SIZE) {
        aligned_size = HEAP_DEFAULT_SIZE;
    } else if (size >= HEAP_MAX_SIZE) {
        aligned_size = HEAP_MAX_SIZE;
    } else {
        aligned_size = ((size + HEAP_ALIGN - 1) / HEAP_ALIGN) * HEAP_ALIGN;
    }

    trace_debug("create_mapview[%u] size %d aligned_size %u\n", id, size, aligned_size);

    memset(name, 0, sizeof(name));
    memcpy(name, HEAP_NAME, id_pos);
    name[id_pos + 0] = '0' + (id / 10000) % 10;
    name[id_pos + 1] = '0' + (id / 1000) % 10;
    name[id_pos + 2] = '0' + (id / 100) % 10;
    name[id_pos + 3] = '0' + (id / 10) % 10;
    name[id_pos + 4] = '0' + id % 10;

    hfile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT | SEC_LARGE_PAGES, 0,
                               aligned_size, name);
    if (hfile == NULL) {
        /* 创建失败 */
        DWORD err = GetLastError();
        trace_debug("CreateFileMappingA(SEC_LARGE_PAGES:%u) failed %d\n", aligned_size, err);
        /* 尝试非Large_pages */
        hfile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT, 0, aligned_size, name);
    }

    if (hfile == NULL) {
        DWORD err = GetLastError();
        trace_debug("CreateFileMappingA(%u) failed %d\n", aligned_size, err);
        ret = 1;
    } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /*已经存在，读取heap summary以获取长度*/
        trace_debug("CreateFileMappingA already exist\n");
        ptr = MapViewOfFile(hfile, FILE_MAP_READ, 0, 0, sizeof(heap_summary));
        if (ptr != NULL) {
            heap_summary* heap;
            heap         = (heap_summary*) ptr;
            aligned_size = heap->size;

            UnmapViewOfFile(ptr);

            ptr = MapViewOfFile(hfile, FILE_MAP_ALL_ACCESS, 0, 0, aligned_size);
        }
    } else {
        /* 新增， 更新 heap summary ; free_node 状态 */
        ptr = MapViewOfFile(hfile, FILE_MAP_ALL_ACCESS, 0, 0, aligned_size);
        if (ptr != NULL) {
            heap_summary* heap;
            free_node*    fnode;

            /* 更新heap 状态 */
            heap        = (heap_summary*) ptr;
            heap->id    = id;
            heap->size  = aligned_size;
            heap->fnode = sizeof(heap_summary);

            /* free node 根节点 */
            fnode           = (free_node*) (ptr + heap->fnode);
            fnode->prev_pos = 0;
            fnode->next_pos = 0;
            fnode->heap_pos = heap->fnode;
            fnode->size     = aligned_size - sizeof(heap_summary);
        }
    }
    if (ret == 0) trace_debug("create_mapview[%p] at name %s\n", hfile, name);

    if (ptr == NULL && hfile != NULL) {
        ret = 1;
        CloseHandle(hfile);
    }

    /* 设置返回值 */
    *pfile = hfile;
    *pptr  = ptr;
    *psize = aligned_size;
    trace_debug("open heap(%d) free size %u\n", ret, aligned_size);

    return ret;
}

mihandle miheap_create(miu32 size) {
    mihandle   handle;
    HANDLE     hfile = NULL;
    miu8*      ptr   = NULL;
    heap_desc* desc;
    int        ret;
    miu32      nsize;

    handle.ptr = NULL;

    ret = create_mapview(0, size, &hfile, &ptr, &nsize);
    if (ret == 0) {
        /* 创建内存成功, 初始化heap描述 */
        desc           = (heap_desc*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(heap_desc));
        desc->size[0]  = nsize;
        desc->ptr[0]   = ptr;
        desc->hfile[0] = hfile;

        handle.ptr = (void*) desc;
    }

    return handle;
}

void miheap_release(mihandle heap) {
    heap_desc* desc;
    heap_desc* next;
    next = (heap_desc*) heap.ptr;
    while (next != NULL) {
        int i;
        desc = next;
        for (i = 0; i < HEAP_COUNT; ++i) {
            if (desc->ptr[i]) UnmapViewOfFile(desc->ptr[i]);
            if (desc->hfile[i]) CloseHandle(desc->hfile[i]);
        }
        next = desc->next;
        HeapFree(GetProcessHeap(), 0, desc);
    }
}

static __inline miu8* miheap_malloc_at_fnode(miu8* heap_begin, free_node* fnode, miu32 aligned_size) {
    heap_summary* hs          = (heap_summary*) (heap_begin);
    miu32         remain_size = fnode->size - aligned_size;
    node_head*    node        = (node_head*) fnode;
    miu32         heap_pos    = fnode->heap_pos;
    /* 更新链表 */
    /* 剩余空间不足 */
    if (remain_size < sizeof(node_head)) {
        /* 修订分配 size */
        aligned_size = fnode->size;

        /* 更新链表 */
        if (fnode->next_pos != 0 && fnode->prev_pos != 0) {
            /* 同时存在 prev, next 节点的时候 */
            free_node* prev;
            free_node* next;

            prev = (free_node*) (heap_begin + fnode->prev_pos);
            next = (free_node*) (heap_begin + fnode->next_pos);

            prev->next_pos = next->heap_pos;
            next->prev_pos = prev->prev_pos;
        } else if (fnode->prev_pos != 0 && fnode->next_pos == 0) {
            /* fnode 为叶节点 存在 prev 节点的时候 */
            free_node* prev;

            prev           = (free_node*) (heap_begin + fnode->prev_pos);
            prev->next_pos = 0;
        }

        if (hs->fnode == fnode->heap_pos) {
            /* fnode为根节点, fnode->next 为新的根节点 */
            hs->fnode = fnode->next_pos;
            if (fnode->next_pos != 0) {
                free_node* next;

                next = (free_node*) (heap_begin + fnode->next_pos);

                next->prev_pos = 0;
            }
        }

    } else {
        /* 插入新的freenode */
        free_node* new_node;

        new_node = (free_node*) (heap_begin + fnode->heap_pos + aligned_size);

        new_node->heap_pos = fnode->heap_pos + aligned_size;
        new_node->size     = remain_size;
        /* 更新freenode链表 */
        if (fnode->prev_pos != 0 && fnode->next_pos != 0) {
            /* fnode 为中间节点，同时存在 prev, next 节点 */
            free_node* prev;
            free_node* next;

            prev = (free_node*) (heap_begin + fnode->prev_pos);
            next = (free_node*) (heap_begin + fnode->next_pos);

            prev->next_pos     = new_node->heap_pos;
            next->prev_pos     = new_node->prev_pos;
            new_node->prev_pos = prev->heap_pos;
            new_node->next_pos = next->heap_pos;
        } else if (fnode->prev_pos != 0 && fnode->next_pos == 0) {
            /* fnode 为叶节点，存在 prev 节点 */
            free_node* prev;

            prev = (free_node*) (heap_begin + fnode->prev_pos);

            prev->next_pos     = new_node->heap_pos;
            new_node->prev_pos = prev->heap_pos;
            new_node->next_pos = 0;
        }

        if (hs->fnode == fnode->heap_pos) {
            /* fnode为根节点 更新 新的根节点位置 */
            new_node->prev_pos = 0;
            hs->fnode          = new_node->heap_pos;

            new_node->prev_pos = 0;
            new_node->next_pos = fnode->next_pos;
            if (fnode->next_pos != 0) {
                free_node* next;

                next = (free_node*) (heap_begin + fnode->next_pos);

                next->prev_pos = new_node->heap_pos;
            }
        }
    }
    /* 更新节点信息 */
    node->heap_id = hs->id;
    node->id      = heap_pos / HEAP_DATA_ALIGN;
    node->size    = aligned_size;
    node->lock    = 0;
#ifdef EAL_INT64
    node->ctick.i64.high = 0;
    node->ctick.i64.low  = 0;
    node->mtick.i64.high = 0;
    node->mtick.i64.low  = 0;
    node->atick.i64.high = 0;
    node->atick.i64.low  = 0;
#else
    node->ctick = 0;
    node->mtick = 0;
    node->atick = 0;
#endif
    return (((miu8*) node) + sizeof(node_head));
}

miu8* node_malloc_raw(heap_summary* hs, miu32 aligned_size) {
    miu8*      ptr        = NULL;
    miu8*      heap_begin = (miu8*) hs;
    free_node* fnode      = (free_node*) (heap_begin + hs->fnode);
    /* 当前堆无空闲空间 查询下一个 heap */
    if (hs->fnode == FNODE_NONE) {
        return ptr;
    }

    /* 从根节点遍历heap上空闲空间链表 */
    for (;;) {
        if (fnode->size >= aligned_size) {
            /* 更新节点状态 */
            trace_debug("malloc %u, size:%u, prev:%u next:%u\n", fnode->heap_pos, aligned_size, fnode->prev_pos,
                        fnode->next_pos);
            ptr = miheap_malloc_at_fnode(heap_begin, fnode, aligned_size);
            /* 分配成功，退出循环 */
            break;
        }

        /* 已经是叶节点，退出 */
        if (fnode->next_pos == 0) break;

        fnode = (free_node*) (heap_begin + fnode->next_pos);
    }
    return ptr;
}

void* miheap_malloc_raw(heap_desc* desc, miu32 data_size) {
    heap_summary* hs;
    miu32         aligned_size; /* 真实分配内存字节数 */
    int           heap_idx = 0; /* 堆 索引 */
    miu8*         ptr      = NULL;

    aligned_size = ((data_size + sizeof(node_head) + HEAP_DATA_ALIGN - 1) / HEAP_DATA_ALIGN) * HEAP_DATA_ALIGN;

    /* find free space */
    do {
        int i;
        for (i = 0; i < HEAP_COUNT; ++i, ++heap_idx) {
            /* heap 检测打开 */
            if (desc->ptr[i] == NULL) {
                HANDLE hfile;
                miu8*  hptr;
                miu32  size;
                int    ret;

                /* 使用默认大小打开/新建内存 */
                ret = create_mapview(heap_idx, aligned_size, &hfile, &hptr, &size);
                if (ret == 0) {
                    desc->hfile[i] = hfile;
                    desc->ptr[i]   = hptr;
                    desc->size[i]  = size;
                }
            }
            hs = (heap_summary*) desc->ptr[i];

            /* 无法分配内存 */
            if (hs == NULL) return NULL;

            EAL_ticket_lock(&hs->lock);
            ptr = node_malloc_raw(hs, aligned_size);
            EAL_ticket_unlock(&hs->lock);

            if (ptr) break;
        } /** end-of for i--HEAP_COUNT */

        /* 已经插入成功，退出循环 */
        if (ptr) break;

        /* 遍历heap链表 */
        if (desc->next == NULL) {
            heap_desc* next_desc;
            next_desc  = (heap_desc*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(heap_desc));
            desc->next = next_desc;
        }
        desc = desc->next;
    } while (desc != NULL);

    return ptr;
}

/**
 * \brief 查找 数据区 对应的 heap指针与位置
 */
static __inline int find_heap(heap_desc* desc, miu8* ptr, miu8** heap_begin, miu32* heap_pos, miu32* aligned_size) {
    node_head*    node;
    heap_summary* hs;
    miu32         heap_id;
    int           ret = 0;

    node          = (node_head*) (ptr - sizeof(node_head));
    *heap_pos     = node->id * HEAP_DATA_ALIGN;
    *aligned_size = node->size;
    heap_id       = node->heap_id;

    for (; heap_id >= HEAP_COUNT; heap_id -= HEAP_COUNT) {
        if (desc->next == NULL) {
            ret = 1;
            break;
        }
        desc = desc->next;
    }
    if (ret == 0) {
        *heap_begin = desc->ptr[heap_id];
        hs          = (heap_summary*) desc->ptr[heap_id];
        if (hs == NULL) {
            ret = 1;
        } else if (hs->size < (*heap_pos)) {
            ret = 1;
        }
    }

    return ret;
}

/** find_prev_next
 * @brief 查找相邻的空节点
 * @param heap_begin
 * @param heap_pos
 * @param aligned_size
 * @param pprev
 * @param pnext
 */
static __inline void find_prev_next(miu8* heap_begin, miu32 heap_pos, miu32 aligned_size, free_node** pprev,
                                    free_node** pnext) {
    free_node*    fnode = NULL;
    free_node*    prev  = NULL;
    free_node*    next  = NULL;
    heap_summary* hs;
    hs    = (heap_summary*) heap_begin;
    fnode = (free_node*) (heap_begin + hs->fnode);
    if (hs->fnode != FNODE_NONE) {
        for (;;) {
            if (fnode->heap_pos + fnode->size <= heap_pos) {
                /* 前方fnode */
                prev = fnode;
            } else if (next == NULL && fnode->heap_pos >= heap_pos + aligned_size) {
                /* 后方fnode， 退出 */
                next = fnode;
                break;
            }
            /* fnode为叶节点 退出 */
            if (fnode->next_pos == 0) break;

            fnode = (free_node*) (heap_begin + fnode->next_pos);
        }
    }
    *pprev = prev;
    *pnext = next;
}

void node_free_raw(heap_summary* hs, miu32 heap_pos, miu32 aligned_size) {
    miu8* heap_begin = (miu8*) hs;
    if (hs->fnode == FNODE_NONE) {
        free_node* fnode;
        /* 无根节点  新建根节点 */
        fnode           = (free_node*) (heap_begin + heap_pos);
        fnode->heap_pos = heap_pos;
        fnode->next_pos = 0;
        fnode->prev_pos = 0;
        fnode->size     = aligned_size;
        /* 更新堆状态信息 */
        hs->fnode = heap_pos;
    } else {
        /* 链表查找node前后方是否是空节点 */
        int        is_ok = 0;
        free_node* prev  = NULL;
        free_node* next  = NULL;
        find_prev_next(heap_begin, heap_pos, aligned_size, &prev, &next);

        printf("free_prev_next %p ", heap_begin);
        if (prev)
            printf(" prev %08u %08u [%u,%u]",                   /*prev data*/
                   prev->heap_pos, prev->heap_pos + prev->size, /*prev pos end*/
                   prev->prev_pos, prev->next_pos               /*prev prev-next*/
            );
        printf("--node %08u %08u--", heap_pos, heap_pos + aligned_size);
        if (next)
            printf(" next %08u %08u [%u, %u]",                  /*next data*/
                   next->heap_pos, next->heap_pos + next->size, /*next pos end*/
                   next->prev_pos, next->next_pos               /* next prev-next */
            );
        printf("\n");

        /* prev-node-next 相连， 则合并==>prev */
        if (prev && next && is_ok == 0) {
            if (prev->heap_pos + prev->size == heap_pos && next->heap_pos == heap_pos + aligned_size) {
                prev->size     = prev->size + aligned_size + next->size;
                prev->next_pos = next->next_pos;

                is_ok = 1;

                if (next->next_pos != 0) {
                    free_node* nnext;
                    nnext = (free_node*) (heap_begin + next->next_pos);

                    nnext->prev_pos = prev->heap_pos;
                }
            }
        }

        /* prev-node 相连， 则合并 ==>prev */
        if (prev && is_ok == 0) {
            if (prev->heap_pos + prev->size == heap_pos) {
                prev->size = prev->size + aligned_size;

                is_ok = 1;
            }
        }

        /* node-next 相连， 则合并 ==>nnode */
        if (next && is_ok == 0) {
            if (next->heap_pos == heap_pos + aligned_size) {
                free_node* nnode;
                nnode = (free_node*) (heap_begin + heap_pos);

                nnode->prev_pos = next->prev_pos;
                nnode->next_pos = next->next_pos;
                nnode->heap_pos = heap_pos;
                nnode->size     = next->size + aligned_size;

                is_ok = 1;

                if (next->prev_pos != 0) {
                    free_node* nprev;
                    nprev = (free_node*) (heap_begin + next->prev_pos);

                    nprev->next_pos = heap_pos;
                } else {
                    /*next 为根节点 更新summary*/
                    hs->fnode = heap_pos;
                }

                if (next->next_pos != 0) {
                    free_node* nnext;
                    nnext = (free_node*) (heap_begin + next->next_pos);

                    nnext->prev_pos = heap_pos;
                }
            }
        }

        /* 均不相连 插入新的空闲空间节点 ==>nnode */
        if (is_ok == 0) {
            free_node* nnode;
            nnode = (free_node*) (heap_begin + heap_pos);

            nnode->prev_pos = 0;
            nnode->next_pos = 0;
            nnode->heap_pos = heap_pos;
            nnode->size     = aligned_size;

            printf("insert new free node %u %u\n", heap_pos, aligned_size);

            if (prev) {
                prev->next_pos  = heap_pos;
                nnode->prev_pos = prev->heap_pos;
            } else {
                /* 这是根节点 */
                hs->fnode = heap_pos;
            }

            if (next) {
                next->prev_pos  = heap_pos;
                nnode->next_pos = next->heap_pos;
            }
        }
    }
}

void miheap_free_raw(heap_desc* desc, void* ptr) {
    miu32         heap_pos;
    miu8*         heap_begin;
    miu32         aligned_size;
    int           ret;
    heap_summary* hs;

    /* 获取heap */
    ret = find_heap(desc, ptr, &heap_begin, &heap_pos, &aligned_size);
    if (ret != 0) {
        trace_debug("miheap_free invalid pointer %p\n", ptr);
        return;
    }

    hs = (heap_summary*) (heap_begin);

    EAL_ticket_lock(&hs->lock);
    node_free_raw(hs, heap_pos, aligned_size);
    EAL_ticket_unlock(&hs->lock);
}

miu8* node_realloc_raw(heap_summary* hs, miu32 heap_pos, miu32 aligned_size, node_head* head, miu32 n_size) {
    miu8*      heap_begin = (miu8*) hs;
    miu8*      ptr        = heap_begin + heap_pos;
    free_node* prev       = NULL;
    free_node* next       = NULL;
    node_head* nhead      = NULL;
    miu8*      nptr       = NULL;

    find_prev_next(heap_begin, heap_pos, aligned_size, &prev, &next);
    if (next) {
        if (next->heap_pos == heap_pos + aligned_size && next->size + aligned_size >= n_size) {
            /* 合并 node-next --> nnode */
            free_node* nprev;
            free_node* nnode;

            nnode = (free_node*) (heap_begin + heap_pos);

            nnode->size     = next->size + aligned_size;
            nnode->heap_pos = heap_pos;
            nnode->prev_pos = next->prev_pos;
            nnode->next_pos = next->next_pos;

            if (nnode->prev_pos != 0) {
                nprev = (free_node*) (heap_begin + nnode->prev_pos);

                nprev->next_pos = nnode->heap_pos;
            } else {
                hs->fnode = nnode->heap_pos;
            }

            nptr  = miheap_malloc_at_fnode(heap_begin, nnode, n_size);
            nhead = (node_head*) (nptr - sizeof(node_head));

            nhead->ctick = head->ctick;
            nhead->mtick = head->mtick;
            nhead->atick = head->atick;

            return nptr;
        }
    }

    if (prev && next) {
        if (prev->heap_pos + prev->size == heap_pos && next->heap_pos == heap_pos + aligned_size &&
            prev->size + aligned_size + next->size >= n_size) {
            free_node* nnext;
            free_node* nnode;

            /* 合并 prev-node-next --> nnode */
            nnode           = prev;
            nnode->size     = prev->size + aligned_size + next->size;
            nnode->next_pos = next->next_pos;

            if (next->next_pos != 0) {
                nnext = (free_node*) (heap_begin + next->next_pos);

                nnext->prev_pos = nnode->heap_pos;
            }

            /* 分配内存 */
            nptr  = miheap_malloc_at_fnode(heap_begin, nnode, n_size);
            nhead = (node_head*) (nptr - sizeof(node_head));

            nhead->ctick = head->ctick;
            nhead->mtick = head->mtick;
            nhead->atick = head->atick;

            /* 移动数据内容 */
            memcpy(nptr, ptr, aligned_size - sizeof(node_head));

            return nptr;
        }
    }

    if (prev) {
        if (prev->heap_pos + prev->size == heap_pos && prev->size + aligned_size >= n_size) {
            free_node* nnode;

            /* 合并 prev-node -->nnode */
            nnode       = prev;
            nnode->size = prev->size + aligned_size;
            /* 分配内存 */
            nptr = miheap_malloc_at_fnode(heap_begin, nnode, n_size);
            /* 更新节点内容 */
            nhead = (node_head*) (nptr - sizeof(node_head));

            nhead->ctick = head->ctick;
            nhead->mtick = head->mtick;
            nhead->atick = head->atick;

            /* 移动数据内容 */
            memcpy(nptr, ptr, aligned_size - sizeof(node_head));

            return nptr;
        }
    }
    return nptr;
}

void* miheap_realloc_raw(heap_desc* desc, void* ptr, miu32 data_size) {
    miu32         heap_pos;
    miu8*         heap_begin;
    miu32         aligned_size;
    int           ret;
    miu8*         nptr;
    miu32         n_size;
    node_head     head;
    node_head*    nhead = NULL;
    heap_summary* hs    = NULL;

    /* 获取heap */
    ret = find_heap(desc, ptr, &heap_begin, &heap_pos, &aligned_size);
    if (ret != 0) return NULL;

    n_size = ((data_size + sizeof(node_head) + HEAP_DATA_ALIGN - 1) / HEAP_DATA_ALIGN) * HEAP_DATA_ALIGN;

    /* 1 空闲空间满足要求 */
    if (n_size <= aligned_size) return ptr;

    /* 2 查找相临空闲节点，进行分配 */
    hs = (heap_summary*) heap_begin;
    memcpy(&head, heap_begin + heap_pos, sizeof(node_head));
    EAL_ticket_lock(&hs->lock);
    nptr = node_realloc_raw(hs, heap_pos, aligned_size, &head, n_size);
    EAL_ticket_unlock(&hs->lock);
    if (nptr) return nptr;

    /* 3 在堆上重新分配数据 */
    nptr = miheap_malloc_raw(desc, n_size);

    /* 分配失败 */
    if (nptr == NULL) return NULL;

    memset(nptr, 0, n_size);
    /* 移动数据内容 */
    memcpy(nptr, ptr, aligned_size - sizeof(node_head));

    nhead = (node_head*) (nptr - sizeof(node_head));
    /* 更新节点内容 */
    nhead->ctick = head.ctick;
    nhead->mtick = head.mtick;
    nhead->atick = head.atick;
    /* 移动数据内容 */
    memcpy(nptr, ptr, aligned_size - sizeof(node_head));

    /* 释放现有内存 */
    miheap_free_raw(desc, ptr);

    return nptr;
}

void miheap_report_raw(heap_desc* desc) {
    miu32 heap_idx = 0;
    while (desc != NULL) {
        int i;
        for (i = 0; i < HEAP_COUNT; ++i, ++heap_idx) {
            heap_summary* hs;
            miu8*         heap_begin;
            free_node*    fnode;
            /* heap 没打开 */
            if (desc->ptr[i] == NULL) break;

            hs         = (heap_summary*) desc->ptr[i];
            heap_begin = desc->ptr[i];
            printf("heap %u %p, size: %u\t", heap_idx, (void*) hs, hs->size);

            EAL_ticket_lock(&hs->lock);
            fnode = (free_node*) (heap_begin + hs->fnode);
            if (hs->fnode == FNODE_NONE) {
                printf("no free node\n");
            } else {
                int j = 0;
                printf("first free node %u\n", hs->fnode);
                for (;; ++j) {
                    printf(
                        "  fnode[%02d] pos[%08d]"                     /* fnode info */
                        ", size[%08d] [%u,%u] \n",                    /* fnode detail*/
                        j + 1, fnode->heap_pos,                       /*info val*/
                        fnode->size, fnode->prev_pos, fnode->next_pos /* detail val */
                    );
                    if (fnode->next_pos == 0) break;
                    fnode = (free_node*) (heap_begin + fnode->next_pos);
                }
            }
            EAL_ticket_unlock(&hs->lock);
        }
        desc = desc->next;
    }

    printf("heap block count = %u\n", heap_idx);
}

void miheap_report(mihandle heap) {
    heap_desc* desc = (heap_desc*) heap.ptr;

    EAL_ticket_lock(&desc->lock);
    miheap_report_raw(desc);
    EAL_ticket_unlock(&desc->lock);
}

void miheap_info(mihandle heap, void* p) {
    miu8*      ptr  = (miu8*) p;
    node_head* node = (node_head*) (ptr - sizeof(node_head));
    printf("node %p :\tpos:%u\tsize:%u\n", p, node->id * HEAP_DATA_ALIGN, node->size);
}

void* miheap_malloc(mihandle heap, miu32 data_size) {
    void*      ptr;
    heap_desc* desc;
    desc = (heap_desc*) heap.ptr;

    EAL_ticket_lock(&desc->lock);
    ptr = miheap_malloc_raw(desc, data_size);
    EAL_ticket_unlock(&desc->lock);

    return ptr;
}

void miheap_free(mihandle heap, void* ptr) {
    heap_desc* desc;
    desc = (heap_desc*) heap.ptr;

    EAL_ticket_lock(&desc->lock);
    miheap_free_raw(desc, ptr);
    EAL_ticket_unlock(&desc->lock);
}

void* miheap_realloc(mihandle heap, void* ptr, miu32 data_size) {
    void*      nptr;
    heap_desc* desc;
    desc = (heap_desc*) heap.ptr;

    EAL_ticket_lock(&desc->lock);
    nptr = miheap_realloc_raw(desc, ptr, data_size);
    EAL_ticket_unlock(&desc->lock);

    return nptr;
}

miu8* node_open_raw(heap_summary* hs, miu32 node_id) {
    miu8* heap_begin = (miu8*) hs;
    miu32 heap_pos   = node_id * HEAP_DATA_ALIGN;
    return heap_begin + heap_pos + sizeof(node_head);
}

miu8* miheap_open_raw(heap_desc* desc, miu32 heap_id, miu32 node_id) {
    miu8*         ptr      = NULL;
    miu32         heap_idx = 0;
    HANDLE        hfile;
    miu8*         hptr;
    miu32         size;
    int           ret;
    heap_summary* hs;

    /* find node */
    for (; heap_id >= HEAP_COUNT; heap_id -= HEAP_COUNT) {
        if (desc->next == NULL) {
            heap_desc* next_desc;
            next_desc  = (heap_desc*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(heap_desc));
            desc->next = next_desc;
        }
        desc = desc->next;
    }

    hfile = desc->hfile[heap_id];
    if (hfile == NULL) {
        /* 使用默认大小打开/新建内存 */
        ret = create_mapview(heap_idx, 0, &hfile, &hptr, &size);
        if (ret == 0) {
            desc->hfile[heap_id] = hfile;
            desc->ptr[heap_id]   = hptr;
            desc->size[heap_id]  = size;
        }
    }

    hs = (heap_summary*) desc->ptr[heap_id];
    /* 查找节点 */
    if (hs != NULL) ptr = node_malloc_raw(hs, node_id);

    return ptr;
}

void* miheap_open(mihandle heap, miu32 heap_id, miu32 node_id) {
    void*      nptr;
    heap_desc* desc;
    desc = (heap_desc*) heap.ptr;
    EAL_ticket_lock(&desc->lock);
    nptr = miheap_open_raw(desc, heap_id, node_id);
    EAL_ticket_unlock(&desc->lock);

    return nptr;
}
