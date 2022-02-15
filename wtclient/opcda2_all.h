/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_C_H
#define OPCDA2_C_H

#include <comcat.h>
#include <objbase.h>
#include <ocidl.h>

#define LIST_DEFAULT_CAPACIRY 32

extern const IID IID_IUnknown;
extern const IID IID_IClassFactory;

extern const IID   IID_IOPCServerList;
extern const CLSID CLSID_IOPCServerList;

extern const IID IID_CATID_OPCDAServer20;

extern const IID IID_IOPCServer;
extern const IID IID_IOPCBrowseServerAddressSpace;

extern const IID IID_IOPCGroupStateMgt;

extern const IID IID_IOPCItemMgt;

extern const IID IID_IOPCDataCallback;
extern const IID IID_IOPCShutdown;

typedef struct IOPCServerList IOPCServerList;

typedef struct IOPCServer IOPCServer;
typedef interface IOPCBrowseServerAddressSpace IOPCBrowseServerAddressSpace;

typedef struct IOPCGroupStateMgt IOPCGroupStateMgt;
typedef struct IOPCItemMgt       IOPCItemMgt;

typedef interface IOPCDataCallback IOPCDataCallback;
typedef interface IOPCShutdown IOPCShutdown;

/** Definition */

typedef enum tagOPCDATASOURCE { OPC_DS_CACHE = 1, OPC_DS_DEVICE = OPC_DS_CACHE + 1 } OPCDATASOURCE;

typedef enum tagOPCBROWSETYPE { OPC_BRANCH = 1, OPC_LEAF = OPC_BRANCH + 1, OPC_FLAT = OPC_LEAF + 1 } OPCBROWSETYPE;

typedef enum tagOPCNAMESPACETYPE { OPC_NS_HIERARCHIAL = 1, OPC_NS_FLAT = OPC_NS_HIERARCHIAL + 1 } OPCNAMESPACETYPE;

typedef enum tagOPCBROWSEDIRECTION {
    OPC_BROWSE_UP   = 1,
    OPC_BROWSE_DOWN = OPC_BROWSE_UP + 1,
    OPC_BROWSE_TO   = OPC_BROWSE_DOWN + 1
} OPCBROWSEDIRECTION;

#define OPC_READABLE 1
#define OPC_WRITEABLE 2
typedef enum tagOPCEUTYPE { OPC_NOENUM = 0, OPC_ANALOG = OPC_NOENUM + 1, OPC_ENUMERATED = OPC_ANALOG + 1 } OPCEUTYPE;

typedef enum tagOPCSERVERSTATE {
    OPC_STATUS_RUNNING   = 1,
    OPC_STATUS_FAILED    = OPC_STATUS_RUNNING + 1,
    OPC_STATUS_NOCONFIG  = OPC_STATUS_FAILED + 1,
    OPC_STATUS_SUSPENDED = OPC_STATUS_NOCONFIG + 1,
    OPC_STATUS_TEST      = OPC_STATUS_SUSPENDED + 1
} OPCSERVERSTATE;

typedef enum tagOPCENUMSCOPE {
    OPC_ENUM_PRIVATE_CONNECTIONS = 1,
    OPC_ENUM_PUBLIC_CONNECTIONS  = OPC_ENUM_PRIVATE_CONNECTIONS + 1,
    OPC_ENUM_ALL_CONNECTIONS     = OPC_ENUM_PUBLIC_CONNECTIONS + 1,
    OPC_ENUM_PRIVATE             = OPC_ENUM_ALL_CONNECTIONS + 1,
    OPC_ENUM_PUBLIC              = OPC_ENUM_PRIVATE + 1,
    OPC_ENUM_ALL                 = OPC_ENUM_PUBLIC + 1
} OPCENUMSCOPE;

typedef DWORD OPCHANDLE;

typedef struct tagOPCGROUPHEADER {
    DWORD     dwSize;
    DWORD     dwItemCount;
    OPCHANDLE hClientGroup;
    DWORD     dwTransactionID;
    HRESULT   hrStatus;
} OPCGROUPHEADER;

