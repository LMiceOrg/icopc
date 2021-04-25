/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include <stddef.h>

#include "callback.h"
#include "server.h"
#include "group.h"
#include "util_trace.h"

typedef struct client_ishundown {
    struct IOPCShutdownVtbl *lpVtbl;
} client_ishundown;

/* IOPCDataCallback (base) object */
typedef struct client_interface {
    struct IOPCDataCallbackVtbl *lpVtbl;    /* IOPCDataCallback(base) */
    client_ishundown             shutdown;  /* IOPCShutdown object */
    volatile LONG                ref_count; /* 引用计数 */
    data_list *                  datas;     /* 数据表 用于 DataCallback */
    server_connect *             conn;      /* 通知连接关闭 用于Shutdown */
} client_interface;

HRESULT STDMETHODCALLTYPE callback_QueryInterface(/* [in] */ IOPCDataCallback *This,
                                                  /* [in] */ REFIID            riid,
                                                  /* [iid_is][out] */ void **  ppvObject);

ULONG STDMETHODCALLTYPE callback_AddRef(IOPCDataCallback *This);

ULONG STDMETHODCALLTYPE callback_Release(IOPCDataCallback *This);

HRESULT STDMETHODCALLTYPE callback_OnDataChange(IOPCDataCallback *             This,
                                                /* [in] */ DWORD               dwTransid,
                                                /* [in] */ OPCHANDLE           hGroup,
                                                /* [in] */ HRESULT             hrMasterquality,
                                                /* [in] */ HRESULT             hrMastererror,
                                                /* [in] */ DWORD               dwCount,
                                                /* [size_is][in] */ OPCHANDLE *phClientItems,
                                                /* [size_is][in] */ VARIANT *  pvValues,
                                                /* [size_is][in] */ WORD *     pwQualities,
                                                /* [size_is][in] */ FILETIME * pftTimeStamps,
                                                /* [size_is][in] */ HRESULT *  pErrors);

HRESULT STDMETHODCALLTYPE callback_OnReadComplete(IOPCDataCallback *             This,
                                                  /* [in] */ DWORD               dwTransid,
                                                  /* [in] */ OPCHANDLE           hGroup,
                                                  /* [in] */ HRESULT             hrMasterquality,
                                                  /* [in] */ HRESULT             hrMastererror,
                                                  /* [in] */ DWORD               dwCount,
                                                  /* [size_is][in] */ OPCHANDLE *phClientItems,
                                                  /* [size_is][in] */ VARIANT *  pvValues,
                                                  /* [size_is][in] */ WORD *     pwQualities,
                                                  /* [size_is][in] */ FILETIME * pftTimeStamps,
                                                  /* [size_is][in] */ HRESULT *  pErrors);

HRESULT STDMETHODCALLTYPE callback_OnWriteComplete(IOPCDataCallback *             This,
                                                   /* [in] */ DWORD               dwTransid,
                                                   /* [in] */ OPCHANDLE           hGroup,
                                                   /* [in] */ HRESULT             hrMastererr,
                                                   /* [in] */ DWORD               dwCount,
                                                   /* [size_is][in] */ OPCHANDLE *pClienthandles,
                                                   /* [size_is][in] */ HRESULT *  pErrors);

HRESULT STDMETHODCALLTYPE callback_OnCancelComplete(IOPCDataCallback *   This,
                                                    /* [in] */ DWORD     dwTransid,
                                                    /* [in] */ OPCHANDLE hGroup);

HRESULT STDMETHODCALLTYPE callback_QueryInterface(IOPCDataCallback *         self,
                                                  /* [in] */ REFIID          riid,
                                                  /* [iid_is][out] */ void **ppv) {
    HRESULT           ret  = NOERROR;
    client_interface *base = NULL;

    *ppv = NULL;
    base = (client_interface *) (self);

#ifdef DEBUG
    LPOLESTR iid_name;

    StringFromIID(riid, &iid_name);
    wtrace_debug(L"IOPCDataCallback Query Interface %ls\n", iid_name);
    CoTaskMemFree(iid_name);
#endif

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IOPCDataCallback)) {
        *ppv = self;
        /* 引用计数 +1 */
        self->lpVtbl->AddRef(self);
    } else if (IsEqualIID(riid, &IID_IOPCShutdown)) {
        *ppv = &base->shutdown;
        /* 引用计数 +1 */
        self->lpVtbl->AddRef(self);
    } else {
        ret = E_NOINTERFACE;
    }
    return ret;
}

ULONG STDMETHODCALLTYPE callback_AddRef(IOPCDataCallback *self) {
    LONG              count;
    client_interface *base;

    base  = (client_interface *) (self);
    count = InterlockedIncrement(&base->ref_count);
    trace_debug("AddRef %ld\n", count);
    return count;
}

