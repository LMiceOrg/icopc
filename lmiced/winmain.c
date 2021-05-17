/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */

#include "iocpserver.h"

int main(int argc, char* argv[]) {
    iocp_server * svr;
    svr = iocp_server_start(100);

    Sleep(1000);
    iocp_server_stop(svr);
    return 0;
}