typedef struct tagOPCITEMHEADER1 {
    OPCHANDLE hClient;
    DWORD     dwValueOffset;
    WORD      wQuality;
    WORD      wReserved;
    FILETIME  ftTimeStampItem;
} OPCITEMHEADER1;

typedef struct tagOPCITEMHEADER2 {
    OPCHANDLE hClient;
    DWORD     dwValueOffset;
    WORD      wQuality;
    WORD      wReserved;
} OPCITEMHEADER2;

typedef struct tagOPCGROUPHEADERWRITE {
    DWORD     dwItemCount;
    OPCHANDLE hClientGroup;
    DWORD     dwTransactionID;
    HRESULT   hrStatus;
} OPCGROUPHEADERWRITE;

typedef struct tagOPCITEMHEADERWRITE {
    OPCHANDLE hClient;
    HRESULT   dwError;
} OPCITEMHEADERWRITE;

typedef struct tagOPCITEMSTATE {
    OPCHANDLE hClient;
    FILETIME  ftTimeStamp;
    WORD      wQuality;
    WORD      wReserved;
    VARIANT   vDataValue;
} OPCITEMSTATE;

typedef struct tagOPCSERVERSTATUS {
    FILETIME              ftStartTime;
    FILETIME              ftCurrentTime;
    FILETIME              ftLastUpdateTime;
    OPCSERVERSTATE        dwServerState;
    DWORD                 dwGroupCount;
    DWORD                 dwBandWidth;
    WORD                  wMajorVersion;
    WORD                  wMinorVersion;
    WORD                  wBuildNumber;
    WORD                  wReserved;
    /* [string] */ LPWSTR szVendorInfo;
} OPCSERVERSTATUS;

typedef struct tagOPCITEMDEF {
    /* [string] */ LPWSTR szAccessPath;
    /* [string] */ LPWSTR szItemID;
    BOOL                  bActive;
    OPCHANDLE             hClient;
    DWORD                 dwBlobSize;
    /* [size_is] */ BYTE __RPC_FAR *pBlob;
    VARTYPE                         vtRequestedDataType;
    WORD                            wReserved;
} OPCITEMDEF;

typedef struct tagOPCITEMATTRIBUTES {
    /* [string] */ LPWSTR szAccessPath;
    /* [string] */ LPWSTR szItemID;
    BOOL                  bActive;
    OPCHANDLE             hClient;
    OPCHANDLE             hServer;
    DWORD                 dwAccessRights;
    DWORD                 dwBlobSize;
    /* [size_is] */ BYTE __RPC_FAR *pBlob;
    VARTYPE                         vtRequestedDataType;
    VARTYPE                         vtCanonicalDataType;
    OPCEUTYPE                       dwEUType;
    VARIANT                         vEUInfo;
} OPCITEMATTRIBUTES;

typedef struct tagOPCITEMRESULT {
    OPCHANDLE            hServer;
    VARTYPE              vtCanonicalDataType;
    WORD                 wReserved;
    DWORD                dwAccessRights;
    DWORD                dwBlobSize;
    /* [size_is] */ BYTE __RPC_FAR *pBlob;
} OPCITEMRESULT;

#define OPC_QUALITY_MASK 0xC0
#define OPC_STATUS_MASK 0xFC
#define OPC_LIMIT_MASK 0x03
#define OPC_QUALITY_BAD 0x00
#define OPC_QUALITY_UNCERTAIN 0x40
#define OPC_QUALITY_GOOD 0xC0
#define OPC_QUALITY_CONFIG_ERROR 0x04
#define OPC_QUALITY_NOT_CONNECTED 0x08
#define OPC_QUALITY_DEVICE_FAILURE 0x0c
#define OPC_QUALITY_SENSOR_FAILURE 0x10
#define OPC_QUALITY_LAST_KNOWN 0x14
#define OPC_QUALITY_COMM_FAILURE 0x18
#define OPC_QUALITY_OUT_OF_SERVICE 0x1C
#define OPC_QUALITY_LAST_USABLE 0x44
#define OPC_QUALITY_SENSOR_CAL 0x50
#define OPC_QUALITY_EGU_EXCEEDED 0x54
#define OPC_QUALITY_SUB_NORMAL 0x58
#define OPC_QUALITY_LOCAL_OVERRIDE 0xD8
#define OPC_LIMIT_OK 0x00
#define OPC_LIMIT_LOW 0x01
#define OPC_LIMIT_HIGH 0x02
#define OPC_LIMIT_CONST 0x03

