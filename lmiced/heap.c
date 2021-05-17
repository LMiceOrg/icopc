/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com>
 * \file heap.c
 * \brief 堆内存分配
 * \details 堆内存按照块进行分配，使用。默认块大小64kB； 资源分配以64字节对齐
 * heap_summary | block_bitmap | res_index | data ...
 * block-0 第一个64kB，资源类型RES_HEAP, ID(0)
 * |0 --    26687|26688 -- 65535|
 * |HEAP summary |  HEAP64      |
 * |... 26688 ... |
 * |0 --64--  63|64--2048--2111|2112 --48*512-- 26687|
 * |heap_summary| block_bitmap | res_index           |
   |... 38848 ... | 资源类型RES_HEAP64, ID(417+33+i)
 * |0 --64-- 63 |64 --2048-- 2111|2112 --48*328-- 17855|17856 --64*328 -- 38847|
 * |heap_summary| block_bitmap   | res_index           | 64B data block        |
 *
 * block-n 资源类型RES_HEAP64, ID(1024*n+33+i)
 * |0 --64--  63|64--2048--2111|2112 --48*566-- 29279|29280 --64*566-- 65503|
 * |heap_summary| block_bitmap |    res_index        |      64B data block  |
 *
 * block-n:m 资源类型RES_DATA, ID(1024*n)
 * |0 -- 65536*m-- |
 * | data block    |
 *
 * block-n 资源类型RES_INDEX，ID(1024*n)
 * |0 --48*1356-- 65535|
 * | res_index         |
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

/* 自旋锁 */
#include "../opcda2/util_ticketlock.h"

/* heap 名称模版 */
#define HEAP_NAME "Global\\smLMiced"

#define HEAP_COUNT 8
#define HEAP_BLOCK_SIZE 65536
#define HEAP_MAX_SIZE (1024 * 1024 * 1024)
#define HEAP_DEFAULT_SIZE (10 * 1024 * 1024)

typedef struct heap_desc {
    miu32 lock;
    miu32             size[HEAP_COUNT];
    miu8*             ptr[HEAP_COUNT];
    HANDLE            hfile[HEAP_COUNT];
    struct heap_desc* next;
} heap_desc;


#if 0
typedef int heap_pos;

typedef struct {
    int      size;             /**< 字节数 */
    int      lock;             /**< 原子锁 */
    int      heap_no;          /**< 堆 编号 */
    int      block_size;       /**< 每块字节数 */
    int      block_count;      /**< 总共内存块数 */
    int      free_block_count; /**< 空闲块数 */
    int      res_id_base;      /**< 资源id基值 */
    int      res_count;        /**< 资源数量 */
    int      free_res_count;
    heap_pos block_bitmap; /**< 指向 块位图 */
    heap_pos res_table;    /**< 指向 资源表 */
    heap_pos res_data;     /**< 指向 数据 */
} heap_summary;

typedef enum {
    RES_DATA,   /**< 数据 资源 */
    RES_HEAP,   /**< 堆 资源 */
    RES_HEAP64, /**< 小内存堆 */
    RES_INDEX,  /**< 索引表 资源 */
    RES_BITMAP  /**< 块使用情况位图 */
} HEAP_RES_TYPE;

typedef struct {
    int     size;      /**< 字节数 */
    int     lock;      /**< 原子锁 */
    int     data_size; /**< 数据字节数 */
    miint16 mode;      /**< 访问权限 */
    miint16 uid;       /**< 所有者id */
    mitick  ctime;     /**< 创建时间 */
    mitick  mtime;     /**< 修改时间 */
    mitick  atime;     /**< 访问时间 */
} heap_res_summary;

typedef struct {
    int      res_id;    /**<  资源id */
    miuchar8 res_type;  /**< 资源类型 */
    miuchar8 mode;      /**< 访问权限 */
    miint16  uid;       /**< 所有者id */
    int      size;      /**< 字节数 */
    int      lock;      /**< 原子锁 */
    int      data_size; /**< 数据字节数 */
    int      data_type; /**< 数据类型 */

    mitick ctick; /**< 创建节拍 */
    mitick mtick; /**< 修改节拍 */
    mitick atick; /**< 访问节拍 */
} heap_res_index;

/** 8字节对齐的 使用情况位图 大小
 * @param block_count 总共块数量
 */
#define bitmap_size(block_count) ((((int) block_count) + 63) / 64) * 8

