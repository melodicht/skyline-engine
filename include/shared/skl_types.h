#pragma once

#include <meta_definitions.h>
#include <skl_thread_safe_primitives.h>

// NOTE(marvin): A mutex, where threads get their turn in the order
// that they request for the ticket. Not using SDL Mutexs because I
// think it's more trouble than it's worth.

struct TicketMutex
{
    u64 volatile next;
    u64 volatile serving;
};

inline void BeginTicketMutex(TicketMutex *mutex)
{
    u64 NextTicket = AtomicAddU64(&mutex->next, 1);
    while (NextTicket != mutex->serving) {CPUPause();}
}

inline void EndTicketMutex(TicketMutex *mutex)
{
    AtomicAddU64(&mutex->serving, 1);
}