typedef struct IOPCServerListVtbl {
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE *QueryInterface)
    (IOPCServerList *           This,
     /* [in] */ REFIID          riid,
     /* [iid_is][out] */ void **ppvObject);

    ULONG(STDMETHODCALLTYPE *AddRef)(IOPCServerList *This);

    ULONG(STDMETHODCALLTYPE *Release)(IOPCServerList *This);

    HRESULT(STDMETHODCALLTYPE *EnumClassesOfCategories)
    (IOPCServerList *          This,
     /* [in] */ ULONG          cImplemented,
     /* [size_is][in] */ CATID rgcatidImpl[],
     /* [in] */ ULONG          cRequired,
     /* [size_is][in] */ CATID rgcatidReq[],
     /* [out] */ IEnumGUID **  ppenumClsid);

    HRESULT(STDMETHODCALLTYPE *GetClassDetails)
    (IOPCServerList *      This,
     /* [in] */ REFCLSID   clsid,
     /* [out] */ LPOLESTR *ppszProgID,
     /* [out] */ LPOLESTR *ppszUserType);

    HRESULT(STDMETHODCALLTYPE *CLSIDFromProgID)
    (IOPCServerList *     This,
     /* [in] */ LPCOLESTR szProgId,
     /* [out] */ LPCLSID  clsid);

    END_INTERFACE
} IOPCServerListVtbl;

interface IOPCServerList {
    CONST_VTBL struct IOPCServerListVtbl __RPC_FAR *lpVtbl;
};

/** IOPCServer */
typedef struct IOPCServerVtbl {
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *QueryInterface)
    (IOPCServer __RPC_FAR *   This,
     /* [in] */ REFIID        riid,
     /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);

    ULONG(STDMETHODCALLTYPE __RPC_FAR *AddRef)(IOPCServer __RPC_FAR *This);

    ULONG(STDMETHODCALLTYPE __RPC_FAR *Release)(IOPCServer __RPC_FAR *This);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *AddGroup)
    (IOPCServer __RPC_FAR *     This,
     /* [string][in] */ LPCWSTR szName,
     /* [in] */ BOOL            bActive,
     /* [in] */ DWORD           dwRequestedUpdateRate,
     /* [in] */ OPCHANDLE       hClientGroup,
     /* [in][unique] */ LONG __RPC_FAR *pTimeBias,
     /* [in][unique] */ FLOAT __RPC_FAR *pPercentDeadband,
     /* [in] */ DWORD                    dwLCID,
     /* [out] */ OPCHANDLE __RPC_FAR *phServerGroup,
     /* [out] */ DWORD __RPC_FAR * pRevisedUpdateRate,
     /* [in] */ REFIID             riid,
     /* [iid_is][out] */ LPUNKNOWN __RPC_FAR *ppUnk);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *GetErrorString)
    (IOPCServer __RPC_FAR *     This,
     /* [in] */ HRESULT         dwError,
     /* [in] */ LCID            dwLocale,
     /* [string][out] */ LPWSTR __RPC_FAR *ppString);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *GetGroupByName)
    (IOPCServer __RPC_FAR *        This,
     /* [string][in] */ LPCWSTR    szName,
     /* [in] */ REFIID             riid,
     /* [iid_is][out] */ LPUNKNOWN __RPC_FAR *ppUnk);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *GetStatus)
    (IOPCServer __RPC_FAR *      This,
     /* [out] */ OPCSERVERSTATUS __RPC_FAR *__RPC_FAR *ppServerStatus);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *RemoveGroup)
    (IOPCServer __RPC_FAR *This,
     /* [in] */ OPCHANDLE  hServerGroup,
     /* [in] */ BOOL       bForce);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *CreateGroupEnumerator)
    (IOPCServer __RPC_FAR *        This,
     /* [in] */ OPCENUMSCOPE       dwScope,
     /* [in] */ REFIID             riid,
     /* [iid_is][out] */ LPUNKNOWN __RPC_FAR *ppUnk);

    END_INTERFACE
} IOPCServerVtbl;

