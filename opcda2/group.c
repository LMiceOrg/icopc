/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include "group.h"
#include "server.h"
#include "util_trace.h"

#define LEAF_NODE (0)

#include "util_hash.h"



static void opcda2_group_remove_item(group* grp, int pos) {
    item_data* data     = grp->item[pos];
    int        tail_num = grp->item_size - pos - 1;

    /* 删除 data */
    opcda2_data_del(grp->datas, data->handle);

    /* remove item[pos], move pos+1 to pos */
    if (tail_num > 0) {
        memcpy(grp->item + pos, grp->item + pos + 1, sizeof(item_data*) * tail_num);
    }
    grp->item_size--;
}

unsigned int opcda2_data_add(data_list* datas, unsigned int hash) {
    item_data*   data   = opcda2_data(datas, hash);
    unsigned int handle = OPCDA2_HANDLE_INVALID;
    int          prev   = 0;
    int          extra_pos;
    item_data*   parent;

    if (data->used == OPCDA2_NOTUSE || data->used == OPCDA2_HASEXTRA) {
        data->used   = OPCDA2_USING;
        data->handle = hash;
        data->hash   = hash;
        handle       = hash;
    } else if (data->used == OPCDA2_USING) {
        /* 表中位置正在使用， 向extra中插入 */
        parent = data;

        /*1.查找 父节点 和 叶节点prev */
        if (parent->next == LEAF_NODE) {
            /*没有子节点，当前节点就是叶节点 */
            prev = 0; /* 父节点 指向 散列表 */
        } else {
            /* 有子节点，循环查找叶节点 */
            for (extra_pos = 1; extra_pos < OPCDA2_EXTRA_DATA_MAX; ++extra_pos) {
                /* 指向 extra 记录 */
                prev = parent->next;
                parent = datas->extra + parent->next;

                /* 找到叶节点，退出查找 */
                if (parent->next == LEAF_NODE) break;
            }

            /* 循环深度达到最大值，意味数据表被破坏，出现死循环 */
            if (extra_pos == OPCDA2_EXTRA_DATA_MAX) {
                trace_debug("opcda2_data_add error:table status incorrect\n");
                handle = OPCDA2_HANDLE_INVALID;
                goto err_occured;
            }
        }

        /* 2. 查找extra 在未使用的位置插入 */
        for (extra_pos = OPCDA2_EXTRA_DATA_START; extra_pos < OPCDA2_EXTRA_DATA_MAX; ++extra_pos) {
            item_data* extra = datas->extra + extra_pos;
            if (extra->used != OPCDA2_USING) {
                /* 此时 parent:父节点 extra:叶节点 */
                parent->next  = extra_pos;    /* 修改父节点的next */
                extra->next   = 0;            /* 修改叶节点的next(0) */
                extra->prev   = prev;         /* 修改叶节点的prev */
                extra->used   = OPCDA2_USING; /* 修改叶节点状态 正在使用 */
                handle        = extra_pos;    /* 要返回的插入位置 */
                extra->handle = OPCDA2_ITEM_DATA_MAX + extra_pos;
                extra->hash   = hash;
                break;
            }
        }

        /* 循环深度达到最大值，意味数据表被破坏，出现死循环 */
        if (extra_pos == OPCDA2_EXTRA_DATA_MAX) {
            trace_debug("opcda2_data_add error:table status incorrect\n");
            handle = OPCDA2_HANDLE_INVALID;
            goto err_occured;
        }

    } else {
        trace_debug("opcda2_data_add error:table status incorrect\n");
        handle = OPCDA2_HANDLE_INVALID;
    }
err_occured:
    return handle;
}

