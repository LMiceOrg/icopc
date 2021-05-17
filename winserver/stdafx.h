#if !defined(AFX_STDAFX_H__B76452ED_8818_4DD3_8E67_7257E1492628__INCLUDED_)
#define AFX_STDAFX_H__B76452ED_8818_4DD3_8E67_7257E1492628__INCLUDED_

// Change these values to use different versions
#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define _WIN32_IE 0x0400
#define _RICHEDIT_VER 0x0100

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Ws2tcpip.h>


#include <atlbase.h>
#include <atlapp.h>

#include <atlcrack.h>
#include <atlmisc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include <Windows.h>

#include "../wtclient/opc_ae.h"
#include "../wtclient/opcda.h"
#include "../wtclient/WTclientAPI.h"
#include "../wtclient/WtclientAPIex.h"


#include "../call_thunk/call_thunk.h"

//#define xmalloc(s) malloc(s)
//#define xfree(p)   free(p)
//#define xmalloc(s) HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(s))
//#define xfree(p)   HeapFree(GetProcessHeap(),0,(p))
#define winserver_malloc(s) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (s))
#define winserver_free(p) HeapFree(GetProcessHeap(), 0, (p))

#endif // !defined(AFX_STDAFX_H__B76452ED_8818_4DD3_8E67_7257E1492628__INCLUDED_)
