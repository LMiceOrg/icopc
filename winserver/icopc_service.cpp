#include "stdafx.h"

#include <Windows.h>
#include <shellapi.h>
#include <Shlwapi.h>

#include <tchar.h>
#include <strsafe.h>
#include <signal.h>

#include <string>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Shlwapi.lib")

int signal_process(void);
VOID WINAPI service_process( DWORD dwArgc, LPTSTR *lpszArgv );
int manage_process(int argc, wchar_t* argv[]);

int signal_process() {
    LPWSTR* szArglist;
    int nArgs;
	int i;
	std::wstring action;

	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if( NULL == szArglist )
	{
		//wprintf(L"CommandLineToArgvW failed\n");
		return 0;
	}
	else 
	{
		std::wstring ws=L"-s";
		std::wstring ws2 = L"--signal";
		for( i=0; i<nArgs-1; i++) 
		{
			if( ws.compare(szArglist[i]) == 0 ||
				ws2.compare(szArglist[i]) == 0)
			{
				action = szArglist[i+1];
				break;
			}
			//wprintf(L"%d: %ws\n", i, szArglist[i]);
		}
		//wprintf(L"%d: %ws\n", i, szArglist[i]);
		//if(nArgs -1 > i)
		//wprintf(L"%d: %ws\n", i+1, szArglist[nArgs-1]);
	}
	// Free memory allocated for CommandLineToArgvW arguments.

	LocalFree(szArglist);

	/// process signal
	if(action.compare(L"stop") == 0)
	{

		return 1;

	}

	return 0;
}