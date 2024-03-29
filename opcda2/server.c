/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include <stddef.h>
#include <stdlib.h>

#include "server.h"
#include "util_hash.h"
#include "util_trace.h"

iclist_def_function(connect_list, server_connect)

    static inline unsigned int salt_gen() {
    /*
    Seed(n+1) = (a * Seed(n) + c) % m
    Seed(0) = 0x1234
                         m        a         c
    Numerical Recipes	2^32	1664525	1013904223
    */
    static unsigned int seed = 0x1234;

    seed = seed * 1664525 + 1013904223;
    return seed;
}

static int opcda2_clsid(const wchar_t *host, const wchar_t *prog_id, GUID *clsid) {
    int             ret = 1;
    HRESULT         hr  = S_OK;
    COSERVERINFO    svr_info;
    MULTI_QI        qi_list[1];
    IID             cat_id      = IID_CATID_OPCDAServer20;
    IOPCServerList *server_list = NULL;
    IEnumGUID *     ienum_guid  = NULL;
    ULONG           fetched;

    if (host == NULL || wcscmp(host, L"127.0.0.1") == 0 || wcscmp(host, L"localhost") == 0) {
        hr  = CLSIDFromProgID(prog_id, clsid);
        ret = 0;
        goto clean_up;
    }

    memset(&svr_info, 0, sizeof(svr_info));
    svr_info.pwszName = (wchar_t *) host;

    memset(qi_list, 0, sizeof(qi_list));
    qi_list[0].pIID = &IID_IOPCServerList;

    /* 获取 IID_IClassFactory */
    hr = CoCreateInstanceEx(&CLSID_IOPCServerList, 0, CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER, &svr_info,
                            sizeof(qi_list) / sizeof(MULTI_QI), qi_list);
    if (FAILED(hr)) {
        trace_debug("can not create CLSID_IOPCServerList\n");
        goto clean_up;
    }
    server_list = (IOPCServerList *) qi_list[0].pItf;

    /*获取 IID_CATID_OPCDAServer20*/
    hr = server_list->lpVtbl->EnumClassesOfCategories(server_list, 1, &cat_id, 1, &cat_id, &ienum_guid);
    if (FAILED(hr)) {
        trace_debug("can not run EnumClassesOfCategories(IID_CATID_OPCDAServer20) 0x%X\n", hr);
        goto clean_up;
    }

    while ((hr = ienum_guid->lpVtbl->Next(ienum_guid, 1, clsid, &fetched)) == S_OK) {
        LPOLESTR remote_prog_id;
        LPOLESTR remote_user_type;
        hr = server_list->lpVtbl->GetClassDetails(server_list, clsid, &remote_prog_id, &remote_user_type);
        if (hr == S_OK) {
            if (wcscmp(prog_id, remote_prog_id) == 0) {
                /* find it */
                ret = 0;
                CoTaskMemFree(remote_prog_id);
                CoTaskMemFree(remote_user_type);
                break;
            }
            /* wtrace_debug(L"OPCServer prog_id: %ls \tuser_type: %ls\n", prog_id, user_type); */
            CoTaskMemFree(remote_prog_id);
            CoTaskMemFree(remote_user_type);
        }
    }
clean_up:
    if (ienum_guid) {
        ienum_guid->lpVtbl->Release(ienum_guid);
    }

    if (server_list) {
        server_list->lpVtbl->Release(server_list);
    }
    return ret;
}

static OPCHANDLE opcda2_server_get_group_handle() {
    static volatile LONG s_grp_handle = 0;
    OPCHANDLE            handle;

    handle = InterlockedIncrement(&s_grp_handle);

    return handle;
}