void opcda2_data_del(data_list* datas, unsigned int handle) {
    item_data*   data       = opcda2_data(datas, handle);
    unsigned int data_table = data->handle >> 16;

    /* 1. data未使用，直接返回 */
    if (data->used != OPCDA2_USING) return;

    /* 1.1 清除data 数据 */
    VariantClear(&data->value);

    /* 2 双向链表 操作删除data节点 */
    if (data_table > 0) {
        /* data在extra 中, 更新父节点next=data.next, 子节点prev=data.prev */
        item_data* parent;
        item_data* child;

        child = datas->extra + data->next;
        if (data->prev > 0) {
            /* data的父节点在extra中， 位置由prev确定 */
            parent = datas->extra + data->prev;
        } else {
            /* data的父节点在data中, 位置由hash确定 */
            parent = datas->data + data->hash;
        }

        parent->next = data->next;
        child->prev = data->prev;

        data->used = OPCDA2_NOTUSE;
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
}

/* 获取data 通过句柄 */
item_data* opcda2_data_find(data_list* datas, unsigned int handle) {
    item_data* data = NULL;
    #if _DEBUG
    unsigned int table = handle >> 16;
    unsigned int pos = (handle & ((1 << 16) - 1));
    if (table == 0) {
        /* data 数据表 */
        if (pos < OPCDA2_ITEM_DATA_MAX) data = datas->data + pos;
    } else {
        /* extra 数据表 */
        if (pos < OPCDA2_EXTRA_DATA_MAX) data = datas->extra + pos;
    }
    #else
    /* handle ==> data + extra */
    if (handle < OPCDA2_ITEM_DATA_MAX + OPCDA2_EXTRA_DATA_MAX) {
        data = datas->data + handle;
    }
    #endif

    return data;
}

int opcda2_group_advise_callback(group* grp, IUnknown* cb) {
    int     ret = 0;
    HRESULT hr;

    if (cb == NULL) return ret;

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

int opcda2_item_find(group* grp, const wchar_t* id) {
    int i;
    for (i = 0; i < grp->item_size; ++i) {
        item_data* item = grp->item[i];
        if (wcscmp(item->id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

int opcda2_item_add(group* grp, int size, const wchar_t* item_ids, int* active) {
    int            ret         = 1;
    HRESULT        hr          = S_OK;
    OPCITEMDEF*    item_def    = NULL;
    OPCITEMRESULT* item_result = NULL;
    HRESULT*       item_hr     = NULL;
    int            item_cnt;
    int            grp_item_base;
    int i;

    item_def = (OPCITEMDEF*) CoTaskMemAlloc(sizeof(OPCITEMDEF) * size);
    memset(item_def, 0, sizeof(OPCITEMDEF) * size);

    item_cnt      = 0;
    grp_item_base = grp->item_size;

    for (i = 0; i < size; ++i) {
        unsigned int   hval;
        unsigned int hash;
        unsigned int handle;
        size_t         id_len;
        const wchar_t* id;
        item_data*     data;

        id     = item_ids + i * OPCDA2_ITEM_ID_LEN;
        id_len = wcslen(id);
        hval   = eal_hash32_fnv1a_more(id, id_len*sizeof(wchar_t), grp->hash);
        hash   = eal_hash32_to16(hval);


        /* 在group中item已经插入, 跳过此id */
        if (opcda2_item_find(grp, id)) continue;

        /* 向数据表中添加 data */
        handle = opcda2_data_add(grp->datas, hash);

        /* 如果添加失败，则跳过此id */
        if (handle == OPCDA2_HANDLE_INVALID) continue;

        /* 更新数据data 状态 */
        data = opcda2_data(grp->datas, handle);

        data->ts.dwHighDateTime = 0;
        data->ts.dwLowDateTime  = 0;
        data->cli_group         = grp->cli_group;
        data->svr_handle        = 0;
        data->active            = active[i];
        memset(data->id, 0, OPCDA2_ITEM_ID_SIZE);
        memcpy(data->id, id, id_len * sizeof(wchar_t));

        /* 插入 item_def 列表 */
        item_def[item_cnt].szItemID     = data->id;
        item_def[item_cnt].bActive      = data->active;
        item_def[item_cnt].hClient      = data->handle;
        item_cnt++;

        /* 插入 group item 列表 */
        grp->item[grp->item_size] = data;
        grp->item_size++;
    }

    /* 2. 检查是否可以插入 */
    item_result = NULL;
    item_hr     = NULL;

    hr = grp->itm_mgt->lpVtbl->ValidateItems(grp->itm_mgt, item_cnt, item_def, FALSE, &item_result, &item_hr);
    if (SUCCEEDED(hr)) {
        if (hr == S_FALSE) {
            trace_debug("some item cannot be added\n");
            for (i = 0; i < item_cnt; ++i) {
                wtrace_debug(L"validate item(%ls) hr 0x%lX\n", item_def[i].szItemID, item_hr[i]);
            }
        } else {
            trace_debug("all items(%d) can be added\n", item_cnt);

        }
        if (item_result) {
            CoTaskMemFree(item_result);
            item_result = NULL;
        }
        if (item_hr) {
            CoTaskMemFree(item_hr);
            item_hr = NULL;
        }
    } else {
        trace_debug("All items cannot be added 0x%lX\n", hr);
        if (item_result) {
            CoTaskMemFree(item_result);
            item_result = NULL;
        }
        if (item_hr) {
            CoTaskMemFree(item_hr);
            item_hr = NULL;
        }
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

        for (i = item_cnt - 1; i >= 0; --i) {
            unsigned int handle;
            item_data*   data;

            hr     = item_hr[i];
            handle = item_def[i].hClient;
            data   = opcda2_data(grp->datas, handle);
            wtrace_debug(L"add item(%ls) hr %ld type ask[%d] ret[%d]\n", item_def[i].szItemID, item_hr[i],
                         item_def[i].vtRequestedDataType, item_result[i].vtCanonicalDataType);
            if (hr == S_OK) {
                /*Server 确认插入成功， 设置字段*/
                data->svr_handle = item_result[i].hServer;
            } else {
                /* Server 未插入, 删除 group data */
                int pos = grp_item_base + i;
                opcda2_group_remove_item(grp, pos);
            }
        }

        if (item_result) {
            CoTaskMemFree(item_result);
            item_result = NULL;
        }
        if (item_hr) {
            CoTaskMemFree(item_hr);
            item_hr = NULL;
        }

        /* 插入成功 刷新 data value */
        if (grp->async_io2) {
            DWORD cancel_id;

            hr = grp->async_io2->lpVtbl->Refresh2(grp->async_io2, OPC_DS_CACHE, 1, &cancel_id);
            /* trace_debug("refresh data 0x%lX\n", hr); */
        }
    } else {
        trace_debug("All items cannot added 0x%lx\n", hr);
        if (item_result) {
            CoTaskMemFree(item_result);
            item_result = NULL;
        }
        if (item_hr) {
            CoTaskMemFree(item_hr);
            item_hr = NULL;
        }
        goto clean_up;
    }
    ret = 0;
clean_up:
    if (item_def) CoTaskMemFree(item_def);
    return ret;
}

int opcda2_item_del(group* grp, int size, const wchar_t* item_ids) {
    HRESULT        hr;
    HRESULT*       item_hr     = NULL;
    OPCHANDLE*     remove_item = NULL;
    int*           item_pos;
    int            remove_size;
    int i;
    int j;
    /* group为空 */
    if (grp->item_size == 0) return 0;

    /* 1 初始化 */
    remove_size = 0;
    remove_item = (OPCHANDLE*) CoTaskMemAlloc(sizeof(OPCHANDLE) * grp->item_size);
    item_pos    = (int*) CoTaskMemAlloc(sizeof(int) * grp->item_size);

    /* size == -1 ==> 清空全部 */
    /* 在group.item 中查找 */
    for ( i = grp->item_size - 1; i >= 0; --i) {
        item_data* data = grp->item[i];
        if (size == OPCDA2_ITEM_ALL) {
            item_pos[remove_size]    = i;
            remove_item[remove_size] = data->svr_handle;
            remove_size++;
        } else {
            for (j = 0; j < size; ++j) {
                const wchar_t* id = item_ids + j * OPCDA2_ITEM_ID_LEN;
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
        for (i = 0; i < remove_size; ++i) {
            wtrace_debug(L"item(%ls) remove %lx\n", grp->item[i]->id, item_hr[i]);
            if (item_hr[i] == S_OK) {
                int pos = item_pos[i];
                opcda2_group_remove_item(grp, pos);
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

int opcda2_item_write(group* grp, int size, const wchar_t* item_ids, const VARIANT* item_values) {
    int        ret             = 1;
    HRESULT    hr              = 0;
    int        item_size       = 0;
    DWORD      cancel_id       = 0;
    DWORD      trans_id        = 2;
    HRESULT*   item_hr         = NULL;
    OPCHANDLE* item_svr_handle = NULL;
    VARIANT*   item_new_value  = NULL;
    int id_idx;
    int itm_idx;
    int i;

    if (!grp->async_io2) {
        return ret;
    }
    if (size <= 0) return ret;

    item_hr         = (HRESULT*) CoTaskMemAlloc(sizeof(HRESULT) * size);
    item_svr_handle = (OPCHANDLE*) CoTaskMemAlloc(sizeof(OPCHANDLE) * size);
    item_size       = 0;
    for (id_idx = 0; id_idx < size; ++id_idx) {
        const wchar_t* id         = item_ids + id_idx * OPCDA2_ITEM_ID_LEN;
        OPCHANDLE     svr_handle = 0;
        for (itm_idx = 0; itm_idx < grp->item_size; ++itm_idx) {
            item_data* item = grp->item[itm_idx];
            if (wcscmp(id, item->id) == 0) {
                svr_handle = item->svr_handle;
                break;
            }
        }
        if (svr_handle) {
            item_svr_handle[item_size] = svr_handle;
            if (item_new_value) VariantCopy(item_new_value + item_size, (VARIANT*)(item_values + id_idx));
            ++item_size;
        } else {
            if (item_new_value == NULL) {
                item_new_value = CoTaskMemAlloc(sizeof(VARIANT) * size);
                memset(item_new_value, 0, sizeof(VARIANT) * size);
                for (i = 0; i < item_size; ++i) {
                    VariantCopy(item_new_value + i,  (VARIANT*)(item_values + i));
                }
            }
        }
    }
    if (item_new_value) {
        hr = grp->async_io2->lpVtbl->Write(grp->async_io2, item_size, item_svr_handle, item_new_value, trans_id, &cancel_id,
                                           &item_hr);
        CoTaskMemFree(item_new_value);
    } else {
        hr = grp->async_io2->lpVtbl->Write(grp->async_io2, size, item_svr_handle, item_new_value, trans_id, &cancel_id,
                                           &item_hr);
    }
    if (SUCCEEDED(hr)) {
        ret = 0;
    }
    CoTaskMemFree(item_hr);
    CoTaskMemFree(item_svr_handle);
    return ret;
}
