#include "stdafx.h"

#include <Shlwapi.h>
#include <Windows.h>
#include <shellapi.h>

#include <locale.h>
#include <signal.h>
#include <strsafe.h>
#include <tchar.h>

#include "icopc_rpc.h"
#include "win_message.h"
#include "win_server.h"
#include "win_trace.h"

#include "../websocket/wsserver.h"

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

SERVICE_STATUS gSvcStatus;
SERVICE_STATUS_HANDLE gSvcStatusHandle = NULL;
HANDLE ghSvcStopEvent[2];
HANDLE gEventSource=NULL;

VOID SvcInstall(void);
VOID SvcUninstall();
VOID WINAPI SvcCtrlHandler(DWORD);
VOID WINAPI SvcMain(DWORD, LPTSTR *);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR *);

VOID SvcReportInfo(LPCTSTR szFunc, LPCTSTR szStr);
VOID SvcReportError(LPCTSTR szFunc, LPCTSTR szStr);
void SignalHandler(int /*signal*/);

iocp_server *g_server = NULL;
DWORD WINAPI WorkThread(LPVOID /*lpHandle*/);
void StopWorkThread();

/* 默认 以服务启动 */
int g_app_mode = 0; 

DWORD WINAPI WorkThread(LPVOID /*lpHandle*/) {
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
  rpc_session ses;

  ses = create_rpc_session();

  g_server = iocp_server_start(5000);

  wsserver_start(g_server, get_accept_callback(&ses), 0);

  iocp_server_join(g_server);

  // iocp_server_stop(g_server);

  close_rpc_session(&ses);

  ::CoUninitialize();

  g_server = NULL;

  return 0;
}

void StopWorkThread() {
  if (g_server) {
    iocp_server_stop(g_server);
    g_server = NULL;
  }
}

int _tmain(int argc, TCHAR *argv[]) {
  /* 以命令行启动 */
  g_app_mode = 1;
  setlocale(LC_ALL, ".OCP");

  /* 注册服务  */
  if (lstrcmpi(argv[1], TEXT("install")) == 0) {
    SvcInstall();
    return 0;
  }
  /* 命令行运行 */
  else if (lstrcmpi(argv[1], TEXT("start")) == 0) {

    signal(SIGINT, SignalHandler);

    SvcInit(argc - 1, argv);

    return 0;
  }
  /* 注销服务 */
  else if (lstrcmpi(argv[1], TEXT("uninstall")) == 0) {
    SvcUninstall();
    return 0;
  }

  /* 启动服务 */
  g_app_mode = 0;

  SERVICE_TABLE_ENTRY DispatchTable[] = {
      {SZSERVICENAME, (LPSERVICE_MAIN_FUNCTION)SvcMain}, {NULL, NULL}};
  if (!StartServiceCtrlDispatcher(DispatchTable)) {
    SvcReportError(TEXT("ICOPC"), TEXT("StartServiceCtrlDispatcher failed"));
  }
  return 0;
}

/* 注册服务 */
VOID SvcInstall() {
  SC_HANDLE schSCManager;
  SC_HANDLE schService;
  TCHAR szPath[MAX_PATH];

  if (!GetModuleFileName(NULL, szPath, MAX_PATH)) {
    SvcReportError(TEXT("SvcInstall"), TEXT("Cannot install service"));
    return;
  }

  // Get a handle to the SCM database.

  schSCManager = OpenSCManager(NULL, // local computer
                               NULL, // ServicesActive database
                               SC_MANAGER_ALL_ACCESS); // full access rights

  if (NULL == schSCManager) {
    SvcReportError(TEXT("SvcInstall"), TEXT("OpenSCManager failed"));
    return;
  }

  // Create the service

  schService = CreateService(schSCManager,         // SCM database
                             SZSERVICENAME,        // name of service
                             SZSERVICEDISPLAYNAME, // service name to display
                             SERVICE_ALL_ACCESS,   // desired access
                             SERVICE_WIN32_OWN_PROCESS, // service type
                             SERVICE_DEMAND_START,      // start type
                             SERVICE_ERROR_NORMAL,      // error control type
                             szPath, // path to service's binary
                             NULL,   // no load ordering group
                             NULL,   // no tag identifier
                             NULL,   // no dependencies
                             NULL,   // LocalSystem account
                             NULL);  // no password

  if (schService == NULL) {
    SvcReportError(TEXT("SvcInstall"), TEXT("CreateService failed"));
    CloseServiceHandle(schSCManager);
    return;
  } else {
    SvcReportInfo(TEXT("SvcInstall"), TEXT("Service installed successfully"));

    HKEY scAppKey;
    LONG lret;
    lret = RegCreateKeyEx(
        HKEY_LOCAL_MACHINE,
        SZREGKEY,
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &scAppKey,
        NULL);
    if (lret != ERROR_SUCCESS) {
      SvcReportError(TEXT("SvcInstall"), TEXT("Register event failed"));
    } else {
      RegSetValueEx(scAppKey, _T("EventMessageFile"), 0, REG_SZ, (const BYTE*)szPath,
                    sizeof(TCHAR) * _tcslen(szPath));

      DWORD type = 0x000000007;
      RegSetValueEx(scAppKey, _T("TypesSupported"), 0, REG_DWORD, (const BYTE*)&type,
                    sizeof(type));
      RegCloseKey(scAppKey);
    }
  }

  CloseServiceHandle(schService);
  CloseServiceHandle(schSCManager);
}

