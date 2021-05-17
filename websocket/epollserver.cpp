
/** c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** system */
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>

//#include <pthread.h>

/** c++ */
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "epollserver.h"
#include "lmlidar_types.h"

#define MAXEVENTSSIZE 16
#define TIMEWAIT 100

std::mutex g_epoll_mutex;
std::map<int, void *> g_epoll_registers;
std::vector<int> g_epoll_unregisters;

std::map<int, void *> g_epoll_ins;

void epollserver_main(void);
void epollserver_proc_error(int ep, ep_data *data);
void epollserver_ctl(int ep);

void epollserver_ctl(int ep) {
  int ret;
  std::lock_guard<std::mutex> lock(g_epoll_mutex);
  for (auto ite = g_epoll_registers.begin(); ite != g_epoll_registers.end();
       ++ite) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = ite->second;

    ret = epoll_ctl(ep, EPOLL_CTL_ADD, ite->first, &ev);
    printf("epoll add %d %d\r\n", ite->first, ret);
    g_epoll_ins.insert(std::pair<int, void *>(ite->first, ite->second));
  }

  for (auto ite = g_epoll_unregisters.begin(); ite != g_epoll_unregisters.end();
       ++ite) {
    struct epoll_event ev;
    ret = epoll_ctl(ep, EPOLL_CTL_DEL, *ite, &ev);

    printf("epoll del %d %d\n", *ite, ret);
    auto in_ite = g_epoll_ins.find(*ite);
    if (in_ite != g_epoll_ins.end()) {
      g_epoll_ins.erase(in_ite);
    }
  }

  g_epoll_registers.clear();
  g_epoll_unregisters.clear();
}

int ep_create_intervaltimer(int sec, int64_t nsec) {
  struct itimerspec timer;
  // struct timespec now;
  int ret;

  // clock_gettime(CLOCK_MONOTONIC, &now);
  // printf("now :%ld %ld\n", now.tv_sec, now.tv_nsec);

  timer.it_value.tv_nsec = 0;       // nsec;
  timer.it_value.tv_sec = 1;        // sec;
  timer.it_interval.tv_nsec = nsec; // nsec;
  timer.it_interval.tv_sec = sec;   // sec;

  int tmfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  if (tmfd == -1)
    return tmfd;

  ret = timerfd_settime(tmfd, 0, &timer, NULL);
  if (ret == -1)
    return ret;

  return tmfd;
}

int ep_register_fd(int fd, void *ptr) {
  std::lock_guard<std::mutex> lock(g_epoll_mutex);
  g_epoll_registers.insert(std::pair<int, void *>(fd, ptr));
  return 0;
}

int ep_unregister_fd(int fd) {
  std::lock_guard<std::mutex> lock(g_epoll_mutex);
  g_epoll_unregisters.push_back(fd);
  return 0;
}

int epollserver_start(void) {
  std::thread trd(epollserver_main);
  trd.detach();
  return 0;
}

void epollserver_main(void) {
  int ret;
  int ep;
  ep = epoll_create(MAXEVENTSSIZE);
  if (ep == -1) {
    // perror("epoll_create");
    return;
  }

  int nfds = 0;
  struct epoll_event events[MAXEVENTSSIZE];
  memset(events, 0, sizeof(struct epoll_event) * MAXEVENTSSIZE);
  while (opt_quit_flag == 0) {
    sigset_t sigmask;
    memset(&sigmask, 0, sizeof(sigmask));
    nfds = epoll_pwait(ep, events, MAXEVENTSSIZE, TIMEWAIT, &sigmask);
    // printf("epollwait %d\r\n", nfds);
    for (int i = 0; i < nfds; i++) {
      ep_data *data = NULL;
      data = (ep_data *)(events[i].data.ptr);
      if (data == NULL)
        continue;
      // printf("epollproc: %d %p\n", data->type, data);
      //  处理epoll错误
      if (events[i].events & EPOLLERR) {
        printf("epoll error\r\n");
        epollserver_proc_error(ep, data);
      } else {
        // 处理数据
        // printf("proc callback data\n");
        ret = data->proc_callback(data);
        if (ret == -1) {
          // 处理逻辑错误
          epollserver_proc_error(ep, data);
        }
      }
    }
    epollserver_ctl(ep);
  };

  // clean
  close(ep);
  {
    std::lock_guard<std::mutex> lock(g_epoll_mutex);
    for (auto ite = g_epoll_ins.begin(); ite != g_epoll_ins.end(); ++ite) {
      int fd = ite->first;
      void *ptr = ite->second;
      ep_data *data;
      data = (ep_data *)(ptr);

      data->proc_close(data);
      close(fd);
      free(ptr);
    }
  }
}

void epollserver_proc_error(int ep, ep_data *data) {
  int fd;
  struct epoll_event ev;
  fd = data->fd;
  epoll_ctl(ep, EPOLL_CTL_DEL, data->fd, &ev);
  close(data->fd);

  data->proc_close(data);
  free(data);
  {
    std::lock_guard<std::mutex> lock(g_epoll_mutex);
    g_epoll_ins.erase(fd);
  }
}

void ep_proc_timer(void *ptr) {
  uint64_t buf;
  ep_data *data;
  data = (ep_data *)(ptr);
  read(data->fd, &buf, sizeof(uint64_t));
}
