/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include <stddef.h>

#include "callback.h"
#include "group.h"
#include "trace.h"

typedef struct client_ishundown {
    struct IOPCShutdownVtbl *lpVtbl;
} client_ishundown;

/* IOPCDataCallback (base) object */
typedef struct client_interface {
    struct IOPCDataCallbackVtbl *lpVtbl;    /* IOPCDataCallback(base) */
    client_ishundown             shutdown;  /* IOPCShutdown object */
    volatile LONG                ref_count; /* 引用计数 */
    data_list *                  datas;     /* 数据表 */
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

    base = (client_interface *) (self);
    trace_debug("group: %lu\tOnDataChange: %lu\n", hGroup, dwCount);
    for (DWORD i = 0; i < dwCount; ++i) {
        unsigned int handle = phClientItems[i];
        item_data *  data;
        data = opcda2_data_find(base->datas, handle);
        if (data == NULL) continue;

        VariantClear(&data->value);
        VariantCopy(&data->value, pvValues + i);

        wtrace_debug(L"group [%d] item %ls:\tvalue %d\n", hGroup, data->id, data->value.intVal);
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
    return S_OK;
}

HRESULT STDMETHODCALLTYPE callback_OnWriteComplete(IOPCDataCallback *             self,
                                                   /* [in] */ DWORD               dwTransid,
                                                   /* [in] */ OPCHANDLE           hGroup,
                                                   /* [in] */ HRESULT             hrMastererr,
                                                   /* [in] */ DWORD               dwCount,
                                                   /* [size_is][in] */ OPCHANDLE *pClienthandles,
                                                   /* [size_is][in] */ HRESULT *  pErrors) {
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

    wtrace_debug(L"server shutdown :%ls, count:%ld \n", reason, base->ref_count);

    return S_OK;
}

/* 静态 DataCallback 函数表结构体  */
static IOPCDataCallbackVtbl private_datacallback_vtbl = {
    callback_QueryInterface, callback_AddRef,          callback_Release,         callback_OnDataChange,
    callback_OnReadComplete, callback_OnWriteComplete, callback_OnCancelComplete};

/* 静态 Shutdown 函数表结构体  */
static IOPCShutdownVtbl private_shutdown_vtbl = {shutdown_QueryInterface, shutdown_AddRef, shutdown_Release,
                                                 shutdown_ShutdownRequest};

int opcda2_callback_create(IUnknown **cb, data_list *datas) {
    client_interface *it;

    /* 创建对象内存 */
    it = (client_interface *) CoTaskMemAlloc(sizeof(client_interface));
    memset(it, 0, sizeof(client_interface));

    *cb = (IUnknown *) it;

    /* 初始化设置 VTable */
    it->lpVtbl          = &private_datacallback_vtbl;
    it->shutdown.lpVtbl = &private_shutdown_vtbl;

    it->datas = datas;

    /* 增加引用计数 */
    it->lpVtbl->AddRef((IOPCDataCallback *) it);

    return S_OK;
}
