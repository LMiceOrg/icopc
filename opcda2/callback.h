/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_CALLBACK_H_
#define OPCDA2_CALLBACK_H_

#include "opcda2_all.h"

struct server_connect;

/* support IUnknown, IOPCDataCallback IOPCShutdown
Important: call Release when finish
每个OPCServer连接可以共享同一个callback对象
*/
int opcda2_callback_create(struct server_connect* conn, IUnknown **cb);

#endif /**  OPCDA2_CALLBACK_H_ */
