/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include "group.h"
#include "server.h"
#include "trace.h"


#include "util_hash.h"

item_data* opcda2_data_find(data_list* datas, unsigned int handle) {
    item_data* data = NULL;
    unsigned int table = handle >> 16;
    unsigned int pos = (handle & ((1 << 16) - 1));
    if (table == 0) {
        /* data 数据表 */
        if (pos < OPCDA2_ITEM_DATA_MAX) data = datas->data + pos;
    } else {
        /* extra 数据表 */
        if (pos < OPCDA2_EXTRA_DATA_MAX) data = datas->extra + pos;
    }

    return data;
}

static item_data* opcda2_extra_insert(group* grp, item_data* data) {
    item_data* extra     = grp->datas->extra;
    item_data* ret       = data;
    int        prev      = 0;
    int        find_one  = 0;



    /* 1. 查找extra中的父节点
        extra(ret->next == 0 ) 还没有记录 prev = 0(指向data表) */
    if (ret->next > 0) {
        /* ret->next 指向 第一个extra 记录 */
        prev = ret->next;
        ret  = extra + ret->next;
        for (;;) {
            /* 判断是否已经插入过 */
            if (ret->used == 1 && wcscmp(ret->id, data->id) == 0 && ret->cli_group == grp->cli_group) {
                /* data已经插入extra (group_id, data_id)都相等 */
                wtrace_debug(L"item(%ls) already inserted\n", ret->id);
                return ret;
            }

            /* 找到父节点，退出查找 */
            if (ret->next == 0) break;

            /* 指向下一个extra 记录 */
            prev = ret->next;
            ret  = extra + ret->next;
        }
    }

    /* 2. 查找extra 中未使用的元素位置 */
    for (int i = 1; i < OPCDA2_EXTRA_DATA_MAX; ++i) {
        item_data* p = extra + i;
        if (p->used == 0) {
            /* ret:父节点 p:叶节点 */
            VariantClear(&p->value);
            memset(p, 0, sizeof(item_data));

            ret->next = i; /* 修改父节点的next */

            p->next   = 0;             /* 修改叶节点的next(0) */
            p->prev   = prev;       /* 修改叶节点的prev(ret) */
            p->handle = (1 << 16) + i; /* 修改叶节点 handle 指向 extra */

            ret        = p; /* ret 指向 p （ret成为页节点）*/
            find_one   = 1;
            break;
        }
    }

    /* 3. 处理extra中未找到空闲元素的情况 */
    if (find_one == 0) {
        /* extra full, failed!*/
        trace_debug("item data list full, cannot add items!\n");
        ret = NULL;
    }

    return ret;
}

int opcda2_group_advise_callback(group* grp, IUnknown* cb) {
    int     ret = 0;
    HRESULT hr;

    /* 关闭现有通知 */
    opcda2_group_unadvise_callback(grp);

    /* 创建通知 */
    hr = grp->cp->lpVtbl->Advise(grp->cp, cb, &grp->cookie);
    if (FAILED(hr)) {
        ret = 1;
    }
    return ret;
}

void opcda2_group_unadvise_callback(group* grp) {
    if (grp->cookie) {
        grp->cp->lpVtbl->Unadvise(grp->cp, grp->cookie);
        grp->cookie = 0;
    }
}

