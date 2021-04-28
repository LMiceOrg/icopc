/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include <crtdbg.h>
#include <sysinfoapi.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "opcda2_all.h"

#include "server.h"
#include "util_trace.h"

#if 0
void test_websocet();



static void print_u16(const unsigned char *p) {
  int i;
  for (i = 0; i < 16; ++i) {
    printf("0x%02x ", *(p + i));
    if (i == 7)
      printf("\n");
  }
  printf("\n");
}

#include "../libs/sms/sm4.h"
void test_websocket() {
    {
    /* test sm4 */
    const unsigned char user_key[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe,
                         0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};

    const unsigned char plaintext[16] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
    };

    const unsigned char ciphertext1[16] = {
        0x68, 0x1e, 0xdf, 0x34, 0xd2, 0x06, 0x96, 0x5e,
        0x86, 0xb3, 0xe9, 0x4f, 0x53, 0x6e, 0x42, 0x46,
    };

    unsigned char buf[16];
    unsigned char buf2[16];
    int out_size;

    const char p[] = "wel to CN!";
    const char k[] = "13010388100";

    sm4_encrypt(plaintext, buf, 16, user_key, 16);

    if (memcmp(ciphertext1, buf, 16) == 0) {
      printf("sms4 encrypt pass!\n");
    } else {
      printf("sms4 encrypt not pass!\n");
    }

    sm4_decrypt(buf, buf2, 16, user_key, 16);
    if (memcmp(plaintext, buf2, 16) == 0) {
      printf("sms4 decrypt pass!\n");
    } else {
      printf("sms4 decrypt not pass!\n");
      printf("ori:\n");
      print_u16(plaintext);
      printf("rst:\n");
      print_u16(buf);
    }

    memset(buf, 0, 16);
    memset(buf2, 0, 16);
    out_size = sm4_size(strlen(p));
    printf("p %s %d  k %s %d, outsize:%d\n", p, strlen(p), k, strlen(k),
           out_size);
    sm4_encrypt(p, buf, strlen(p), k, strlen(k));
    printf("rst:\n");
    print_u16(buf);

    sm4_decrypt(buf, buf2, 16, k, strlen(k));
    printf("rst:\n");
    print_u16(buf2);
    printf("%s\n", (const char *)buf2);

  }

}

#endif

#if 0
/* 列出本机 OPC Server */
static void list_local_server();

static void list_group();
void list_local_server() {
  HRESULT hr;

  IOPCServerList *server_list = NULL;
  IClassFactory *classFactory = NULL;
  IEnumGUID *ienum_guid = NULL;
  IID cat_id = IID_CATID_OPCDAServer20;

  hr = CoGetClassObject(&CLSID_IOPCServerList, CLSCTX_SERVER, 0,
                        &IID_IClassFactory, (void **)&classFactory);
  if (FAILED(hr)) {
    trace_debug("can not find IClassFactory %ld\n", hr);
    goto clean_up;
  }
    trace_debug("find IID_IClassFactory\n");

  hr = classFactory->lpVtbl->CreateInstance(
      classFactory, 0, &IID_IOPCServerList, (void **)&server_list);
  if (FAILED(hr)) {
    trace_debug("can not find CLSID_IOPCServerList\n");
    goto clean_up;
  }

  hr = server_list->lpVtbl->EnumClassesOfCategories(server_list, 1, &cat_id, 1,
                                                    &cat_id, &ienum_guid);

  if (FAILED(hr)) {
    trace_debug("can not run EnumClassesOfCategories\n");
    goto clean_up;
  }

  GUID clsid;
  ULONG c;
  while ((hr = ienum_guid->lpVtbl->Next(ienum_guid, 1, &clsid, &c)) == S_OK) {
    LPOLESTR prog_id;
    LPOLESTR user_type;
    hr = server_list->lpVtbl->GetClassDetails(server_list, &clsid, &prog_id,
                                              &user_type);
    if (hr == S_OK) {
      wtrace_debug(L"OPCServer prog_id: %ls \tuser_type: %ls\n", prog_id, user_type);
    }
  }

  trace_debug("done\n");
clean_up:

  if (ienum_guid) {
    ienum_guid->lpVtbl->Release(ienum_guid);
  }

  if (server_list) {
    server_list->lpVtbl->Release(server_list);
  }

  if (classFactory) {
    classFactory->lpVtbl->Release(classFactory);
  }
}

