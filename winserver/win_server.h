#ifndef WIN_SERVER_H
#define WIN_SERVER_H

/* windows server */
/** name of the executable */
#define SZAPPNAME            "ICOPCSvc"
/** internal name of the service */
#define SZSERVICENAME        L"ICOPCService"
// displayed name of the service
#define SZSERVICEDISPLAYNAME L"WWHY ICOPC Service"
// list of service dependencies - "dep1\0dep2\0\0"
#define SZDEPENDENCIES       "\0\0"


#define SZREGKEY _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\ICOPCService")


#endif // WIN_SERVER_H