int opcda2_item_add(group* grp, int size, const wchar_t* item_ids, int* active) {
    int item_cnt;
    HRESULT        hr;
    OPCITEMDEF*    item_def    = NULL;
    OPCITEMRESULT* item_result = NULL;
    HRESULT*       item_hr     = NULL;

    item_def = (OPCITEMDEF*) CoTaskMemAlloc(sizeof(OPCITEMDEF) * size);
    memset(item_def, 0, sizeof(OPCITEMDEF) * size);

    item_cnt = 0;
    for (int i = 0; i < size; ++i) {
        unsigned int   hval;
        unsigned int hash;
        unsigned int handle;
        size_t         id_len;
        const wchar_t* id;
        item_data*     data;

        id     = item_ids + i*32;
        id_len = wcslen(id);
        hval   = eal_hash32_fnv1a_more(id, id_len*sizeof(wchar_t), grp->hash);
        hash   = eal_hash32_to16(hval);
        handle = hash; /* 默认 handle 指向 data */
        data   = grp->datas->data + handle;

        if (data->used == OPCDA2_NOTUSE) {
            /* 未使用，新的data */
            VariantClear(&data->value);
            memset(data, 0, sizeof(item_data));
            data->handle = handle;
        } else if (data->used == OPCDA2_HASEXTRA) {
            /* 未使用，但是链接有子节点 */
            int next = data->next;
            VariantClear(&data->value);
            memset(data, 0, sizeof(item_data));
            data->handle = handle;
            data->next   = next;
        } else if (data->used == OPCDA2_USING) {
            /* 正在使用，判断是否相等(group, id)  */
            if (wcscmp(data->id, id) == 0 && data->cli_group == grp->cli_group) {
                wtrace_debug(L"item %ls already added to group(%ld)\n", id, grp->cli_group);
                continue;
            } else {
                /* 不相等，但是hash值一致==> hash冲突, 向extra插入 */
                group* grp_data = grp->conn->grp + data->cli_group - 1;
                wtrace_debug(L"hash conflict (%ls) g(%lX) i(%lX) == (%ls) g(%lX) i(%lX)\n", id, grp->hash, handle,
                             data->id, grp_data->hash, data->handle);
                data = opcda2_extra_insert(grp, data);
                if (data == NULL) {
                    trace_debug("item data list full, cannot add items!\n");
                    goto clean_up;
                } else if (wcscmp(data->id, id) == 0 && data->cli_group == grp->cli_group) {
                    wtrace_debug(L"item %ls already added extra to group(%ld)\n", id, grp->cli_group);
                    continue;
                }
            }
        }

        data->cli_group  = grp->cli_group;
        data->active = active[i];
        memcpy(data->id, id, id_len*sizeof(wchar_t));
        data->used = 1;
        data->hash = hash;


        item_def[item_cnt].szItemID = data->id;
        item_def[item_cnt].bActive  = data->active;
        item_def[item_cnt].hClient  = data->handle;
        item_cnt++;
    }

    /* 2. 检查是否可以插入 */
    hr = grp->itm_mgt->lpVtbl->ValidateItems(grp->itm_mgt, item_cnt, item_def, FALSE, &item_result, &item_hr);
    if (SUCCEEDED(hr)) {
        if (hr == S_FALSE) {
            trace_debug("some item cannot be added\n");
            for (int i = 0; i < item_cnt; ++i) {
                wtrace_debug(L"validate item(%ls) hr %ld\n", item_def[i].szItemID, item_hr[i]);
            }
        } else {
            trace_debug("all items(%d) can be added\n", item_cnt);

        }
        CoTaskMemFree(item_result);
        CoTaskMemFree(item_hr);
    } else {
        trace_debug("All items cannot be added 0x%lx\n", hr);
        goto clean_up;
    }

    /* 3. 插入Item */
    hr = grp->itm_mgt->lpVtbl->AddItems(grp->itm_mgt, item_cnt, item_def, &item_result, &item_hr);
    if (SUCCEEDED(hr)) {
        if (hr == S_FALSE) {
            trace_debug("some item cannot added\n");
        } else {
            trace_debug("all items added\n");
        }
        for (int i = 0; i < item_cnt; ++i) {
            hr = item_hr[i];
            wtrace_debug(L"add item(%ls) hr %ld\n", item_def[i].szItemID, item_hr[i]);
            if (hr == S_OK) {
                unsigned int handle = item_def[i].hClient;
                item_data* data;
                data = opcda2_data_find(grp->datas, handle);
                if (data == NULL) {
                    trace_debug("data_items failed\n");
                    break;
                }

                /*设置字段*/
                data->used = 1;
                data->svr_handle = item_result[i].hServer;
                /* 插入group.item */
                grp->item[grp->item_size] = data;
                grp->item_size++;
            }

        }
        CoTaskMemFree(item_result);
        CoTaskMemFree(item_hr);
    } else {
        trace_debug("All items cannot added 0x%lx\n", hr);
    }

    // remove_size = grp->item_size;
    // remove_item = (OPCHANDLE*) CoTaskMemAlloc(sizeof(OPCHANDLE) * remove_size);
    // for(int i=0; i<remove_size; ++i) {
    //     remove_item[i] = grp->item[i]->svr_handle;
    // }

    // hr = grp->itm_mgt->lpVtbl->RemoveItems(grp->itm_mgt, remove_size, remove_item, &item_hr);
    // if (SUCCEEDED(hr)) {
    //     if (hr == S_OK) {
    //         /* remove success */
    //         trace_debug("remove item %d success\n", remove_size);
    //         grp->item_size -= remove_size;
    //     } else if (hr == S_FALSE) {
    //         trace_debug("remove item %d false\n", remove_size);
    //     }
    //     for (int i = 0; i < remove_size; ++i) {
    //         wtrace_debug(L"item(%ls) remove %lx\n", grp->item[i]->id, item_hr[i]);
    //     }
    //     CoTaskMemFree(item_hr);
    // } else {
    //     trace_debug("remove item failed  %ld\n", hr);
    // }

clean_up:
    if (item_def) CoTaskMemFree(item_def);
    return 0;
}

