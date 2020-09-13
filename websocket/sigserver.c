#include "sigserver.h"
#include "kqueueserver.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile int opt_quit_flag = 0;

static int sig_proc(void* ptr);
static void sig_close(void* ptr);

int sig_proc(void* ptr) {
  kq_data* data = (kq_data*)(ptr);
  if (data->fd == SIG_TYPE_QUIT) {
    opt_quit_flag = 1;
  } else if (data->fd == SIG_TYPE_INFO) {
    printf("proc info\n");
  }

  return 0;
}

void sig_close(void* ptr) {
  kq_data* data = (kq_data*)(ptr);
  signal(data->fd, SIG_DFL);
}

int sigserver_start(void* ptr) {
  kq_server* svr = (kq_server*)(ptr);
  kq_data* data;
  /* sig quit */
  data = (kq_data*)malloc(sizeof(kq_data));
  memset(data, 0, sizeof(kq_data));

  data->type = SIG_TYPE_QUIT | KQ_TYPE_SIGNAL;
  data->fd = SIG_TYPE_QUIT;
  data->proc_callback = sig_proc;
  data->proc_close = sig_close;

  signal(SIG_TYPE_QUIT, SIG_IGN);

  svr->kq_register(svr, data);

  /* sig info */
  data = (kq_data*)malloc(sizeof(kq_data));
  memset(data, 0, sizeof(kq_data));

  data->type = SIG_TYPE_INFO | KQ_TYPE_SIGNAL;
  data->fd = SIG_TYPE_INFO;
  data->proc_callback = sig_proc;
  data->proc_close = sig_close;

  signal(SIG_TYPE_QUIT, SIG_IGN);

  svr->kq_register(svr, data);

  return 0;
}
