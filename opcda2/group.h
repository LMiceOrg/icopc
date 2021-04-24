/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_GROUP_H_
#define OPCDA2_GROUP_H_

#include "opcda2_all.h"

/* group 中最大 item 数量 */
#define OPCDA2_GROUP_ITEM_MAX 1024

/* item extra 长度，冲突区大小 */
#define OPCDA2_EXTRA_DATA_MAX 1024

/* item extra 起始位置 */
#define OPCDA2_EXTRA_DATA_START 1

/* item data 散列表长度 */
#define OPCDA2_ITEM_DATA_MAX 65536

/* size == -1: 清空全部item */
#define OPCDA2_ITEM_ALL (-1)

/* 无效的item handle */
#define OPCDA2_HANDLE_INVALID ((unsigned int)0xFFFFFFFF)

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
    item_data         data[OPCDA2_ITEM_DATA_MAX];   /* 数据表 */
    item_data         extra[OPCDA2_EXTRA_DATA_MAX]; /* 属性表 */
} data_list;

typedef struct group {
    int       used; /* 结构体使用情况 */
    OPCHANDLE cli_group;
    OPCHANDLE svr_handle;
    wchar_t   name[32]; /*名称*/

    int          active; /* 激活状态 */
    DWORD        cookie; /* 通知 用于IConnectionPoint */
    DWORD        rate;  /* 更新速率 （毫秒） */
    unsigned int salt; /* 随机数, 用于计算group hash */
    unsigned int hash; /* 散列值, 用于计算item hash */

    struct server_connect* conn;
    data_list*             datas;

    int        item_size;                   /* 现存item的数量 */
    item_data* item[OPCDA2_GROUP_ITEM_MAX]; /* 现存item的指针数组 */

    IOPCGroupStateMgt* grp_mgt;
    IOPCItemMgt*       itm_mgt;
    IConnectionPoint*  cp;
    IOPCAsyncIO2*      async_io2; /*OPCDA2 异步操作 */
} group;

/* IOPCDataCallback */
int opcda2_group_advise_callback(group* grp, IUnknown* cb);
void opcda2_group_unadvise_callback(group* grp);

/** item 操作 size=OPCDA2_ITEM_ALL时，清除全部已添加item   */

/* 在group 查找 item, 1:found, 0:not found */
int opcda2_item_find(group* grp, const wchar_t* id);

/* 向group添加 item */
int opcda2_item_add(group *grp, int size, const wchar_t* item_ids, int *active);

/* 从group删除 item */
int opcda2_item_del(group *grp, int size, const wchar_t* item_ids);

/* 获取 item 数量 group->item_size */
#define opcda2_item_size(grp) ((grp)->item_size)

/* 获取 item data group->item[pos] */
#define opcda2_item_data(grp, pos) ((grp)->item[pos])

/* 写入 item 数据 */
int opcda2_item_write(group* grp, int size, const wchar_t* item_ids, const VARIANT* item_values);

/* 获取data 通过句柄 */
item_data* opcda2_data_find(data_list* datas, unsigned int handle);

void opcda2_data_clear(item_data* data);


/* 从数据表中 删除 data */
void opcda2_data_del(data_list* datas, unsigned int handle);

/* 插入 data */
unsigned int opcda2_data_add(data_list* datas, unsigned int hash);

/* 获取 data 数据 */
#define opcda2_data(_ds, _hdl) ((_ds)->data +(_hdl))

/* 验证 handle是否正确 */
#define opcda2_data_handle(_hdl) ((_hdl) < OPCDA2_ITEM_DATA_MAX + OPCDA2_EXTRA_DATA_MAX)

#endif  /** OPCDA2_GROUP_H_ */

