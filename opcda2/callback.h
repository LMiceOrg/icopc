/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_CALLBACK_H_
#define OPCDA2_CALLBACK_H_

#include "opcda2_all.h"
#include "group.h"

/* support IUnknown, IOPCDataCallback IOPCShutdown
Important: call Release when finish
*/
int opcda2_callback_create(IUnknown **cb, data_list* datas);

int opcda2_shutdown_new(IUnknown** cb);
void opcda2_shutdown_del(IUnknown** cb);

#endif /**  OPCDA2_CALLBACK_H_ */