int opcda2_serverlist_fetch(const wchar_t *host, int *size, server_info **info_list) {
    int             ret = 1;
    HRESULT         hr;
    IOPCServerList *server_list = NULL;
    IEnumGUID *     ienum_guid  = NULL;
    IID             cat_id      = IID_CATID_OPCDAServer20;
    GUID            clsid;
    ULONG           fetched;
    MULTI_QI        qi_list[1];
    COSERVERINFO    svr_info;

    *info_list = NULL;
    *size      = 0;

    memset(&svr_info, 0, sizeof(svr_info));
    svr_info.pwszName = (wchar_t *) host;

    memset(qi_list, 0, sizeof(qi_list));
    qi_list[0].pIID = &IID_IOPCServerList;

    /* 获取 IID_IClassFactory */
    hr = CoCreateInstanceEx(&CLSID_IOPCServerList, 0, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER | CLSCTX_REMOTE_SERVER,
                            &svr_info, sizeof(qi_list) / sizeof(MULTI_QI), qi_list);
    if (FAILED(hr)) {
        trace_debug("can not create CLSID_IOPCServerList\n");
        goto clean_up;
    }
    server_list = (IOPCServerList *) qi_list[0].pItf;

    /*获取 IID_CATID_OPCDAServer20*/
    hr = server_list->lpVtbl->EnumClassesOfCategories(server_list, 1, &cat_id, 1, &cat_id, &ienum_guid);
    if (FAILED(hr)) {
        trace_debug("can not run EnumClassesOfCategories(IID_CATID_OPCDAServer20) %lu\n", hr);
        goto clean_up;
    }

    /* 统计数量 */
    while ((hr = ienum_guid->lpVtbl->Next(ienum_guid, 1, &clsid, &fetched)) == S_OK) {
        (*size)++;
    }
    /* 设置内存 */
    /* printf("size = %d\n", *size); */
    *info_list = (server_info *) CoTaskMemAlloc(sizeof(server_info) * (*size));
    memset(*info_list, 0, sizeof(server_info) * (*size));

    /* 重置enum */
    ienum_guid->lpVtbl->Reset(ienum_guid);
    *size = 0;

    while ((hr = ienum_guid->lpVtbl->Next(ienum_guid, 1, &clsid, &fetched)) == S_OK) {
        LPOLESTR prog_id;
        LPOLESTR user_type;
        hr = server_list->lpVtbl->GetClassDetails(server_list, &clsid, &prog_id, &user_type);
        if (hr == S_OK) {
            info_list[*size]->prog_id   = prog_id;
            info_list[*size]->user_type = user_type;
            /* wtrace_debug(L"OPCServer prog_id: %ls \tuser_type: %ls\n", prog_id, user_type); */
        }
        ++(*size);
    }
    ret = 0;
clean_up:

    if (ienum_guid) {
        ienum_guid->lpVtbl->Release(ienum_guid);
    }

    if (server_list) {
        server_list->lpVtbl->Release(server_list);
    }

    return ret;
}

void opcda2_serverlist_clear(int size, server_info *info_list) {
    int i;
    for (i = 0; i < size; ++i) {
        server_info *info = info_list + i;
        CoTaskMemFree(info->prog_id);
        CoTaskMemFree(info->user_type);
    }
    CoTaskMemFree(info_list);
}