void bitmap_set(void* bitmap, int begin, int size) {
    int ipos    = begin / 32;
    int iremain = begin % 32;
    for (; size > 0;) {
        int           step = 32 - iremain;
        unsigned int  mask = ((1 << step) - 1) << iremain;
        unsigned int* p    = (unsigned int*) bitmap + ipos;
        if (step < size) {
            iremain = 0;
            *p |= mask;
            size -= step;
        } else {
            step = size;
            mask = ((1 << step) - 1) << iremain;
            *p |= mask;
            break;
        }
        ipos++;
    }
}

void bitmap_unset(void* bitmap, int begin, int size) {
    int ipos    = begin / 32;
    int iremain = begin % 32;
    for (; size > 0;) {
        int           step = 32 - iremain;
        unsigned int  mask = 0xffffffff >> step;
        unsigned int* p    = (unsigned int*) bitmap + ipos;

        if (step < size) {
            iremain = 0;
            *p &= mask;
            size -= step;
        } else {
            step = size;
            mask = 0xffffffff << step;
            *p &= mask;
        }
        ipos++;
    }
}

int bitmap_find(void* bitmap, int max_count, int size, int* begin) {
    unsigned int* p     = (unsigned int*) bitmap;
    int           count = 0;
    int           ipos;
    int           i;
    int           j;

    *begin = 0;

    for (i = 0; i < max_count; i += 32, p++) {
        for (j = 0; j < 32; ++j) {
            int value = *p & (1 << j);
            if (value == 1) {
                /* 已经使用，重新计数 */
                count = 0;
            } else {
                if (count == 0) {
                    ipos = i * 32 + j;
                }
                if (i >= max_count) {
                    return 1;
                }
                count++;
                if (count == size) {
                    *begin = ipos;
                    return 0;
                }
            }
        }
    }

    return 1;
}

mihandle miheap_create(int size) {
    heap_desc* desc;
    mihandle   handle;
    HANDLE     hfile;
    void*      ptr;
    int        block_count;

    if (size <= 0) {
        block_count = 1024;
    } else if (size > 1024 * 1024 * 1024) {
        block_count = 1024 * 1024;
    } else {
        block_count = ((size + HEAP_BLOCK_SIZE - 1) / HEAP_BLOCK_SIZE) * HEAP_BLOCK_SIZE;
    }

    size = HEAP_BLOCK_SIZE * block_count;

    hfile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT | SEC_LARGE_PAGES, 0, size,
                               HEAP_NAME);
    if (hfile == NULL) {
        handle.i64 = 0;
        return handle;
    } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
        ptr = MapViewOfFile(hfile, FILE_MAP_ALL_ACCESS, 0, 0, size);
    } else {
        heap_summary* summary;

        ptr = MapViewOfFile(hfile, FILE_MAP_ALL_ACCESS, 0, 0, size);

        summary       = (heap_summary*) (ptr);
        summary->size = size;
        /* summary->lock; */
        summary->block_size       = HEAP_BLOCK_SIZE;
        summary->block_count      = block_count;
        summary->free_block_count = summary->block_size - 1;
        /* summary->res_count; */
        summary->block_bitmap = sizeof(heap_summary);
        summary->res_table    = sizeof(heap_summary) + bitmap_size(summary->block_count);
    }

    desc           = (heap_desc*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(heap_desc));
    desc->ptr[0]   = ptr;
    desc->hfile[0] = hfile;
    desc->size     = size;

    handle.ptr = (void*) desc;

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
            if (desc->ptr) UnmapViewOfFile(desc->ptr);
            if (desc->hfile) CloseHandle(desc->hfile);
        }
        next = desc->next;
        HeapFree(GetProcessHeap(), 0, desc);
    }
}

void* miheap_malloc(mihandle heap, int size) {
    heap_desc* desc = (heap_desc*) (heap.ptr);
    void*      ptr  = NULL;

    if (!desc) return NULL;

    /* 默认分配大小 */
    if (size <= 0) size = HEAP_BLOCK_SIZE;

    if (size > 4096) {
        /* 从堆上分配 64kB的整数倍 */
        int block_count = (size + HEAP_BLOCK_SIZE - 1) / HEAP_BLOCK_SIZE;
    } else {
        /* 从64字节资源堆分配 */
        int block_count = (size + 64 - 1) / 64;
    }
    return ptr;
}
#endif