interface IOPCServer {
    CONST_VTBL struct IOPCServerVtbl __RPC_FAR *lpVtbl;
};

typedef struct IOPCBrowseServerAddressSpaceVtbl {
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE *QueryInterface)
    (IOPCBrowseServerAddressSpace *This,
     /* [in] */ REFIID             riid,
     /* [iid_is][out] */
     __RPC__deref_out void **ppvObject);

    ULONG(STDMETHODCALLTYPE *AddRef)(IOPCBrowseServerAddressSpace *This);

    ULONG(STDMETHODCALLTYPE *Release)(IOPCBrowseServerAddressSpace *This);

    HRESULT(STDMETHODCALLTYPE *QueryOrganization)
    (IOPCBrowseServerAddressSpace *This,
     /* [out] */ OPCNAMESPACETYPE *pNameSpaceType);

    HRESULT(STDMETHODCALLTYPE *ChangeBrowsePosition)
    (IOPCBrowseServerAddressSpace *This,
     /* [in] */ OPCBROWSEDIRECTION dwBrowseDirection,
     /* [string][in] */ LPCWSTR    szString);

    HRESULT(STDMETHODCALLTYPE *BrowseOPCItemIDs)
    (IOPCBrowseServerAddressSpace *This,
     /* [in] */ OPCBROWSETYPE      dwBrowseFilterType,
     /* [string][in] */ LPCWSTR    szFilterCriteria,
     /* [in] */ VARTYPE            vtDataTypeFilter,
     /* [in] */ DWORD              dwAccessRightsFilter,
     /* [out] */ LPENUMSTRING *    ppIEnumString);

    HRESULT(STDMETHODCALLTYPE *GetItemID)
    (IOPCBrowseServerAddressSpace *This,
     /* [in] */ LPWSTR             szItemDataID,
     /* [string][out] */ LPWSTR *  szItemID);

    HRESULT(STDMETHODCALLTYPE *BrowseAccessPaths)
    (IOPCBrowseServerAddressSpace *This,
     /* [string][in] */ LPCWSTR    szItemID,
     /* [out] */ LPENUMSTRING *    ppIEnumString);

    END_INTERFACE
} IOPCBrowseServerAddressSpaceVtbl;

interface IOPCBrowseServerAddressSpace {
    CONST_VTBL struct IOPCBrowseServerAddressSpaceVtbl *lpVtbl;
};

typedef struct IOPCGroupStateMgtVtbl {
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *QueryInterface)
    (IOPCGroupStateMgt __RPC_FAR *This,
     /* [in] */ REFIID            riid,
     /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);

    ULONG(STDMETHODCALLTYPE __RPC_FAR *AddRef)(IOPCGroupStateMgt __RPC_FAR *This);

    ULONG(STDMETHODCALLTYPE __RPC_FAR *Release)
    (IOPCGroupStateMgt __RPC_FAR *This);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *GetState)
    (IOPCGroupStateMgt __RPC_FAR *This,
     /* [out] */ DWORD __RPC_FAR *pUpdateRate,
     /* [out] */ BOOL __RPC_FAR *pActive,
     /* [string][out] */ LPWSTR __RPC_FAR *ppName,
     /* [out] */ LONG __RPC_FAR *pTimeBias,
     /* [out] */ FLOAT __RPC_FAR *pPercentDeadband,
     /* [out] */ DWORD __RPC_FAR *pLCID,
     /* [out] */ OPCHANDLE __RPC_FAR *phClientGroup,
     /* [out] */ OPCHANDLE __RPC_FAR *phServerGroup);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *SetState)
    (IOPCGroupStateMgt __RPC_FAR *This,
     /* [in][unique] */ DWORD __RPC_FAR *pRequestedUpdateRate,
     /* [out] */ DWORD __RPC_FAR *pRevisedUpdateRate,
     /* [in][unique] */ BOOL __RPC_FAR *pActive,
     /* [in][unique] */ LONG __RPC_FAR *pTimeBias,
     /* [in][unique] */ FLOAT __RPC_FAR *pPercentDeadband,
     /* [in][unique] */ DWORD __RPC_FAR *pLCID,
     /* [in][unique] */ OPCHANDLE __RPC_FAR *phClientGroup);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *SetName)
    (IOPCGroupStateMgt __RPC_FAR *This,
     /* [string][in] */ LPCWSTR   szName);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *CloneGroup)
    (IOPCGroupStateMgt __RPC_FAR * This,
     /* [string][in] */ LPCWSTR    szName,
     /* [in] */ REFIID             riid,
     /* [iid_is][out] */ LPUNKNOWN __RPC_FAR *ppUnk);

    END_INTERFACE
} IOPCGroupStateMgtVtbl;