int opcda2_connect_init(server_connect *c, const wchar_t *host, const wchar_t *prog_id, data_list *items) {
    int                           ret = 1;
    HRESULT                       hr;
    COSERVERINFO                  svr_info;
    CLSID                         cls_id;
    MULTI_QI                      qi_list[2];
    OPCSERVERSTATUS *             svr_status = NULL;
    IOPCServer *                  svr        = NULL;
    IOPCBrowseServerAddressSpace *browse     = NULL;

    memset(&svr_info, 0, sizeof(svr_info));
    svr_info.pwszName = (wchar_t *) host;

    memset(qi_list, 0, sizeof(qi_list));
    qi_list[0].pIID = &IID_IOPCServer;
    qi_list[1].pIID = &IID_IOPCBrowseServerAddressSpace;

    /* 清空 connect */
    memset((char *) c + offsetof(server_connect, cookie), 0, sizeof(server_connect) - offsetof(server_connect, cookie));

    /* 设置 connect 常量 */
    c->datas = items;
    memcpy(c->host, host, wcslen(host) * sizeof(wchar_t));
    memcpy(c->prog_id, prog_id, wcslen(prog_id) * sizeof(wchar_t));

    /* 1. 获取COM cls_id */
    hr = opcda2_clsid(host, prog_id, &cls_id);
    if (hr != 0) {
        wtrace_debug(L"opcda2_clsid host(%ls) prog_id(%ls) failed\n", host, prog_id);
        goto clean_up;
    }

    /* 2 创建OPCServer对象 */
    hr = CoCreateInstanceEx(&cls_id, 0, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER | CLSCTX_REMOTE_SERVER, &svr_info,
                            sizeof(qi_list) / sizeof(MULTI_QI), qi_list);
    if (FAILED(hr)) {
        trace_debug("CoCreateInstanceEx(%ls, IOPCServer) failed %ld\n", host, hr);
        goto clean_up;
    }
    svr    = (IOPCServer *) qi_list[0].pItf;
    browse = (IOPCBrowseServerAddressSpace *) qi_list[1].pItf;

    if (svr == NULL) goto clean_up;

        /* 2.1 获取Server状态 */
#if 1
    hr = svr->lpVtbl->GetStatus(svr, &svr_status);
    if (SUCCEEDED(hr)) {
        wtrace_debug(L"connect server vendor: %ls\n", svr_status->szVendorInfo);
        trace_debug("connect server status: %d\n", (int) svr_status->dwServerState);
        trace_debug("connect server groups: %d\n", (int) svr_status->dwGroupCount);

        CoTaskMemFree(svr_status->szVendorInfo);
        CoTaskMemFree(svr_status);
    }
#endif
/* 3 IOPCBrowseServerAddressSpace */
#if 1
    if (browse) {
        OPCNAMESPACETYPE ns;
        IEnumString *    enum_str = NULL;

        /* 3.1 获取组织方式 */
        browse->lpVtbl->QueryOrganization(browse, &ns);
        trace_debug("svr OPCNAMESPACETYPE %d\n", ns);

        /* 3.2 获取 枚举 Itemid */
        hr = browse->lpVtbl->BrowseOPCItemIDs(browse, OPC_FLAT, L"", VT_EMPTY, OPC_READABLE | OPC_WRITEABLE, &enum_str);
        if (SUCCEEDED(hr)) {
            trace_debug("browse OPCItem IDs\n");
            for (;;) {
                LPOLESTR   strs[64];
                ULONG      count = 0;
                item_info *new_list;
                ULONG      mem_size;
                unsigned   i;

                /* 分配内存 */
                mem_size = sizeof(item_info) * (64 + c->item_size);
                new_list = (item_info *) CoTaskMemRealloc(c->item_list, mem_size);
                if (new_list == NULL) {
                    trace_debug("realloc failed %lu\n", mem_size);
                    CoTaskMemFree(c->item_list);
                    c->item_size = 0;
                    break;
                } else {
                    /* 指向新的内存地址 */
                    c->item_list = new_list;
                }

                /* 获取 Id列表 */
                hr = enum_str->lpVtbl->Next(enum_str, 64, strs, &count);
                if (hr == S_FALSE || count == 0) {
                    break;
                }

                for (i = 0; i < count; ++i) {
                    LPOLESTR str = strs[i];
                    if (str == NULL) continue;
                    if (wcslen(str) >= OPCDA2_ITEM_ID_LEN) {
                        wtrace_debug(L"item id(%ls) length >= 32\n", str);
                    } else {
                        wtrace_debug(L"item id(%ls)\n", str);
                        wcscpy(c->item_list[c->item_size].id, str);
                        c->item_size++;
                    }
                    CoTaskMemFree(str);
                    strs[i] = NULL;
                }
            }
            enum_str->lpVtbl->Release(enum_str);
        } else {
            trace_debug("svr BrowseOPCItemIDs failed %ld\n", hr);
        }

        /* 3.3 获取 枚举 accesspath */
        hr = browse->lpVtbl->BrowseAccessPaths(browse, L"", &enum_str);
        if (!FAILED(hr)) {
            for (;;) {
                LPOLESTR strs[64];
                ULONG    count = 0;
                unsigned i;

                hr = enum_str->lpVtbl->Next(enum_str, 64, strs, &count);
                if (hr == S_FALSE || count == 0) {
                    break;
                }

                for (i = 0; i < count; ++i) {
                    LPOLESTR str = strs[i];
                    if (str) {
                        wtrace_debug(L"item path: %ls\n", str);
                        CoTaskMemFree(str);
                        strs[i] = NULL;
                    }
                }
            }
            enum_str->lpVtbl->Release(enum_str);
        } else {
            trace_debug("svr BrowseAccessPaths failed %ld\n", hr);
        }
    } else {
        trace_debug("connect QueryInterface(IID_IOPCBrowseServerAddressSpace) failed %ld\n", hr);
    }
#endif

    /* 初始化成功 */
    c->svr = svr;
    ret    = 0;
clean_up:
    if (browse) browse->lpVtbl->Release(browse);
    if (ret != 0 && svr) svr->lpVtbl->Release(svr);
    return ret;
}
int opcda2_connect_add(connect_list *clist, const wchar_t *host, const wchar_t *prog_id, data_list *items,
                       server_connect **conn) {
    int             ret = 1;
    server_connect *c   = NULL;
    unsigned int    hash;
    unsigned int    handle;

    /* 1. 初始化 */
    *conn = NULL;

    /* 计算 server hash */
    hash = eal_hash32_fnv1a(host, wcslen(host) * sizeof(wchar_t));
    hash = iclist_connect_list_hash(prog_id, wcslen(prog_id) * sizeof(wchar_t), hash);

    /* 插入 散列表 */
    handle = iclist_connect_list_add(clist, hash);
    if (handle == ICLIST_INVALID_HANDLE) {
        goto clean_up;
    }
    c = iclist_data(clist, handle);

    /* 初始化connect */
    ret = opcda2_connect_init(c, host, prog_id, items);
    if (ret != 0) {
        trace_debug("opcda2_connect_init failed\n");
        goto clean_up;
    }
#if 1
    /* 4 创建 客户端接口 回调对象 */
    opcda2_callback_create(clist, c, &c->callback);
    /* 4.1 注册 通知 */
    opcda2_server_advise(c, NULL);
#endif
    /* 设置返回值 */
    *conn = c;

clean_up:
    /* 清除已插入的对象 */
    if (*conn == NULL && c != NULL) {
        iclist_connect_list_del(clist, handle);
        opcda2_connect_del(clist, c);
    }

    return ret;
}