#define EMPTY_FREE_NODE NULL
#define HEAP_DATA_ALIGN 16

/** free_node
 * \brief bidirected link free node list
 *
 * |0 -- heap size bytes -- size|
 * | free 0 | node 1 | node 2 | free 1 | node 3 | free 3 |
 */
typedef struct free_node {
    miu32 prev_pos; /* 上一条可分配空间 */
    miu32 next_pos; /* 下一条可分配空间 */
    miu32 heap_pos; /* 堆中相对位置（字节数） */
    miu32 size; /* 可分配空间（字节数） */
} free_node;

/** heap summary */
typedef struct heap_summary {
    miu32 id; /* heap id */
    miu32 size;
    miu32 lock;
    miu32 fnode; /* free_node 根节点 */
} heap_summary;


#define FNODE_NONE 0

typedef struct node_head {
    miu32 heap_idx; /* 堆 id */
    miu32 id;      /* 资源id */
    miu32 size;    /* 对齐数据长度 */
    miu32 lock;
#if 0
    miu8   data_type; /**< 资源类型 */
    miu8   mode;      /**< 访问权限 */
    miu16  uid;       /**< 所有者id */
#endif
    mitick ctick;
    mitick mtick;
    mitick atick;
} node_head;

static __inline int create_mapview(miu32 id, int size, HANDLE* pfile, miu8** pptr, miu32* psize) {
    HANDLE      hfile        = NULL;
    miu8*       ptr          = NULL;
    char        name[32]     = {0};
    const miu32 id_pos       = sizeof(HEAP_NAME) - 1;
    int         ret          = 0;
    miu32       block_count  = 0;
    miu32       aligned_size = 0;

    /* 初始化 */
    *pfile = NULL;
    *pptr  = NULL;
    *psize = 0;

    if (size <= 0) {
        block_count = 1024;
    } else if (size > 1024 * 1024 * 1024) {
        block_count = 16 * 1024 * 1024;
    } else {
        block_count = ((size + HEAP_BLOCK_SIZE - 1) / HEAP_BLOCK_SIZE) * HEAP_BLOCK_SIZE;
    }

    aligned_size = HEAP_BLOCK_SIZE * block_count;

    memset(name, 0, sizeof(name));
    memcpy(name, HEAP_NAME, id_pos);
    name[id_pos+0] = '0' + (id/10000) % 10;
    name[id_pos+1] = '0' + (id/1000) % 10;
    name[id_pos+2] = '0' + (id/100) % 10;
    name[id_pos+3] = '0' + id/10;
    name[id_pos+4] = '0' + id % 10;


    hfile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE | SEC_COMMIT | SEC_LARGE_PAGES, 0, aligned_size,
                               name);
    if (hfile == NULL) {
        /* 创建失败 */
        ret = 1;
    } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /*已经存在，读取heap summary以获取长度*/
        ptr = MapViewOfFile(hfile, FILE_MAP_READ, 0, 0, sizeof(heap_summary));
        if (ptr != NULL) {
            heap_summary* heap;
            heap = (heap_summary*)ptr;
            aligned_size = heap->size;

            UnmapViewOfFile(ptr);

            ptr = MapViewOfFile(hfile, FILE_MAP_ALL_ACCESS, 0, 0, aligned_size);
        }
    } else {
        /* 新增， 更新 heap summary ; free_node 状态 */
        ptr = MapViewOfFile(hfile, FILE_MAP_ALL_ACCESS, 0, 0, aligned_size);
        if (ptr != NULL) {
            heap_summary* heap;
            free_node* fnode;

            /* 更新heap 状态 */
            heap       = (heap_summary*) ptr;
            heap->id = 0;
            heap->size = size;
            heap->fnode = sizeof(heap_summary);

            /* free node 根节点 */
            fnode = (free_node*)(ptr + heap->fnode);
            fnode->prev_pos = 0;
            fnode->next_pos = 0;
            fnode->heap_pos  = heap->fnode;
            fnode->size = aligned_size - sizeof(heap_summary);
        }
    }

    if (ptr == NULL && hfile != NULL) {
        ret = 1;
        CloseHandle(hfile);
    }

    /* 设置返回值 */
    *pfile = hfile;
    *pptr  = ptr;
    *psize = aligned_size;

    return ret;
}