#define container_of(ptr, type, member)                      \
    ({                                                       \
        const typeof(((type *) 0)->member) *__mptr = (ptr);  \
        (type *) ((char *) __mptr - offsetof(type, member)); \
    })

void list_group(const wchar_t *host, const wchar_t *prog_id) {
  HRESULT hr;
  COSERVERINFO svr_info;
  CLSID cls_id;
  MULTI_QI qi_list[1];
  OPCSERVERSTATUS *svr_status;

  IClassFactory *svr_fac = NULL;
  IOPCServer *svr = NULL;
  IUnknown *un = NULL;
  IOPCGroupStateMgt *grp_mgt = NULL;

  /* 1. 初始化 */
  memset(&svr_info, 0, sizeof(svr_info));
  svr_info.pwszName = (wchar_t *)host;

  memset(qi_list, 0, sizeof(qi_list));
  qi_list[0].pIID = &IID_IOPCServer;

  CLSIDFromProgID(prog_id, &cls_id);
  OLECHAR *str_id;
  StringFromCLSID(&cls_id, &str_id);
  wtrace_debug(L"cls id:%ls\n", str_id);

/* 创建Server对象 */
#if 1
  hr = CoCreateInstanceEx(&cls_id, 0, CLSCTX_LOCAL_SERVER|CLSCTX_INPROC_SERVER|CLSCTX_REMOTE_SERVER,
  NULL,  // &svr_info,
  sizeof(qi_list)/sizeof(MULTI_QI), qi_list);
  if (FAILED(hr)) {
    goto clean_up;
  }

  svr = (IOPCServer*) qi_list[0].pItf;
#else
  hr = CoGetClassObject(&cls_id, CLSCTX_SERVER, 0, &IID_IClassFactory,
                        (void **)&svr_fac);
  if (FAILED(hr)) {
    trace_debug("can not find IClassFactory %ld\n", hr);
    goto clean_up;
  }

  hr = svr_fac->lpVtbl->CreateInstance(svr_fac, 0, &IID_IOPCServer,
                                       (void **)&svr);
  if (FAILED(hr)) {
    goto clean_up;
  }
  trace_debug("get svr opcserver\n");

#endif

  hr = svr->lpVtbl->GetStatus(svr, &svr_status);
  if (FAILED(hr)) {
    goto clean_up;
  }
  trace_debug("server has status %d\n", (int)svr_status->dwServerState);
  trace_debug("there are %d groups\n", (int)svr_status->dwGroupCount);

  LONG time_bias = 5000;
  OPCHANDLE cli_grp = 101;
  OPCHANDLE svr_grp;
  FLOAT dead_band = 0.0;
  DWORD lang_id = 0x407;
  DWORD upd_rate;

  hr = svr->lpVtbl->AddGroup(svr, L"grouptest1", TRUE, 2000, cli_grp,
                             &time_bias, &dead_band, lang_id, &svr_grp,
                             &upd_rate, &IID_IUnknown, &un);
  if (FAILED(hr)) {
    goto clean_up;
  }

  hr = svr->lpVtbl->GetStatus(svr, &svr_status);
  if (FAILED(hr)) {
    goto clean_up;
  }
  trace_debug("server has status %d\n", (int)svr_status->dwServerState);
  trace_debug("there are %d groups\n", (int)svr_status->dwGroupCount);

  hr =
      un->lpVtbl->QueryInterface(un, &IID_IOPCGroupStateMgt, (void **)&grp_mgt);
  if (FAILED(hr)) {
    goto clean_up;
  }

  grp_mgt->lpVtbl->SetName(grp_mgt, L"grouptest1_changename");

  trace_debug("list group finish\n");

clean_up:

  if (grp_mgt) {
    grp_mgt->lpVtbl->Release(grp_mgt);
  }

  if (un) {
    un->lpVtbl->Release(un);
  }

  if (svr) {
    svr->lpVtbl->Release(svr);
  }

  if (svr_fac) {
    svr_fac->lpVtbl->Release(svr_fac);
  }
}

void test(int size, const wchar_t *items) {
    for (int i = 0; i < size; ++i) {
        const wchar_t *id = items + i*32;
        wtrace_debug(L"id: %ls\n", id);
    }
}
#endif