void opcda2_connect_del(connect_list *clist, server_connect *conn) {
    int i;

    trace_debug("call opcda2_connect_del\n");

    /* 1. 注销 通知 Shutdown */
    opcda2_server_unadvise_shutdown(conn);

    /* 1.1 清除 callback IUnknown */
    if (conn->callback) {
        conn->callback->lpVtbl->Release(conn->callback);
        conn->callback = NULL;
        trace_debug("conn del callback\n");
    }

    /* 2. 清除 groups */
    for (i = 0; i < OPCDA2_SERVER_GROUP_MAX; ++i) {
        group *grp = conn->grp + i;
        opcda2_group_del(conn, grp);
    }
    conn->grp_size = 0;

    /* 3. 清除 item_lists */
    if (conn->item_list) {
        CoTaskMemFree(conn->item_list);
        conn->item_list = NULL;
    }

    /* 4. 清除 IOPCServer */
    if (conn->svr) {
        conn->svr->lpVtbl->Release(conn->svr);
        conn->svr = NULL;
        trace_debug("conn del svr\n");
    }

    iclist_connect_list_del(clist, conn->handle);
}

int opcda2_server_advise(server_connect *conn, IUnknown *cb) {
    int ret;
    int i;

    /* 注册 server 通知 Shutdown */
    ret = opcda2_server_advise_shutdown(conn, cb);

    /* 注册 group  通知 DataCallback */
    for (i = 0; i < OPCDA2_SERVER_GROUP_MAX; ++i) {
        group *grp = conn->grp + i;
        if (grp->used) opcda2_group_advise_callback(grp, conn->callback);
    }
    return ret;
}