mihandle miheap_create(int size) {
    mihandle handle = {.ptr = NULL};
    miu32 block_count = 0;
    HANDLE hfile = NULL;
    miu8* ptr = NULL;
    heap_desc *desc;
    int ret;
    miu32 nsize;

    if (size <= 0) {
        block_count = 1024;
    } else if (size > 1024 * 1024 * 1024) {
        block_count = 16 * 1024 * 1024;
    } else {
        block_count = ((size + HEAP_BLOCK_SIZE - 1) / HEAP_BLOCK_SIZE) * HEAP_BLOCK_SIZE;
    }

    size = HEAP_BLOCK_SIZE * block_count;

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
            if (desc->ptr) UnmapViewOfFile(desc->ptr);
            if (desc->hfile) CloseHandle(desc->hfile);
        }
        next = desc->next;
        HeapFree(GetProcessHeap(), 0, desc);
    }
}

void* miheap_malloc(mihandle heap, miu32 data_size) {
    heap_desc*    desc;
    heap_summary* hs;
    free_node*    fnode;
    miu8*         heap_begin;
    miu32         aligned_size; /* 真实分配内存字节数 */
    int           is_ok    = 0; /* 是否分配成功 */
    int           heap_idx = 0; /* 堆 索引 */
    miu32         heap_pos = 0; /* 堆 偏移量 */
    miu8*         node_ptr = NULL;

    aligned_size = ((data_size + sizeof(node_head) + HEAP_DATA_ALIGN - 1) / HEAP_DATA_ALIGN) * HEAP_DATA_ALIGN;

    /* find free space */
    desc = (heap_desc*)heap.ptr;

    do {
        int i;
        for (i = 0; i < HEAP_COUNT; ++i, ++heap_idx) {
            /* heap 没打开 */
            if (desc->ptr[i] == NULL) {
                HANDLE hfile;
                miu8* ptr;
                miu32 size;
                int ret;

                /* 使用默认大小打开/新建内存 */
                ret = create_mapview(heap_idx, 0, &hfile, &ptr, &size);
                if (ret == 0) {
                    desc->hfile[i] = hfile;
                    desc->ptr[i] = ptr;
                    desc->size[i] = size;
                } else {
                    /* 无法分配内存 */
                    return NULL;
                }
            }

            hs         = (heap_summary*) desc->ptr[i];
            heap_begin = (miu8*) hs;
            fnode      = (free_node*) (heap_begin + hs->fnode);

            /* 当前堆无空闲空间 查询下一个 heap */
            if (hs->fnode == FNODE_NONE) continue;

            /* 从根节点遍历heap上空闲空间链表 */
            for (;;) {
                if (fnode->size >= aligned_size) {
                    miu32 remain_size = fnode->size - aligned_size;

                    /* 更新节点状态 */
                    is_ok = 1;
                    heap_pos = fnode->heap_pos;
                    node_ptr = heap_begin + heap_pos;


                    /* 更新链表 */
                    if (remain_size < sizeof(node_head)) {
                        /* 剩余部分不足 创建新freenode */
                        fnode->size = 0;
                        /* 修订分配 size */
                        aligned_size = fnode->size;

                        /* 更新链表 */
                        if (fnode->next_pos != 0 && fnode->prev_pos != 0) {
                            /* 同时存在 prev, next 节点的时候 */
                            free_node* prev;
                            free_node* next;

                            prev           = (free_node*) (heap_begin + fnode->prev_pos);
                            next           = (free_node*) (heap_begin + fnode->next_pos);
                            prev->next_pos = next->heap_pos;
                            next->prev_pos = prev->prev_pos;
                        } else if (fnode->prev_pos != 0 && fnode->next_pos == 0) {
                            /* fnode 为叶节点 存在 prev 节点的时候 */
                            free_node* prev;

                            prev           = (free_node*) (heap_begin + fnode->prev_pos);
                            prev->next_pos = 0;
                        } else if (fnode->prev_pos == 0 && fnode->next_pos == 0) {
                            /* fnode为根节点 */
                            hs->fnode = FNODE_NONE;
                        }


                    } else {
                        /* 插入新的freenode */
                        free_node* new_node;

                        new_node           = (free_node*) (heap_begin + fnode->heap_pos + aligned_size);
                        new_node->heap_pos = fnode->heap_pos + aligned_size;
                        new_node->size     = remain_size;
                        /* 更新freenode链表 */
                        if (fnode->prev_pos != 0 && fnode->next_pos != 0) {
                            /* fnode 为中间节点，同时存在 prev, next 节点 */
                            free_node* prev;
                            free_node* next;

                            prev               = (free_node*) (heap_begin + fnode->prev_pos);
                            next               = (free_node*) (heap_begin + fnode->next_pos);
                            prev->next_pos     = new_node->heap_pos;
                            next->prev_pos     = new_node->prev_pos;
                            new_node->prev_pos = prev->heap_pos;
                            new_node->next_pos = next->heap_pos;
                        } else if (fnode->prev_pos != 0 && fnode->next_pos == 0) {
                            /* fnode 为叶节点，存在 prev 节点 */
                            free_node* prev;

                            prev               = (free_node*) (heap_begin + fnode->prev_pos);
                            prev->next_pos     = new_node->heap_pos;
                            new_node->prev_pos = prev->heap_pos;
                            new_node->next_pos = 0;

                        } else if (fnode->prev_pos == 0 && fnode->next_pos == 0) {
                            /* fnode为根节点 更新 新的根节点位置 */
                            hs->fnode = new_node->heap_pos;

                            new_node->prev_pos = 0;
                            new_node->next_pos = 0;
                        }
                    }
                    /* 分配成功，退出循环 */
                    break;
                }

                /* 已经是叶节点，退出 */
                if (fnode->next_pos == 0) {
                    break;
                }
                fnode = (free_node*) (heap_begin + fnode->next_pos);
            }
        } /** end-of for i--HEAP_COUNT */

        /* 已经插入成功，退出循环 */
        if (is_ok) break;

        /* 遍历heap链表 */
        if (desc->next == NULL) {
            heap_desc *next_desc;
            next_desc           = (heap_desc*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(heap_desc));
            desc->next = next_desc;
        }
        desc = desc->next;
    } while (desc != NULL);

    if (is_ok) {
        /* 更新节点数据 */
        node_head* node;
        node           = (node_head*) node_ptr;
        node->heap_idx = heap_idx;
        node->id       = heap_pos / HEAP_DATA_ALIGN;
        node->size     = aligned_size;
        node->lock     = 0;
        node->ctick    = 0;
        node->mtick    = 0;
        node->atick    = 0;
        /* 指向数据区域 */
        return node_ptr + sizeof(node_head);
    } else {
        return NULL;
    }
}

