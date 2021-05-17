/** Copyright 2018, 2021 He Hao<hehaoslj@sina.com> */

#include "util_ticketlock.h"
#include "util_atomic.h"

void eal_ticket_lock(ticketlock_t *t) {
    unsigned short me = eal_atomic_xadd(&t->s.users, 1);

    while (t->s.ticket != me) cpu_relax();
}

void eal_ticket_unlock(ticketlock_t *t)
{
    barrier();
    t->s.ticket++;
}

int eal_ticket_trylock(ticketlock_t *t)
{
    unsigned me = t->s.users;
    unsigned menew = me + 1;
    unsigned cmp = (me << 16) + me;
    unsigned cmpnew = (menew << 16) + me;

    if (eal_atomic_cmpxchg(&t->u, cmp, cmpnew) == cmp) return 0;

    return 1;
}

int eal_ticket_lockable(ticketlock_t *t)
{
    ticketlock_t u = *t;
    barrier();
    return (u.s.ticket == u.s.users);
}