int main_test() {
    int     item_size = 6;
    int     item_active[6];
    wchar_t item_id[6][OPCDA2_ITEM_ID_LEN];
    wchar_t item_write_id[6][OPCDA2_ITEM_ID_LEN];
    VARIANT item_write_value[6];
    int     surfix[6] = {1, 2, 4, 1, 2, 4};
    int     i;

    memset(item_id, item_size, 6 * OPCDA2_ITEM_ID_SIZE);

    for (i = 0; i < item_size; ++i) {
        wchar_t *id = item_id[i];

        item_active[i] = 1;

        memset(item_write_value + i, 0, sizeof(VARIANT));
        item_write_value[i].vt     = VT_I4;
        item_write_value[i].intVal = i * 3;
        swprintf(item_write_id[i], OPCDA2_ITEM_ID_LEN, L"Write Only.Int%d", surfix[i]);

        if (i < 3) {
            swprintf(id, OPCDA2_ITEM_ID_LEN, L"Random.Int%d", surfix[i]);
        } else {
            swprintf(id, OPCDA2_ITEM_ID_LEN, L"Random.UInt%d", surfix[i]);
        }
    }

    trace_debug("server conn size%d\n", sizeof(connect_list) );

    _CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF);

#if 0
  HEAP_SUMMARY summary;
  memset(&summary, 0, sizeof(summary));
  summary.cb = sizeof(HEAP_SUMMARY);
  HANDLE heap = GetProcessHeap();

  printf("call heap summary\n");

  HeapSummary(heap, 0, &summary);
  trace_debug("heap: alloc:%lu  commit: %lu reserv: %lu\n", summary.cbAllocated, summary.cbCommitted, summary.cbReserved);
#endif

    {
        server_info *info_list;
        int          size;
        opcda2_serverlist_fetch(L"127.0.0.1", &size, &info_list);
        printf("info_list size %d\n", size);
        if (info_list) {
            for (i = 0; i < size; ++i) {
                server_info *info = info_list + i;
                wprintf(L"info name:%ls, user_type:%ls\n", info->prog_id, info->user_type);
            }
        }
        opcda2_serverlist_clear(size, info_list);
    }

    {
        data_list *     items;
        server_connect *conn;
        group *         grp = NULL;

        items = CoTaskMemAlloc(sizeof(data_list));
        memset(items, 0, sizeof(data_list));
        opcda2_server_connect(L"127.0.0.1", L"Matrikon.OPC.Simulation.1", items, &conn);
        for (i = 0; i < conn->item_size; ++i) {
            wtrace_debug(L"item id:%ls\n", conn->item_list[i].id);
        }

        /* test add group */

        opcda2_group_add(conn, L"group001", 1, 2000, &grp);
        opcda2_item_add(grp, 3, (const wchar_t *) item_id, item_active);

#if 0
        opcda2_group_add(conn, L"group002", 1, 4000, &grp);
        opcda2_item_add(grp, item_size, (const wchar_t *) item_id, item_active);
        opcda2_group_add(conn, L"group003", 1, 3000, &grp);
        opcda2_item_add(grp, item_size, (const wchar_t *) item_id, item_active);
        opcda2_group_add(conn, L"group004", 1, 2000, &grp);
        opcda2_item_add(grp, item_size, (const wchar_t *) item_id, item_active);
#endif

        Sleep(10000);

        trace_debug("test write data\n");
        opcda2_item_add(grp, 3, (const wchar_t *) item_write_id, item_active);
        opcda2_item_add(grp, 3, (const wchar_t *) item_id[3], item_active);
        Sleep(1000);
        opcda2_item_write(grp, 3, (const wchar_t *) item_write_id, item_write_value);
        Sleep(2000);
        opcda2_item_refresh(grp);
        Sleep(5000);

        opcda2_server_disconnect(conn);
        CoTaskMemFree(conn);
        CoTaskMemFree(items);
    }

    _CrtDumpMemoryLeaks();