void opcda2_server_unadvise(server_connect *conn) {
    int i;
    /* 取消 server 通知 Shutdown */
    opcda2_server_unadvise_shutdown(conn);

    /* 取消 group  通知 DataCallback */
    for (i = 0; i < OPCDA2_SERVER_GROUP_MAX; ++i) {
        group *grp = conn->grp + i;
        if (grp->used) opcda2_group_unadvise_callback(grp);
    }
}

void opcda2_server_unadvise_shutdown(server_connect *conn) {
    HRESULT                    hr;
    IConnectionPointContainer *cp_ter = NULL;
    IConnectionPoint *         cp     = NULL;
    IOPCServer *               svr    = conn->svr;
    DWORD                      cookie = conn->cookie;

    conn->cookie = 0;

    if (cookie && svr) {
        /* 1 获取 IConnectionPointContainer */
        hr = svr->lpVtbl->QueryInterface(svr, &IID_IConnectionPointContainer, (void **) &cp_ter);
        if (FAILED(hr)) {
            trace_debug("server QueryInterface(IID_IConnectionPointContainer) faild %ld\n", hr);
            goto clean_up;
        }

        /* 2 获取 IOPCShutdown */
        hr = cp_ter->lpVtbl->FindConnectionPoint(cp_ter, &IID_IOPCShutdown, &cp);
        if (FAILED(hr)) {
            trace_debug("server FindConnectionPoint(IID_IOPCShutdown) faild %ld\n", hr);
            goto clean_up;
        }

        /* 3 取消通知 */
        hr = cp->lpVtbl->Unadvise(cp, cookie);
        if (FAILED(hr)) {
            trace_debug("server Unadvise(IID_IOPCShutdown) faild %ld\n", hr);
            goto clean_up;
        }
    }
clean_up:
    if (cp) cp->lpVtbl->Release(cp);
    if (cp_ter) cp_ter->lpVtbl->Release(cp_ter);
}

/* 注册 通知 shutown */
int opcda2_server_advise_shutdown(server_connect *conn, IUnknown *sd) {
    int                        ret = 1;
    HRESULT                    hr;
    IConnectionPointContainer *cp_ter = NULL;
    IConnectionPoint *         cp     = NULL;
    IOPCServer *               svr    = conn->svr;

    if (svr == NULL) goto clean_up;

    if (sd == NULL) {
        /* 之前没有callback，返回 */
        if (conn->callback == NULL) return ret;

        /* 使用之前设置的callback */
        sd = conn->callback;
    } else {
        /* 释放之前的callback */
        if (conn->callback) {
            conn->callback->lpVtbl->Release(conn->callback);
            conn->callback = NULL;
        }
        /* 使用设置的sd */
        conn->callback = sd;
        sd->lpVtbl->AddRef(sd);
    }

    /* 1 获取 IConnectionPointContainer */
    hr = svr->lpVtbl->QueryInterface(svr, &IID_IConnectionPointContainer, (void **) &cp_ter);
    if (FAILED(hr)) {
        trace_debug("server QueryInterface(IID_IConnectionPointContainer) faild %ld\n", hr);
        goto clean_up;
    }

    /* 2 获取 IOPCShutdown */
    hr = cp_ter->lpVtbl->FindConnectionPoint(cp_ter, &IID_IOPCShutdown, &cp);
    if (FAILED(hr)) {
        trace_debug("server FindConnectionPoint(IID_IOPCShutdown) faild %ld\n", hr);
        goto clean_up;
    }

    /* 2.1 首先注销以前的 cookie */
    if (conn->cookie) {
        hr = cp->lpVtbl->Unadvise(cp, conn->cookie);
    }
    /* 3 注册通知 */
    hr = cp->lpVtbl->Advise(cp, sd, &conn->cookie);
    if (FAILED(hr)) {
        trace_debug("server Advise(IID_IOPCShutdown) faild %ld\n", hr);
        goto clean_up;
    }

    ret = 0;
clean_up:

    if (cp) {
        cp->lpVtbl->Release(cp);
    }

    if (cp_ter) {
        cp_ter->lpVtbl->Release(cp_ter);
    }
    return ret;
}

