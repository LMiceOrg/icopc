/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */

#include <stdio.h>

#include "heap.h"
#include "iocpserver.h"

int main(int argc, char* argv[]) {
    iocp_server* svr;
    void*        p;

    svr = iocp_server_start(100);

    {
        miheap_report(svr->heap);
        p = svr->iocp_malloc(svr, 512);
        memset(p, 0, 512);
        miheap_report(svr->heap);
        miheap_info(svr->heap, p);
    }
    getchar();

    miheap_report(svr->heap);
    miheap_info(svr->heap, p);
    svr->iocp_free(svr, p);
    miheap_report(svr->heap);

    iocp_server_stop(svr);
    return 0;
}