ULONG STDMETHODCALLTYPE callback_Release(IOPCDataCallback *self) {
    LONG              count;
    client_interface *base;

    base  = (client_interface *) (self);
    count = InterlockedDecrement(&base->ref_count);
    if (count == 0) {
        CoTaskMemFree(self);
    }
    trace_debug("Release %ld\n", count);
    return count;
}

HRESULT STDMETHODCALLTYPE callback_OnDataChange(IOPCDataCallback *             self,
                                                /* [in] */ DWORD               dwTransid,
                                                /* [in] */ OPCHANDLE           hGroup,
                                                /* [in] */ HRESULT             hrMasterquality,
                                                /* [in] */ HRESULT             hrMastererror,
                                                /* [in] */ DWORD               dwCount,
                                                /* [size_is][in] */ OPCHANDLE *phClientItems,
                                                /* [size_is][in] */ VARIANT *  pvValues,
                                                /* [size_is][in] */ WORD *     pwQualities,
                                                /* [size_is][in] */ FILETIME * pftTimeStamps,
                                                /* [size_is][in] */ HRESULT *  pErrors) {
    client_interface *base;
    DWORD tid;
    DWORD i;

    base = (client_interface *) (self);
    tid = GetCurrentThreadId();


    trace_debug("th[%ld] group: %lu\tOnDataChange[%lu] item count: %lu\n", tid, hGroup, dwTransid, dwCount);
    for (i = 0; i < dwCount; ++i) {
        unsigned int handle = phClientItems[i];
        item_data *  data;

        if (handle >= OPCDA2_ITEM_DATA_MAX + OPCDA2_EXTRA_DATA_MAX) continue;

        data = opcda2_data(base->datas, handle);

        VariantClear(&data->value);
        VariantCopy(&data->value, pvValues + i);
        memcpy(&(data->ts), pftTimeStamps, sizeof(FILETIME));

        if (data->value.vt == VT_R8) {
            wtrace_debug(L" item[%X]: %ls,\t[%d]ivalue: %.04lf, quality:%d, hr:%ld\n", handle, data->id,
                         data->value.vt, data->value.dblVal, pwQualities[i], pErrors[i]);
        } else {
            wtrace_debug(L" item[%X]: %ls,\t[%d]ivalue: %d, quality:%d, hr:%ld\n", handle, data->id, data->value.vt,
                         data->value.intVal, pwQualities[i], pErrors[i]);
        }
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE callback_OnReadComplete(IOPCDataCallback *             self,
                                                  /* [in] */ DWORD               dwTransid,
                                                  /* [in] */ OPCHANDLE           hGroup,
                                                  /* [in] */ HRESULT             hrMasterquality,
                                                  /* [in] */ HRESULT             hrMastererror,
                                                  /* [in] */ DWORD               dwCount,
                                                  /* [size_is][in] */ OPCHANDLE *phClientItems,
                                                  /* [size_is][in] */ VARIANT *  pvValues,
                                                  /* [size_is][in] */ WORD *     pwQualities,
                                                  /* [size_is][in] */ FILETIME * pftTimeStamps,
                                                  /* [size_is][in] */ HRESULT *  pErrors) {
    client_interface *base;
    DWORD i;

    base = (client_interface *) (self);
    trace_debug("group: %lu\tOnReadComplete[%lu] item count: %lu\n", hGroup, dwTransid, dwCount);
    for (i = 0; i < dwCount; ++i) {
        unsigned int handle = phClientItems[i];
        item_data *  data;

        if (handle >= OPCDA2_ITEM_DATA_MAX + OPCDA2_EXTRA_DATA_MAX) continue;

        data = opcda2_data(base->datas, handle);

        VariantClear(&data->value);
        VariantCopy(&data->value, pvValues + i);
        memcpy(&(data->ts), pftTimeStamps, sizeof(FILETIME));

        if (data->value.vt == VT_R8) {
            wtrace_debug(L" item[%X]: %ls\t[%d]ivalue: %.04lf, quality:%d, hr:%ld\n", handle, data->id,
                         data->value.vt, data->value.dblVal, pwQualities[i], pErrors[i]);
        } else {
            wtrace_debug(L" item[%X]: %ls\t[%d]ivalue: %d, quality:%d, hr:%ld\n", handle, data->id, data->value.vt,
                         data->value.intVal, pwQualities[i], pErrors[i]);
        }
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE callback_OnWriteComplete(IOPCDataCallback *             self,
                                                   /* [in] */ DWORD               dwTransid,
                                                   /* [in] */ OPCHANDLE           hGroup,
                                                   /* [in] */ HRESULT             hrMastererr,
                                                   /* [in] */ DWORD               dwCount,
                                                   /* [size_is][in] */ OPCHANDLE *pClienthandles,
                                                   /* [size_is][in] */ HRESULT *  pErrors) {
    client_interface *base;
    DWORD             i;

    base = (client_interface *) (self);
    trace_debug("group: %lu\tOnWriteComplete[%lu] item count: %lu\n", hGroup, dwTransid, dwCount);
    for (i = 0; i < dwCount; ++i) {
        unsigned int handle = pClienthandles[i];
        item_data *  data;
        if (handle >= OPCDA2_ITEM_DATA_MAX + OPCDA2_EXTRA_DATA_MAX) continue;
        data = opcda2_data(base->datas, handle);
        wtrace_debug(L" item[%ls] write %X\n", data->id, pErrors[i]);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE callback_OnCancelComplete(IOPCDataCallback *   self,
                                                    /* [in] */ DWORD     dwTransid,
                                                    /* [in] */ OPCHANDLE hGroup) {
    return S_OK;
}

/******** Shutdown *******/

HRESULT STDMETHODCALLTYPE shutdown_QueryInterface(IOPCShutdown *             self,
                                                  /* [in] */ REFIID          riid,
                                                  /* [iid_is][out] */ void **ppv) {
    HRESULT ret = NOERROR;

#ifdef DEBUG
    LPOLESTR iid_name;

    StringFromIID(riid, &iid_name);
    wtrace_debug(L"IOPCShutdown Query Interface %ls\n", iid_name);
    CoTaskMemFree(iid_name);
#endif

    /* 请求IUnknown， OPCShutdown，则返回self */
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IOPCShutdown)) {
        *ppv = self;
        /* 引用计数 +1 */
        self->lpVtbl->AddRef(self);
    } else if (IsEqualIID(riid, &IID_IOPCDataCallback)) {
        /* 请求 DataCallback， 则返回 base */
        IOPCDataCallback *base = (IOPCDataCallback *) ((unsigned char *) self - offsetof(client_interface, shutdown));
        *ppv                   = base;
        /* 引用计数 +1 */
        base->lpVtbl->AddRef(base);
    } else {
        *ppv = NULL;
        ret  = E_NOINTERFACE;
    }

    return ret;
}

ULONG STDMETHODCALLTYPE shutdown_AddRef(IOPCShutdown *self) {
    IOPCDataCallback *base;

    base = (IOPCDataCallback *) ((unsigned char *) self - offsetof(client_interface, shutdown));
    return base->lpVtbl->AddRef(base);
}

ULONG STDMETHODCALLTYPE shutdown_Release(IOPCShutdown *self) {
    IOPCDataCallback *base;

    base = (IOPCDataCallback *) ((unsigned char *) self - offsetof(client_interface, shutdown));
    return base->lpVtbl->Release(base);
}

HRESULT STDMETHODCALLTYPE shutdown_ShutdownRequest(IOPCShutdown *             self,
                                                   /* [string][in] */ LPCWSTR reason) {
    client_interface *base;

    base = (client_interface *) ((unsigned char *) self - offsetof(client_interface, shutdown));
    if (base->conn) {
        wtrace_debug(L"host[%ls] server[%ls] shutdown :%ls, group count:%ld \n", base->conn->host, base->conn->prog_id,
                     reason, base->conn->grp_size);
        opcda2_server_disconnect(base->conn);
        base->conn = NULL;
    }

    return S_OK;
}

/* 静态 DataCallback 函数表结构体  */
static IOPCDataCallbackVtbl private_datacallback_vtbl = {
    callback_QueryInterface, callback_AddRef,          callback_Release,         callback_OnDataChange,
    callback_OnReadComplete, callback_OnWriteComplete, callback_OnCancelComplete};

/* 静态 Shutdown 函数表结构体  */
static IOPCShutdownVtbl private_shutdown_vtbl = {shutdown_QueryInterface, shutdown_AddRef, shutdown_Release,
                                                 shutdown_ShutdownRequest};

/* 创建callback对象 */
int opcda2_callback_create(struct server_connect* conn, IUnknown **cb) {
    client_interface *it;

    /* 创建对象内存 */
    it = (client_interface *) CoTaskMemAlloc(sizeof(client_interface));
    memset(it, 0, sizeof(client_interface));

    *cb = (IUnknown *) it;

    /* 初始化设置 VTable */
    it->lpVtbl          = &private_datacallback_vtbl;
    it->shutdown.lpVtbl = &private_shutdown_vtbl;

    it->conn = conn;
    it->datas = conn->datas;

    /* 增加引用计数 */
    it->lpVtbl->AddRef((IOPCDataCallback *) it);

    return S_OK;
}