int opcda2_group_add(server_connect *conn, const wchar_t *name, int active, int rate, group **grp) {
    int                        ret        = 1;
    HRESULT                    hr         = 0;
    OPCHANDLE                  cli_group  = 0;
    OPCHANDLE                  svr_handle = 0;
    DWORD                      svr_rate   = 0;
    IOPCItemMgt *              itm_mgt    = NULL;
    IOPCGroupStateMgt *        grp_mgt    = NULL;
    IConnectionPointContainer *cp_ter     = NULL;
    IConnectionPoint *         cp         = NULL;
    IOPCAsyncIO2 *             async_io2  = NULL;
    group *                    new_grp    = NULL;
    FLOAT                      dead_band  = 0;
    const DWORD                locale_id  = 0x409; /* english */
    int                        i;
    size_t                     name_len;

    *grp = NULL;

    if (conn == NULL) return ret;
    if (conn->svr == NULL) return ret;

    if (wcslen(name) >= 32) {
        wtrace_debug(L"group name(%ls) too long", name);
        return ret;
    }
    if (conn->grp_size >= OPCDA2_SERVER_GROUP_MAX) {
        trace_debug("group list is full\n");
        return ret;
    }
    /* group 是否已经存在 */
    for (i = 0; i < OPCDA2_SERVER_GROUP_MAX; ++i) {
        group *cur_grp = conn->grp + i;
        if (wcscmp(cur_grp->name, name) == 0 && cur_grp->used) {
            wtrace_debug(L"group name(%ls) already added\n", name);
            *grp = cur_grp;
            return ret;
        } else if (cur_grp->used == 0 && new_grp == NULL) {
            new_grp = cur_grp;
        }
    }

    /* 找不到可插入组 */
    if (new_grp == NULL) goto clean_up;

    cli_group = opcda2_server_get_group_handle();

    /* 1 向server增加组 */
    hr = conn->svr->lpVtbl->AddGroup(conn->svr, name, active, rate, cli_group, NULL, &dead_band, locale_id, &svr_handle,
                                     &svr_rate, &IID_IOPCGroupStateMgt, (IUnknown **) &grp_mgt);
    if (FAILED(hr)) {
        goto clean_up;
    }

    /* 2 获取Group 接口 */
    /* ItemMgt */
    hr = grp_mgt->lpVtbl->QueryInterface(grp_mgt, &IID_IOPCItemMgt, (void **) &itm_mgt);
    if (FAILED(hr)) {
        goto clean_up;
    }

    hr = grp_mgt->lpVtbl->QueryInterface(grp_mgt, &IID_IConnectionPointContainer, (void **) &cp_ter);
    if (FAILED(hr)) {
        goto clean_up;
    }

    /*ConnectPoint*/
    hr = cp_ter->lpVtbl->FindConnectionPoint(cp_ter, &IID_IOPCDataCallback, &cp);
    if (FAILED(hr)) {
        goto clean_up;
    }

    /*AsyncIO2*/
    hr = grp_mgt->lpVtbl->QueryInterface(grp_mgt, &IID_IOPCAsyncIO2, (void **) &async_io2);
    if (FAILED(hr)) {
        trace_debug("server dont support IOPCAsyncIO2\n");
    }

    /* 初始化group 对象 */
    name_len = wcslen(name);
    memset(new_grp, 0, sizeof(group));
    memcpy(new_grp->name, name, sizeof(wchar_t) * name_len);

    new_grp->used       = 1;
    new_grp->cli_group  = cli_group;
    new_grp->svr_handle = svr_handle;
    new_grp->active     = active;
    new_grp->rate       = svr_rate;
    new_grp->conn       = conn;
    new_grp->datas      = conn->datas;
    new_grp->item_size  = 0;
    new_grp->cli_group  = cli_group;
    new_grp->svr_handle = svr_handle;
    new_grp->grp_mgt    = grp_mgt;
    new_grp->itm_mgt    = itm_mgt;
    new_grp->async_io2  = async_io2;
    new_grp->cp         = cp;
    new_grp->salt       = salt_gen();
    new_grp->hash       = eal_hash32_fnv1a_more(name, name_len * sizeof(wchar_t), conn->hash);
    new_grp->hash       = eal_hash32_fnv1a_more(&new_grp->salt, 4, new_grp->hash);

    /* 更新 group 数量 */
    conn->grp_size++;

    /* 注册 callback */
    opcda2_group_advise_callback(new_grp, conn->callback);

    *grp = new_grp;
    ret  = 0;
    wtrace_debug(L"add grp(%ls) handle [%d] hash %u svr %u\n", new_grp->name, new_grp->cli_group, new_grp->hash,
                 conn->hash);
clean_up:
    if (cp_ter) cp_ter->lpVtbl->Release(cp_ter);

    if (ret) {
        if (svr_handle) {
            conn->svr->lpVtbl->RemoveGroup(conn->svr, svr_handle, TRUE);
            svr_handle = 0;
        }
        if (cp) cp->lpVtbl->Release(cp);
        if (itm_mgt) itm_mgt->lpVtbl->Release(itm_mgt);
        if (grp_mgt) grp_mgt->lpVtbl->Release(grp_mgt);
        if (async_io2) async_io2->lpVtbl->Release(async_io2);
    }
    return ret;
}