/* 注销服务 */
VOID SvcUninstall() {
  SC_HANDLE schSCManager;
  SC_HANDLE schService;
  schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (NULL == schSCManager) {
    SvcReportError(TEXT("SvcUninstall"), TEXT("OpenSCManager failed"));
    return;
  } else {
    schService = OpenService(schSCManager, SZSERVICENAME, DELETE);
    if (schService == NULL) {
      SvcReportError(TEXT("SvcUninstall"), TEXT("OpenService failed"));
      return;
    }
    if (!DeleteService(schService)) {
      SvcReportError(TEXT("SvcUninstall"), TEXT("Service Delete error"));
    } else {
      SvcReportInfo(TEXT("SvcUninstall"), TEXT("Delete Service Successfully"));
    }
    CloseServiceHandle(schService);
  }

  RegDeleteKey(HKEY_LOCAL_MACHINE,
      SZREGKEY);
}

/* 信号处理 */
void SignalHandler(int /*signal*/) {
  lmice_info_print("Application aborting (Ctrl+C)...\n");
  StopWorkThread();
}

/* 服务运行 */
VOID SvcInit(DWORD /*dwArgc*/, LPTSTR *) // lpszArgv)
{

  /* 注册 event */
  gEventSource = RegisterEventSource(NULL, SZSERVICENAME);

  SvcReportInfo(TEXT("SvcInit"), TEXT("启动ICOPC服务") );
  // Create an event. The control handler function, SvcCtrlHandler,
  // signals this event when it receives the stop control code.
  ghSvcStopEvent[0] = CreateEvent(NULL,  // default security attributes
                                  TRUE,  // manual reset event
                                  FALSE, // not signaled
                                  NULL); // no name

  if (ghSvcStopEvent[0] == NULL) {
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
    return;
  }

  //SvcReportInfo(TEXT("SvcInit"), TEXT("icopc server starting ..2"));

  // TO_DO: Perform work until service stops.
  DWORD threadId;
  ghSvcStopEvent[1] = CreateThread(NULL, 0, WorkThread,
                                   (LPVOID)&ghSvcStopEvent[0], 0, &threadId);
  if (ghSvcStopEvent[1] == NULL) {
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
    CloseHandle(ghSvcStopEvent[0]);
    return;
  }

  // Report running status when initialization is complete.

  ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

  while (1) {
    // Check whether to stop the service.

    DWORD id = WaitForMultipleObjects(2, ghSvcStopEvent, FALSE, INFINITE);
    switch (id) {
    case WAIT_TIMEOUT:
      StopWorkThread();
      break;
    case WAIT_OBJECT_0:
      SvcReportInfo(TEXT("SvcInit"), TEXT("ICOPC Server WAIT_OBJECT_0"));
      StopWorkThread();
      CloseHandle(ghSvcStopEvent[0]);
      CloseHandle(ghSvcStopEvent[1]);
      break;
    case WAIT_OBJECT_0 + 1:
      SvcReportInfo(TEXT("SvcInit"), TEXT("ICOPC Server WAIT_OBJECT_1"));
      CloseHandle(ghSvcStopEvent[0]);
      CloseHandle(ghSvcStopEvent[1]);
      break;
    default:
      StopWorkThread();
      ExitProcess(0);
      break;
    }
    SvcReportInfo(TEXT("SvcInit"), TEXT("停止ICOPC服务"));
    DeregisterEventSource(gEventSource);
    gEventSource=NULL;
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
    
    return;
  }
}

