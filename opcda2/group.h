/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_GROUP_H_
#define OPCDA2_GROUP_H_

#include "opcda2_all.h"

/* group 中最大 item 数量 */
#define OPCDA2_GROUP_ITEM_MAX 1024

/* item extra 长度，冲突区大小 */
#define OPCDA2_EXTRA_DATA_MAX 1024

/* item data 散列表长度 */
#define OPCDA2_ITEM_DATA_MAX 65536

/* size == -1: 清空全部item */
#define OPCDA2_DEL_ALL (-1)

#define OPCDA2_NOTUSE (0) /* 状态 未使用 */
#define OPCDA2_USING (1)  /* 状态 正在使用 */
#define OPCDA2_HASEXTRA (2)  /* 状态 未使用，但有extra */

struct server_connect;


typedef struct item_data {
    VARIANT      value;      /* 数据（类型，值） */
    FILETIME     ts;         /* 更新时间 */
    unsigned int handle;     /* 所在列表位置: high(16):所在列表 low(16):列表偏移量 */
    OPCHANDLE    cli_group;  /* 从属组 */
    OPCHANDLE    svr_handle; /* AddItems时从 Server 返回，用于RemoveItems */
    int          active;     /* 状态 */
    wchar_t      id[32];     /* id */
    int          used;       /* 使用与否 */
    unsigned int hash;       /* item 散列值 */
    int          next;       /* 相同hash下 在extra的数据位置, 从1开始 0:没有 */
    int          prev;       /* 相同hash下 指向上一条记录，正在extra中的位置,从1开始，0:data */
} item_data;

typedef struct data_list {
    int               size;                         /* 当前插入的数据 */
    int               capacity;                     /* 65536 */
    struct data_list* next;                         /* !! 未实现 */
    int               used[2048];                   /* 使用情况 bitmap 2048*32 == 65536 */
    item_data         data[OPCDA2_ITEM_DATA_MAX];   /* 数据表 */
    item_data         extra[OPCDA2_EXTRA_DATA_MAX]; /* 属性表 */
} data_list;

typedef struct group {
    int used; /* 结构体使用情况 */
    OPCHANDLE cli_group;
    OPCHANDLE svr_handle;
    wchar_t name[32]; /*名称*/

    int     active;
    DWORD      cookie;
    DWORD      rate;
    int salt; /* 用于计算group hash 值 */
    unsigned int hash;

    struct server_connect *conn;
    data_list* datas;

    int        item_size;                        /* group_item 数量 */
    item_data* item[OPCDA2_GROUP_ITEM_MAX]; /* 指向item_data 的指针数组 */

    IOPCGroupStateMgt* grp_mgt;
    IOPCItemMgt*       itm_mgt;
    IConnectionPoint*  cp;
} group;

/* IOPCDataCallback */
int opcda2_group_advise_callback(group* grp, IUnknown* cb);
void opcda2_group_unadvise_callback(group* grp);

/** item 操作 */

/* 向group添加 item */
int opcda2_item_add(group *grp, int size, const wchar_t* item_ids, int *active);

/* 从group删除 item， size=OPCDA2_DEL_ALL时，清除全部已添加item  */
int opcda2_item_del(group *grp, int size, const wchar_t* item_ids);


item_data* opcda2_data_find(data_list* datas, unsigned int handle);

#endif  /** OPCDA2_GROUP_H_ */

