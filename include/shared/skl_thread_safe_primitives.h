#pragma once

// NOTE(marvin): Right now we rely on std library's atomic, since it's
// cross-platform and gets the job done. Ideas of having multiple
// implementaiton of the interface, using the intrinsics. Usage code
// should mark the stores as volatile, but the std implementation
// doesn't require volatile, so this particular implementation casts
// the volatile-ness away.

#include <atomic>

#include <meta_definitions.h>

inline u64 AtomicExchangeU64(u64 volatile *store_, u64 newValue)
{
    u64 *store = const_cast<u64 *>(store_);
    std::atomic_ref<u64> atomicRef(*store);
    return atomicRef.exchange(newValue);
}

// Adds the given addend to the value of the given store, atomically,
// and returns the old value of the store.
inline u64 AtomicAddU64(u64 volatile *store_, u64 addend)
{
    u64 *store = const_cast<u64 *>(store_);
    std::atomic_ref<u64> atomicRef(*store);
    return atomicRef.fetch_add(addend);
}


