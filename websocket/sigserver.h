#ifndef SIGSERVER_H
#define SIGSERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/* quit flag */
extern volatile int opt_quit_flag;

enum sig_type {
  SIG_TYPE_QUIT = 2, /* SIGINT */
  SIG_TYPE_INFO = 29 /* SIGINFO */
};

int sigserver_start(void *ptr);

#ifdef __cplusplus
}
#endif

#endif  // SIGSERVER_H
