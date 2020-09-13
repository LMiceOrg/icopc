
#ifndef KQUEUESERVER_H
#define KQUEUESERVER_H
#include <pthread.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int type;
  int fd; /* file descriptor or signal or process id */
  int (*proc_callback)(void* ptr);
  void (*proc_close)(void* ptr);
} kq_data;

struct kq_data_list {
  kq_data* data[32];
  struct kq_data_list* next;
};
typedef struct kq_data_list kq_datas;

typedef struct kq_server_s {
  volatile int64_t lock;
  pthread_t pt;
  int kq;
  kq_datas* ins;
  kq_datas* registers;
  kq_datas* unregisters;

  void (*kq_register)(struct kq_server_s* ser, kq_data* data);
  void (*kq_unregister)(struct kq_server_s* ser, kq_data* data);

} kq_server;

enum kq_type {
  KQ_TYPE_SIGNAL = 1 << 16,
  KQ_TYPE_READ = 1 << 17,
  KQ_TYPE_WRITE = 1 << 18
};

kq_server* kqueue_server_start(void);

void kqueue_server_stop(kq_server* svr);

void kqueue_server_join(kq_server* svr);

#ifdef __cplusplus
}
#endif

#endif  // KQUEUESERVER_H
