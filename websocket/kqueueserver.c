/** Copyright 2018, 2019 He Hao<hehaoslj@sina.com> */

/** c headers */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** c++ headers */

/** system headers */

#include <arpa/inet.h>
#include <fcntl.h>

#include <pthread.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "lmice_eal_common.h"
#include "lmice_eal_spinlock.h"

#include "kqueueserver.h"
#include "wsserver.h"

#define MAX_EVENT_COUNT 16

extern volatile int opt_quit_flag;

static void inline kq_datas_clean(kq_datas* klist);
static void inline kq_datas_erase(kq_datas* klist, kq_data* one);
static inline void kq_datas_insert(kq_datas* klist, kq_data* one);

static inline int kq_data_filter(kq_data* data);

static inline void* kqueue_server_main(void*);

static inline void kqueue_server_proc_close(kq_server* svr, kq_data* data);

static inline void kqueue_server_ctl(kq_server* svr);

static inline void kq_datas_insert(kq_datas* klist, kq_data* one) {
  while (klist != NULL) {
    for (int i = 0; i < 32; ++i) {
      kq_data* data = klist->data[i];
      if (data == NULL) {
        klist->data[i] = one;
        return;
      }
    }
    if (klist->next == NULL) {
      kq_datas* new_datas = (kq_datas*)malloc(sizeof(kq_datas));
      memset(new_datas, 0, sizeof(kq_datas));
      klist->next = new_datas;
      new_datas->data[0] = one;
      return;
    }
    klist = klist->next;
  }
}

static void kq_register(struct kq_server_s* svr, kq_data* newone) {
  eal_spin_lock(&svr->lock);

  kq_datas* klist = svr->registers;

  kq_datas_insert(klist, newone);

  eal_spin_unlock(&svr->lock);
}

static void kq_unregister(struct kq_server_s* svr, kq_data* newone) {
  eal_spin_lock(&svr->lock);

  kq_datas* klist = svr->unregisters;

  kq_datas_insert(klist, newone);

  eal_spin_unlock(&svr->lock);
}

void kqueue_server_join(kq_server* svr) { pthread_join(svr->pt, NULL); }

void kqueue_server_stop(kq_server* svr) {
  opt_quit_flag = 1;
  pthread_join(svr->pt, NULL);
  free(svr);
}

kq_server* kqueue_server_start(void) {
  kq_server* server;
  //  pthread_attr_t pa;

  //  pthread_attr_init(&pa);
  //  pthread_attr_setdetachstate(&pa, PTHREAD_CREATE_DETACHED);

  server = (kq_server*)malloc(sizeof(kq_server));
  memset(server, 0, sizeof(kq_server));

  server->registers = (kq_datas*)malloc(sizeof(kq_datas));
  memset(server->registers, 0, sizeof(kq_datas));
  server->unregisters = (kq_datas*)malloc(sizeof(kq_datas));
  memset(server->unregisters, 0, sizeof(kq_datas));
  server->ins = (kq_datas*)malloc(sizeof(kq_datas));
  memset(server->ins, 0, sizeof(kq_datas));

  server->kq_register = kq_register;
  server->kq_unregister = kq_unregister;

  // IOThread mythread;
  // mythread = IOCreateThread(&kqueue_server_main, (void*)server);

  pthread_create(&server->pt, NULL, kqueue_server_main, server);

  return server;
}

