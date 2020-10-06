#include "stdafx.h"

#include "win_server.h"
#include "icopc_rpc.h"


#include "../websocket/wsserver.h"

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif


int main() {
    #ifdef _DEBUG
    _CrtMemState state;
    _CrtMemCheckpoint(&state);
    //_CrtSetBreakAlloc(86);
    //xmalloc(10);
#endif
    {
        HRESULT hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);

        rpc_session ses;
    
        iocp_server* server = NULL;

        ses = create_rpc_session();

        server = iocp_server_start(5000);

        wsserver_start(server, get_accept_callback(&ses));

        iocp_server_join(server);

        iocp_server_stop(server);

        close_rpc_session(&ses);

        ::CoUninitialize();
    }

#ifdef _DEBUG
    printf("dump leaks\n");
    _CrtDumpMemoryLeaks( );
#endif
    return 0;
}