/**
 * \brief 查找 数据区 对应的 heap指针与位置
 */
static __inline int find_heap(mihandle heap, void* ptr, miu8** heap_begin, miu32* heap_pos, miu32* aligned_size) {
    node_head*    node;
    heap_summary* hs;
    miu32         heap_idx;
    heap_desc*    desc = (heap_desc*) heap.ptr;

    node         = (node_head*) ((miu8*) ptr - sizeof(node_head));
    *heap_pos    = node->id * HEAP_DATA_ALIGN;
    *aligned_size = node->size;

    for (heap_idx = node->heap_idx; heap_idx >= HEAP_COUNT; heap_idx -= HEAP_COUNT) {
        if (desc->next == NULL) return 1;
        desc = desc->next;
    }
    *heap_begin = desc->ptr[heap_idx];
    hs          = (heap_summary*) *heap_begin;
    if (hs == NULL) return 1;
    if (hs->size < (*heap_pos)) return 1;
    return 0;
}

static __inline void find_prev_next(miu8* heap_begin, miu32 heap_pos, miu32 aligned_size,
                                    free_node** pprev, free_node** pnext) {
    free_node*    fnode = NULL;
    free_node*    prev  = NULL;
    free_node*    next  = NULL;
    heap_summary* hs;
    hs    = (heap_summary*) heap_begin;
    fnode = (free_node*) (heap_begin + hs->fnode);
    if (hs->fnode != FNODE_NONE) {
        for (;;) {
            if (fnode->heap_pos + fnode->size <= heap_pos) {
                /* 前方fnode是空节点, 合并空间 */
                prev = fnode;
            } else if (next == NULL && fnode->heap_pos >= heap_pos + aligned_size) {
                /* 后方fnode是空节点，合并空间 */
                next = fnode;
                break;
            }
            if (fnode->next_pos == 0) break;

            fnode = (free_node*) (heap_begin + fnode->next_pos);
        }
    }
    *pprev = prev;
    *pnext = next;
}