void* kqueue_server_main(void* ptr) {
  int ret;
  int kq;
  int ev_count;
  struct kevent events[MAX_EVENT_COUNT];
  struct timespec time_wait;

  kq_server* svr = (kq_server*)(ptr);

  // signal(SIGINFO, SIG_IGN);

  /* init kqueue */
  kq = kqueue();
  svr->kq = kq;

  //  // init websocket
  //  wsfd = ws_listen();
  //  if (wsfd == -1) {
  //    return -1;
  //  }

  //  int* listendata = (int*)malloc(8);
  //  listendata[0] = WS_TYPE_LISTEN;
  //  listendata[1] = WS_TYPE_LISTEN;
  //  register_socket_read(kq, wsfd, listendata);
  //  // printf("listen %p\n", listendata);

  //  // register fd
  //  // register siginfo
  //  register_signal(kq, SIGINFO);

  /* wait 100 mill-secs */
  time_wait.tv_sec = 0;
  time_wait.tv_nsec = 100 * 1000000;

  // block wait event
  while (opt_quit_flag == 0) {
    ev_count = kevent(kq, NULL, 0, events, MAX_EVENT_COUNT, &time_wait);

    // printf("got %d signals\n", ret);
    // handle event
    for (int i = 0; i < ev_count; ++i) {
      struct kevent* ev = events + i;
      kq_data* data = (kq_data*)ev->udata;
      if (data == NULL) continue;

      if (ev->flags & EV_ERROR) {
        kqueue_server_proc_close(svr, data);
        //        struct kevent changes[1];
        //        EV_SET(&changes[0], ev->ident, ev->filter, EV_DELETE, 0, 0,
        //        NULL); kevent(kq, changes, 1, NULL, 0, NULL);
        printf("kq: fd error\n");
      } else if (ev->flags & EV_EOF) {
        kqueue_server_proc_close(svr, data);
        //        printf("got eof %p\n", ev->udata);
        //        unregister_socket_read(kq, ev->ident);
        //        free(ev->udata);
        //        shutdown(ev->ident, SHUT_RDWR);
        //        close(ev->ident);
        //        printf("goted eof\n");
        printf("kq: fd eof\n");
      } else {
        printf("kq:proc \n");
        ret = data->proc_callback(data);
        if (ret == -1) {
          kqueue_server_proc_close(svr, data);
        }
      }
    }
    kqueue_server_ctl(svr);
  }

  // clean
  close(kq);

  eal_spin_lock(&svr->lock);

  kq_datas_clean(svr->ins);
  svr->ins = NULL;

  kq_datas_clean(svr->registers);
  svr->registers = NULL;

  kq_datas_clean(svr->unregisters);
  svr->unregisters = NULL;

  eal_spin_unlock(&svr->lock);

  return NULL;
}

void kq_datas_clean(kq_datas* klist) {
  while (klist != NULL) {
    kq_data* data;
    for (int i = 0; i < 32; ++i) {
      data = klist->data[i];
      if (data) {
        data->proc_close(data);
        close(data->fd);
        free(data);
      }
    }
    kq_datas* tmp = klist;
    klist = klist->next;
    free(tmp);
  }
}

void kqueue_server_ctl(kq_server* svr) {
  kq_datas* klist;

  /* step 1. get lock */
  eal_spin_lock(&svr->lock);

  /* step 2. register events */
  klist = svr->registers;
  while (klist != NULL) {
    kq_data* data;
    for (int i = 0; i < 32; ++i) {
      struct kevent changes[1];
      int filter;

      data = klist->data[i];
      if (data == NULL) continue;

      filter = kq_data_filter(data);
      EV_SET(&changes[0], data->fd, filter, EV_ADD, 0, 0, data);

      kevent(svr->kq, changes, 1, NULL, 0, NULL);

      kq_datas_insert(svr->ins, data);

      klist->data[i] = NULL;
    }

    klist = klist->next;
  }

  /* step 3. unregister events */
  klist = svr->unregisters;
  while (klist != NULL) {
    kq_data* data;
    for (int i = 0; i < 32; ++i) {
      struct kevent changes[1];
      int filter;

      data = klist->data[i];
      if (data == NULL) continue;

      filter = kq_data_filter(data);
      EV_SET(&changes[0], data->fd, filter, EV_DELETE, 0, 0, data);

      kevent(svr->kq, changes, 1, NULL, 0, NULL);

      kq_datas_insert(svr->ins, data);

      klist->data[i] = NULL;
    }

    klist = klist->next;
  }

  /* step 4. unlock */
  eal_spin_unlock(&svr->lock);
}

static void inline kq_datas_erase(kq_datas* klist, kq_data* one) {
  while (klist != NULL) {
    kq_data* data;
    for (int i = 0; i < 32; ++i) {
      data = klist->data[i];
      if (data == one) {
        klist->data[i] = NULL;
        return;
      }
    }
    klist = klist->next;
  }
}

void kqueue_server_proc_close(kq_server* svr, kq_data* data) {
  int filter;
  if (data) {
    data->proc_close(data);

    filter = kq_data_filter(data);
    if (filter != EVFILT_SIGNAL) close(data->fd);

    free(data);

    eal_spin_lock(&svr->lock);
    kq_datas_erase(svr->ins, data);
    eal_spin_unlock(&svr->lock);
  }
}

int kq_data_filter(kq_data* data) {
  int flt = EVFILT_READ;
  if (data->type & KQ_TYPE_SIGNAL) {
    flt = EVFILT_SIGNAL;
  } else if (data->type & KQ_TYPE_READ) {
    flt = EVFILT_READ;
  } else if (data->type & KQ_TYPE_WRITE) {
    flt = EVFILT_WRITE;
  }
  return flt;
}