int opcda2_item_del(group* grp, int size, const wchar_t* item_ids) {
    HRESULT        hr;
    HRESULT*       item_hr     = NULL;
    OPCHANDLE*     remove_item = NULL;
    int*           item_pos;
    int            remove_size;

    /* group为空 */
    if (grp->item_size == 0) return 0;

    /* 1 初始化 */
    remove_size = 0;
    remove_item = (OPCHANDLE*) CoTaskMemAlloc(sizeof(OPCHANDLE) * grp->item_size);
    item_pos    = (int*) CoTaskMemAlloc(sizeof(int) * grp->item_size);

    /* size == -1 ==> 清空全部 */
    /* 在group.item 中查找 */
    for (int i = grp->item_size - 1; i >= 0; --i) {
        item_data* data = grp->item[i];
        if (size == -1) {
            item_pos[remove_size]    = i;
            remove_item[remove_size] = data->svr_handle;
            remove_size++;
        } else {
            for (int j = 0; j < size; ++j) {
                const wchar_t* id = item_ids + j * 32;
                if (wcscmp(data->id, id) == 0) {
                    item_pos[remove_size]    = i;
                    remove_item[remove_size] = data->svr_handle;
                    remove_size++;

                    break;
                }
            }
        }
    }
    trace_debug("begin remove %d items\n", remove_size);
    hr = grp->itm_mgt->lpVtbl->RemoveItems(grp->itm_mgt, remove_size, remove_item, &item_hr);
    if (SUCCEEDED(hr)) {
        if (hr == S_OK) {
            /* remove success */
            trace_debug("remove item %d success\n", remove_size);
        } else if (hr == S_FALSE) {
            trace_debug("remove item %d false\n", remove_size);
        }
        for (int i = 0; i < remove_size; ++i) {
            wtrace_debug(L"item(%ls) remove %lx\n", grp->item[i]->id, item_hr[i]);
            if (item_hr[i] == S_OK) {
                int tail_num;
                int pos;
                item_data* data;

                pos = item_pos[i];
                tail_num = grp->item_size - pos - 1;
                data = grp->item[pos];

                /* 清除data 数据 */
                VariantClear(&data->value);

                unsigned int data_table = data->handle >> 16;
                unsigned int data_pos = (data->handle & ((1 << 16) - 1));
                /* 双向链表 操作 */
                if (data_table > 0) {
                    /* data在extra 中,意味着data有父节点 */
                    data->used = OPCDA2_NOTUSE;

                    item_data* parent;
                    if (data->prev > 0) {
                        /* data的父节点也在extra中 */
                        parent = grp->datas->extra + data->prev;
                    } else {
                        /* data的父节点在data中 */
                        parent = grp->datas->data + data_pos;
                    }
                    parent->next = data->next;
                } else {
                    /* data在data中 data就是根节点 */
                    if (data->next == 0) {
                        /* 没有子节点 do-nothing*/
                        data->used = OPCDA2_NOTUSE;
                    } else {
                        /* 有子节点, 设置 used */
                        data->used = OPCDA2_HASEXTRA;
                    }
                }

                /* remove item[pos], move pos+1 to pos */
                if (tail_num > 0) {
                    memcpy( grp->item+pos, grp->item+pos+1, sizeof(item_data*) *tail_num );
                }
                grp->item_size--;

            }
        }
        CoTaskMemFree(item_hr);
        item_hr = NULL;
    } else {
        trace_debug("remove item failed  %ld\n", hr);
    }
    if (remove_item) CoTaskMemFree(remove_item);
    if (item_pos) CoTaskMemFree(item_pos);
    return 0;
}