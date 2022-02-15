/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#include <crtdbg.h>
#include <sysinfoapi.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "opcda2_all.h"

#include "server.h"
#include "util_trace.h"

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
        item_write_value[i].intVal = i * 6;
        swprintf(item_write_id[i], OPCDA2_ITEM_ID_LEN, L"Write Only.Int%d", surfix[i]);

        if (i < 3) {
            swprintf(id, OPCDA2_ITEM_ID_LEN, L"Random.Int%d", surfix[i]);
        } else {
            swprintf(id, OPCDA2_ITEM_ID_LEN, L"Random.UInt%d", surfix[i]);
        }
    }

    trace_debug("server conn struct size:%d\n", sizeof(connect_list));
#if 0
    _CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF);
#endif

#if 0
  HEAP_SUMMARY summary;
  memset(&summary, 0, sizeof(summary));
  summary.cb = sizeof(HEAP_SUMMARY);
  HANDLE heap = GetProcessHeap();

  printf("call heap summary\n");

  HeapSummary(heap, 0, &summary);
  trace_debug("heap: alloc:%lu  commit: %lu reserv: %lu\n", summary.cbAllocated, summary.cbCommitted, summary.cbReserved);
#endif
#if 0
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
#endif

    {
        data_list *     items;
        connect_list *  clist = NULL;
        server_connect *conn  = NULL;
        group *         grp   = NULL;

        items = iclist_data_list_alloc(NULL);
        clist = iclist_connect_list_alloc(opcda2_connect_del);
#if 1
        opcda2_connect_add(clist, L"127.0.0.1", L"Matrikon.OPC.Simulation.1", items, &conn);

        for (i = 0; i < conn->item_size; ++i) {
            wtrace_debug(L"item id:%ls\n", conn->item_list[i].id);
        }
        trace_debug("connect size:%d\n", iclist_size(clist));
        trace_debug("conn using:%d\n", conn->used);
#endif
#if 1
        /* test add group */
        trace_debug("add group %p\n", (void *) conn);
        opcda2_group_add(conn, L"group001", 1, 2000, &grp);
        trace_debug("add item %p\n", (void *) grp);
        opcda2_item_add(grp, 3, (const wchar_t *) item_id, item_active);
#endif

#if 1
        opcda2_group_add(conn, L"group002", 1, 4000, &grp);
        opcda2_item_add(grp, item_size, (const wchar_t *) item_id, item_active);
        opcda2_group_add(conn, L"group003", 1, 3000, &grp);
        opcda2_item_add(grp, item_size, (const wchar_t *) item_id, item_active);
        opcda2_group_add(conn, L"group004", 1, 2000, &grp);
        opcda2_item_add(grp, item_size, (const wchar_t *) item_id, item_active);
#endif

        Sleep(10000);
#if 1
        /* test write data */
        trace_debug("test write data conn used:%d itm_mgt:%p\n", conn->used, (void *) grp->itm_mgt);
        opcda2_item_add(grp, 3, (const wchar_t *) item_write_id, item_active);
        opcda2_item_add(grp, 3, (const wchar_t *) item_id[3], item_active);
        Sleep(1000);
        opcda2_item_write(grp, 3, (const wchar_t *) item_write_id, item_write_value);
        Sleep(2000);
        opcda2_item_refresh(grp);
        Sleep(5000);
#endif

#if 1
        /* 清除列表所有元素 */
        /* iclist_connect_list_clear(clist); */

        /* 释放列表 */
        iclist_connect_list_free(clist);
        iclist_data_list_free(items);
#endif
    }

#if 0
    _CrtDumpMemoryLeaks();
#endif

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
    int pid;
} abc_item;

iclist_def_prototype(abc, abc_item, 15, 3);
iclist_def_function(abc, abc_item);

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
    trace_debug("my connect_list size %d  server_conn size %d  group size %d\n",
    sizeof(connect_list), sizeof(server_connect), sizeof(group));
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
