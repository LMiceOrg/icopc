// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__B76452ED_8818_4DD3_8E67_7257E1492629__INCLUDED_)
#define AFX_STDAFX_H__B76452ED_8818_4DD3_8E67_7257E1492629__INCLUDED_

// Change these values to use different versions
#define WINVER		0x0400
//#define _WIN32_WINNT	0x0400
#define _WIN32_IE	0x0400
#define _RICHEDIT_VER	0x0100

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <atlwin.h>

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlctrlw.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "wtclient/opcda.h"
#include "wtclient/opc_ae.h"
#include "wtclient/WTclientAPI.h"
#include "wtclient/WtclientAPIex.h"
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__B76452ED_8818_4DD3_8E67_7257E1492629__INCLUDED_)