/* 服务入口 */
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR *lpszArgv) {
  // Register the handler function for the service
  gSvcStatusHandle = RegisterServiceCtrlHandler(SZSERVICENAME, SvcCtrlHandler);

  if (!gSvcStatusHandle) {
    SvcReportError(TEXT("SvcMain"), TEXT("RegisterServiceCtrlHandler"));
    return;
  }

  // These SERVICE_STATUS members remain as set here
  gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  gSvcStatus.dwServiceSpecificExitCode = 0;

  // Report initial status to the SCM
  ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

  // Perform service-specific initialization and work.
  SvcInit(dwArgc, lpszArgv);
}

/* 更新服务状态 */
VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
                     DWORD dwWaitHint) {
  static DWORD dwCheckPoint = 1;

  // Fill in the SERVICE_STATUS structure.

  gSvcStatus.dwCurrentState = dwCurrentState;
  gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
  gSvcStatus.dwWaitHint = dwWaitHint;

  if (dwCurrentState == SERVICE_START_PENDING)
    gSvcStatus.dwControlsAccepted = 0;
  else
    gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

  if ((dwCurrentState == SERVICE_RUNNING) ||
      (dwCurrentState == SERVICE_STOPPED))
    gSvcStatus.dwCheckPoint = 0;
  else
    gSvcStatus.dwCheckPoint = dwCheckPoint++;

  // Report the status of the service to the SCM.
  if(gSvcStatusHandle) {
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
  }
}

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl) {
  // Handle the requested control code.

  switch (dwCtrl) {
  case SERVICE_CONTROL_STOP:
    ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

    // Signal the service to stop.
    SetEvent(ghSvcStopEvent[0]);

    return;

  case SERVICE_CONTROL_INTERROGATE:
    // Fall through to send current status.
    break;

  default:
    break;
  }

  ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
}

VOID SvcReportInfo(LPCTSTR szFunc, LPCTSTR szStr) {
    LPCTSTR lpszStrings[2];

    if(g_app_mode == 1) {
      lmice_info_wprint(L"%s:%s\n", szFunc, szStr);
      return;
    }

    if (NULL != gEventSource) {
        lpszStrings[0] = szFunc;
        lpszStrings[1] = szStr;

        ReportEvent(gEventSource,        // event log handle
                EVENTLOG_INFORMATION_TYPE, // event type
                0,                   // event category
                SVC_RUNTIME_INFO,   // event identifier
                NULL,                // no security identifier
                2,                   // size of lpszStrings array
                0,                   // no binary data
                lpszStrings,         // array of strings
                NULL);               // no binary data
    }
}
VOID SvcReportError(LPCTSTR szFunc, LPCTSTR szStr) {
  LPCTSTR lpszStrings[2];

  if(g_app_mode == 1) {
    lmice_error_wprint(L"%s:%s\n", szFunc, szStr);
    return;
  }

  if (NULL != gEventSource) {
    lpszStrings[0] = szFunc;
    lpszStrings[1] = szStr;

    ReportEvent(gEventSource,        // event log handle
                EVENTLOG_ERROR_TYPE, // event type
                0,                   // event category
                SVC_RUNTIME_ERROR,   // event identifier
                NULL,                // no security identifier
                2,                   // size of lpszStrings array
                0,                   // no binary data
                lpszStrings,         // array of strings
                NULL);               // no binary data

    //
  }
}


int main1() {
#ifdef _DEBUG
  _CrtMemState state;
  _CrtMemCheckpoint(&state);
  //_CrtSetBreakAlloc(86);
  // xmalloc(10);
#endif
  {
    HRESULT hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    rpc_session ses;

    iocp_server *server = NULL;

    ses = create_rpc_session();

    server = iocp_server_start(5000);

    wsserver_start(server, get_accept_callback(&ses), 0);

    iocp_server_join(server);

    iocp_server_stop(server);

    close_rpc_session(&ses);

    ::CoUninitialize();
  }

#ifdef _DEBUG
  printf("dump leaks\n");
  _CrtDumpMemoryLeaks();
#endif
  return 0;
}