#if 0
    // Use to convert bytes to KB
    // #define DIV 1024
    // #define WIDTH 7

    //   MEMORYSTATUSEX statex;
    //   statex.dwLength = sizeof(statex);
    //   GlobalMemoryStatusEx(&statex);

    //   wtrace_debug(L"There is  %*ld percent of memory in use.\n", WIDTH, statex.dwMemoryLoad);
    //   wtrace_debug(L"There are %*I64d total KB of physical memory.\n", WIDTH, statex.ullTotalPhys / DIV);
    //   wtrace_debug(L"There are %*I64d free  KB of physical memory.\n", WIDTH, statex.ullAvailPhys / DIV);
    //   wtrace_debug(L"There are %*I64d total KB of paging file.\n", WIDTH, statex.ullTotalPageFile / DIV);
    //   wtrace_debug(L"There are %*I64d free  KB of paging file.\n", WIDTH, statex.ullAvailPageFile / DIV);
    //   wtrace_debug(L"There are %*I64d total KB of virtual memory.\n", WIDTH, statex.ullTotalVirtual / DIV);
    //   wtrace_debug(L"There are %*I64d free  KB of virtual memory.\n", WIDTH, statex.ullAvailVirtual / DIV);
    //   wtrace_debug(L"There are %*I64d free  KB of extended memory.\n", WIDTH, statex.ullAvailExtendedVirtual / DIV);
#endif

#if 0
printf("call heap summary\n");

  HeapSummary(heap, 0, &summary);
  trace_debug("heap: alloc:%lu  commit: %lu reserv: %lu\n", summary.cbAllocated, summary.cbCommitted, summary.cbReserved);
#endif

#if 0
    //   char *p = (char *) CoTaskMemAlloc(1024);

    //   HeapSummary(heap, 0, &summary);
    //   trace_debug("heap: alloc:%lu  commit: %lu reserv: %lu\n", summary.cbAllocated,
    //   summary.cbCommitted,summary.cbReserved);
#endif
    return 0;
}

#include <tlhelp32.h>

typedef struct abc_item {
  ICLIST_DATAHEAD
  int private;
}abc_item;

iclist_def_prototype(abc, abc_item, 15, 3);
iclist_def_function(abc, abc_item)

int main() {
    DWORD c_tid = GetCurrentThreadId();
    /* 初始化COM */
    CoInitializeEx(0, COINIT_MULTITHREADED);
    trace_debug("main thread %ld\n", c_tid);
#if 0

    {
      connect_list *ptest;
      iclist_alloc(ptest, connect_list);
      iclist_lock(ptest);
    trace_debug("my connect_list size %d  server_conn size %d  group size %d\n", sizeof(connect_list), sizeof(server_connect), sizeof(group));
    trace_debug("data_list size %d %x\n", sizeof(data_list), ptest->lock);
    iclist_unlock(ptest);
    trace_debug("data_list size %d %x\n", sizeof(data_list), ptest->lock);
    iclist_free(ptest);
    trace_debug("hash %u\n", iclist_abc_hash(&c_tid, 4, 0) );

    return 0;
    }
    #endif

    for (;;) {
        int c;
        while (1) {
            trace_debug("for loop:press r to run, e to exit\n");
            c = getchar();
            if (c == 'r' || c == 'e') break;
        }
        if (c == 'e') break;

        main_test();
#if 0
        // trace_debug("main_test done\n");
        // DWORD         c_pid     = GetCurrentProcessId();
        // HANDLE        snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        // THREADENTRY32 trd_entry;
        // trd_entry.dwSize = sizeof(THREADENTRY32);
        // if (snap_shot == INVALID_HANDLE_VALUE) {
        //     trace_debug("CreateToolhelp32Snapshot failed\n");
        // } else {
        //     trace_debug("CreateToolhelp32Snapshot success\n");
        //     if (!Thread32First(snap_shot, &trd_entry)) {
        //         trace_debug("Thread32First failed\n");
        //         CloseHandle(snap_shot);
        //     } else {
        //         do {
        //             if (trd_entry.th32OwnerProcessID == c_pid) {
        //                 trace_debug("\n     THREAD ID      = 0x%08X", trd_entry.th32ThreadID);
        //                 if (trd_entry.th32ThreadID == c_tid) {
        //                     trace_debug(" [main]");
        //                 } else {
        //                     // HANDLE trd = OpenThread(THREAD_ALL_ACCESS, FALSE, trd_entry.th32ThreadID);
        //                     // TerminateThread(trd, 0);
        //                     // CloseHandle(trd);
        //                 }
        //                 trace_debug("\n     base priority  = %d", trd_entry.tpBasePri);
        //                 trace_debug("\n     delta priority = %d", trd_entry.tpDeltaPri);
        //             }
        //         } while (Thread32Next(snap_shot, &trd_entry));
        //         trace_debug("\n");
        //         CloseHandle(snap_shot);
        //     }
        // }
#endif
    }
    CoUninitialize();

    while (1) {
        int c = getchar();
        trace_debug("press e to quit\n");

        if (c == 'e') break;
    }

    return 0;
}