void miheap_free(mihandle heap, void* ptr) {
    miu32 heap_pos;
    miu8* heap_begin;
    miu32 aligned_size;
    int ret;

    /* 获取heap */
    ret = find_heap(heap, ptr, &heap_begin, &heap_pos, &aligned_size);
    if (ret != 0) return;


    {
        free_node* fnode;
        heap_summary* hs;
        hs = (heap_summary*)heap_begin;
        fnode = (free_node*)(heap_begin + hs->fnode);

        if (hs->fnode == FNODE_NONE) {
            /* 无根节点  新建空闲节点*/
            fnode = (free_node*)(heap_begin + heap_pos);
            fnode->heap_pos = heap_pos;
            fnode->next_pos = 0;
            fnode->prev_pos = 0;
            fnode->size = aligned_size;
            /* 更新堆状态信息 */
            hs->fnode = heap_pos;
        } else {
            /* 链表查找node前后方是否是空节点 */
            int is_ok = 0;
            free_node *prev = NULL;
            free_node *next = NULL;
            find_prev_next(heap_begin, heap_pos, aligned_size, &prev, &next);

            /* prev-node-next 相连， 则合并 */
            if (prev && next && is_ok == 0) {
                if (prev->heap_pos + prev->size == heap_pos && next->heap_pos == heap_pos + aligned_size) {
                    prev->size += aligned_size + next->size;
                    prev->next_pos = next->next_pos;
                    is_ok = 1;
                }
            }

            /* prev-node 相连， 则合并 */
            if (prev && is_ok == 0) {
                if (prev->heap_pos + prev->size == heap_pos) {
                    prev->size += aligned_size;
                    is_ok = 1;
                }
            }

            /* node-next 相连， 则合并 */
            if (next && is_ok == 0) {
                if (next->heap_pos == heap_pos + aligned_size) {
                    free_node* nnode;
                    nnode           = (free_node*) (heap_begin + heap_pos);
                    nnode->size     = next->size + aligned_size;
                    nnode->prev_pos = next->prev_pos;
                    nnode->next_pos = next->next_pos;
                    nnode->heap_pos = heap_pos;
                    is_ok = 1;

                    if (next->prev_pos != 0) {
                        free_node* nprev;
                        nprev           = (free_node*) (heap_begin + next->prev_pos);
                        nprev->next_pos = heap_pos;
                    } else {
                        /*next 为根节点 更新summary*/
                        hs->fnode = heap_pos;
                    }

                    if (next->next_pos != 0) {
                        free_node* nnext;
                        nnext           = (free_node*) (heap_begin + next->next_pos);
                        nnext->prev_pos = heap_pos;
                    }
                }
            }


            /* 均不相连 插入新的空闲空间节点 */
            if (is_ok == 0) {
                free_node* nnode;
                nnode           = (free_node*) (heap_begin + heap_pos);
                nnode->size     = next->size + aligned_size;
                nnode->prev_pos = 0;
                nnode->next_pos = 0;
                nnode->heap_pos = heap_pos;

                if (prev) {
                    prev->next_pos = heap_pos;
                    nnode->prev_pos = prev->heap_pos;
                } else {
                    /* 这是根节点 */
                    hs->fnode = heap_pos;
                }

                if (next) {
                    next->prev_pos = heap_pos;
                    nnode->next_pos = next->heap_pos;
                }
            }
        } /* 存在freenode 链表 */
    }
}

void* miheap_realloc(mihandle heap, void* ptr, miu32 data_size) {
    miu32 heap_pos;
    miu8* heap_begin;
    miu32 aligned_size;
    int ret;
    miu8* nptr;
    miu32 n_size;

    /* 获取heap */
    ret = find_heap(heap, ptr, &heap_begin, &heap_pos, &aligned_size);
    if (ret != 0) return NULL;

    if (data_size + sizeof(node_head) <= aligned_size) return ptr;


    n_size = ((data_size + sizeof(node_head) + HEAP_DATA_ALIGN - 1) / HEAP_DATA_ALIGN) * HEAP_DATA_ALIGN;

    nptr = miheap_malloc(heap, n_size);

    if (nptr == NULL) return NULL;

    memset(nptr, 0, n_size);
    memcpy(nptr, ptr, aligned_size - sizeof(node_head));

    miheap_free(heap, ptr);

    return nptr;
}

