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

void eal_ticket_lock(ticketlock_t *t);
void eal_ticket_unlock(ticketlock_t *t);
int eal_ticket_trylock(ticketlock_t *t);
int eal_ticket_lockable(ticketlock_t *t);

#define EAL_ticket_lock(tlock)                     \
    do {                                           \
        ticketlock_u * t = (ticketlock_u *) tlock; \
        unsigned short me;                         \
        me = eal_atomic_xadd(&t->s.users, 1);      \
        while (t->s.ticket != me) cpu_relax();     \
    } while (0)

#define EAL_ticket_unlock(tlock)                  \
    do {                                          \
        ticketlock_u *t = (ticketlock_u *) tlock; \
        barrier();                                \
        t->s.ticket++;                            \
    } while (0)

#endif  /** OPCDA2_UTIL_TICKETLOCK_H_ */