void opcda2_group_del(server_connect *conn, group *grp) {
    /* 清除grp资源 */

    if (grp == NULL) return;

    if (grp->used == 0) return;

    grp->used = 0;

    /* 取消通知datacallback */
    opcda2_group_unadvise_callback(grp);
    /* 清空全部item */
    opcda2_item_del(grp, OPCDA2_ITEM_ALL, NULL);

    /* 释放 打开的接口 */
    if (grp->grp_mgt) {
        grp->grp_mgt->lpVtbl->Release(grp->grp_mgt);
        grp->grp_mgt = NULL;
    }

    if (grp->itm_mgt) {
        grp->itm_mgt->lpVtbl->Release(grp->itm_mgt);
        grp->itm_mgt = NULL;
    }

    if (grp->cp) {
        grp->cp->lpVtbl->Release(grp->cp);
        grp->cp = NULL;
    }

    if (grp->async_io2) {
        grp->async_io2->lpVtbl->Release(grp->async_io2);
        grp->async_io2 = NULL;
    }

    /* 注销Server端对象 */
    if (grp->svr_handle) {
        conn->svr->lpVtbl->RemoveGroup(conn->svr, grp->svr_handle, TRUE);
        grp->svr_handle = 0;
    }
    wtrace_debug(L"del grp(%ls) handle [%d] hash %u svr %u\n", grp->name, grp->cli_group, grp->hash, conn->hash);
}
