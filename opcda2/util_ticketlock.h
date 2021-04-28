/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */
#ifndef OPCDA2_UTIL_TICKETLOCK_H_
#define OPCDA2_UTIL_TICKETLOCK_H_

#if defined(_WIN32)
#define eal_tid_t unsigned int
#elif defined(__LINUX__)
#define eal_tid_t pid_t
#endif

union ticketlock_u {
    unsigned u;
    struct {
        unsigned short ticket;
        unsigned short users;
    } s;
};

struct ticketlock_s {
    eal_tid_t threadid;
    union ticketlock_u ticket;
};

typedef union ticketlock_u ticketlock_t;

__inline void eal_ticket_lock(ticketlock_t *t);
__inline void eal_ticket_unlock(ticketlock_t *t);
__inline int eal_ticket_trylock(ticketlock_t *t);
__inline int eal_ticket_lockable(ticketlock_t *t);

#endif  /** OPCDA2_UTIL_TICKETLOCK_H_ */