interface IOPCGroupStateMgt {
    CONST_VTBL struct IOPCGroupStateMgtVtbl __RPC_FAR *lpVtbl;
};

typedef struct IOPCItemMgtVtbl {
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *QueryInterface)
    (IOPCItemMgt __RPC_FAR *  This,
     /* [in] */ REFIID        riid,
     /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);

    ULONG(STDMETHODCALLTYPE __RPC_FAR *AddRef)(IOPCItemMgt __RPC_FAR *This);

    ULONG(STDMETHODCALLTYPE __RPC_FAR *Release)(IOPCItemMgt __RPC_FAR *This);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *AddItems)
    (IOPCItemMgt __RPC_FAR *        This,
     /* [in] */ DWORD               dwCount,
     /* [size_is][in] */ OPCITEMDEF __RPC_FAR *pItemArray,
     /* [size_is][size_is][out] */
     OPCITEMRESULT __RPC_FAR *__RPC_FAR *ppAddResults,
     /* [size_is][size_is][out] */
     HRESULT __RPC_FAR *__RPC_FAR *ppErrors);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *ValidateItems)
    (IOPCItemMgt __RPC_FAR *        This,
     /* [in] */ DWORD               dwCount,
     /* [size_is][in] */ OPCITEMDEF __RPC_FAR *pItemArray,
     /* [in] */ BOOL                           bBlobUpdate,
     /* [size_is][size_is][out] */
     OPCITEMRESULT __RPC_FAR *__RPC_FAR *  ppValidationResults,
     /* [size_is][size_is][out] */ HRESULT __RPC_FAR *__RPC_FAR *ppErrors);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *RemoveItems)
    (IOPCItemMgt __RPC_FAR *       This,
     /* [in] */ DWORD              dwCount,
     /* [size_is][in] */ OPCHANDLE __RPC_FAR *phServer,
     /* [size_is][size_is][out] */ HRESULT __RPC_FAR *__RPC_FAR *ppErrors);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *SetActiveState)
    (IOPCItemMgt __RPC_FAR *       This,
     /* [in] */ DWORD              dwCount,
     /* [size_is][in] */ OPCHANDLE __RPC_FAR *phServer,
     /* [in] */ BOOL                          bActive,
     /* [size_is][size_is][out] */ HRESULT __RPC_FAR *__RPC_FAR *ppErrors);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *SetClientHandles)
    (IOPCItemMgt __RPC_FAR *       This,
     /* [in] */ DWORD              dwCount,
     /* [size_is][in] */ OPCHANDLE __RPC_FAR *phServer,
     /* [size_is][in] */ OPCHANDLE __RPC_FAR *phClient,
     /* [size_is][size_is][out] */ HRESULT __RPC_FAR *__RPC_FAR *ppErrors);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *SetDatatypes)
    (IOPCItemMgt __RPC_FAR *       This,
     /* [in] */ DWORD              dwCount,
     /* [size_is][in] */ OPCHANDLE __RPC_FAR *phServer,
     /* [size_is][in] */ VARTYPE __RPC_FAR *pRequestedDatatypes,
     /* [size_is][size_is][out] */ HRESULT __RPC_FAR *__RPC_FAR *ppErrors);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *CreateEnumerator)
    (IOPCItemMgt __RPC_FAR *       This,
     /* [in] */ REFIID             riid,
     /* [iid_is][out] */ LPUNKNOWN __RPC_FAR *ppUnk);

    END_INTERFACE
} IOPCItemMgtVtbl;

