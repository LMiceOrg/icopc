#ifndef EPOLLSERVER_H
#define EPOLLSERVER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int ep_create_intervaltimer(int sec, int64_t nsec);

void ep_proc_timer(void *ptr);

int ep_register_fd(int fd, void *ptr);
int ep_unregister_fd(int fd);

#ifdef __cplusplus
}
#endif

int epollserver_start(void);

enum ep_type { MC_TYPE_SEND = 0x0C, BC_TYPE_SEND = 0x0D };
typedef struct {
  int type;
  int fd;
  int (*proc_callback)(void *ptr);
  void (*proc_close)(void *ptr);
} ep_data;
#endif // EPOLLSERVER_H
