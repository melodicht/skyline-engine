// NOTE(marvin): Right now we rely on std library's atomic, since it's
// cross-platform and gets the job done. Ideas of having multiple
// implementaiton of the interface, using the intrinsics.

#include <atomic>

inline u64 AtomicExchangeU64(u64 *store, u64 newValue)
{
    std::atomic_ref<u64> atomicRef(*store);
    return atomicRef.exchange(newValue);
}

// Adds the given addend to the value of the given store, atomically,
// and returns the old value of the store.
inline u64 AtomicAddU64(u64 *store, u64 addend)
{
    std::atomic_ref<u64> atomicRef(*store);
    return atomicRef.fetch_add(addend);
}