interface IOPCItemMgt {
    CONST_VTBL struct IOPCItemMgtVtbl __RPC_FAR *lpVtbl;
};

typedef struct IOPCDataCallbackVtbl {
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *QueryInterface)
    (IOPCDataCallback __RPC_FAR *This,
     /* [in] */ REFIID           riid,
     /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);

    ULONG(STDMETHODCALLTYPE __RPC_FAR *AddRef)(IOPCDataCallback __RPC_FAR *This);

    ULONG(STDMETHODCALLTYPE __RPC_FAR *Release)(IOPCDataCallback __RPC_FAR *This);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *OnDataChange)
    (IOPCDataCallback __RPC_FAR *  This,
     /* [in] */ DWORD              dwTransid,
     /* [in] */ OPCHANDLE          hGroup,
     /* [in] */ HRESULT            hrMasterquality,
     /* [in] */ HRESULT            hrMastererror,
     /* [in] */ DWORD              dwCount,
     /* [size_is][in] */ OPCHANDLE __RPC_FAR *phClientItems,
     /* [size_is][in] */ VARIANT __RPC_FAR *pvValues,
     /* [size_is][in] */ WORD __RPC_FAR *pwQualities,
     /* [size_is][in] */ FILETIME __RPC_FAR *pftTimeStamps,
     /* [size_is][in] */ HRESULT __RPC_FAR *pErrors);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *OnReadComplete)
    (IOPCDataCallback __RPC_FAR *  This,
     /* [in] */ DWORD              dwTransid,
     /* [in] */ OPCHANDLE          hGroup,
     /* [in] */ HRESULT            hrMasterquality,
     /* [in] */ HRESULT            hrMastererror,
     /* [in] */ DWORD              dwCount,
     /* [size_is][in] */ OPCHANDLE __RPC_FAR *phClientItems,
     /* [size_is][in] */ VARIANT __RPC_FAR *pvValues,
     /* [size_is][in] */ WORD __RPC_FAR *pwQualities,
     /* [size_is][in] */ FILETIME __RPC_FAR *pftTimeStamps,
     /* [size_is][in] */ HRESULT __RPC_FAR *pErrors);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *OnWriteComplete)
    (IOPCDataCallback __RPC_FAR *  This,
     /* [in] */ DWORD              dwTransid,
     /* [in] */ OPCHANDLE          hGroup,
     /* [in] */ HRESULT            hrMastererr,
     /* [in] */ DWORD              dwCount,
     /* [size_is][in] */ OPCHANDLE __RPC_FAR *pClienthandles,
     /* [size_is][in] */ HRESULT __RPC_FAR *pErrors);

    HRESULT(STDMETHODCALLTYPE __RPC_FAR *OnCancelComplete)
    (IOPCDataCallback __RPC_FAR *This,
     /* [in] */ DWORD            dwTransid,
     /* [in] */ OPCHANDLE        hGroup);

    END_INTERFACE
} IOPCDataCallbackVtbl;

interface IOPCDataCallback {
    CONST_VTBL struct IOPCDataCallbackVtbl __RPC_FAR *lpVtbl;
};

typedef struct IOPCShutdownVtbl {
    BEGIN_INTERFACE

    HRESULT(STDMETHODCALLTYPE *QueryInterface)
    (IOPCShutdown *    This,
     /* [in] */ REFIID riid,
     /* [iid_is][out] */
     __RPC__deref_out void **ppvObject);

    ULONG(STDMETHODCALLTYPE *AddRef)(IOPCShutdown *This);

    ULONG(STDMETHODCALLTYPE *Release)(IOPCShutdown *This);

    HRESULT(STDMETHODCALLTYPE *ShutdownRequest)
    (IOPCShutdown *             This,
     /* [string][in] */ LPCWSTR szReason);

    END_INTERFACE
} IOPCShutdownVtbl;

interface IOPCShutdown {
    CONST_VTBL struct IOPCShutdownVtbl *lpVtbl;
};

#endif /* OPCDA2_C_H */
