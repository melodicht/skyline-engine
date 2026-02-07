#pragma once

#include <meta_definitions.h>

#if SKL_INTERNAL

#include <memory_types.h>
#include <game_platform.h>
#include <timer.h>
#include <skl_thread_safe_primitives.h>

#endif

// This file is responsible for declaring the definitions of the debug
// infrastructure, in which there is profiling of two major pieces,
// time and space. The timed blocks have a bug that if they exist but
// the code path does not cross them, there will be a crash.

// TODO(marvin): Turning off debug temporarily because it's messing with looped-live playback.
#if 0

/**
 * SPACE
 */

// NOTE(marvin): These definitions are isomorphisms of the actual
// memory arena. It contains extra metadata (where it is located,
// history of allocations, etc), that is stored in a new arena
// dedicated to the debug arena. The memory arena themselves don't
// care about the history of allocations, and view an area as memory
// used over total memory size. However, the debug infrastructure
// cares about all previous allocations, and so tracks all previous
// allocations in a list. As arenas can contain arbitrarily many
// subarenas (and those subarenas can also contain subarenas) as well
// as arbitrarily many regular allocations, arenas are encoded in
// n-ary trees.

// NOTE(marvin): (Somewhat) math terminology is used. The actual
// memory arena which we are mapping to the corresponding
// representation in the debug infrastructure is called the
// source. The corresponding representation in the debug
// infrastructure is called the target. We say that the source memory
// arena is isomorphic to the target memory arena.

// NOTE(marvin): The debug infrastructure is dealing with memory
// arenas, and guess what, the debug infrastructure itself needs
// memory arenas! If a memory arena doesn't have the word "source",
// then it's a memory arena for the debug infrastructure.

enum DebugGeneralAllocationType
{
    allocationType_none = 0,
    allocationType_arena = 1,
    allocationType_regular = 2,
};

struct DebugGeneralAllocation;

// Essentially a deque, where the sentinel is a zeroed out general allocation.
struct DebugAllocations
{
    // NOTE(marvin): Has to be a pointer due to circular dependency...
    DebugGeneralAllocation *sentinel;
};

// NOTE(marvin): The base is used for finding the target associated
// with a source. Could also have a mapping from source to debug ID
// and use debug ID for the search, but would need to create and
// maintain a map, would be more work. Something to consider in the
// future.
struct DebugArena
{
    u32 totalSize;
    u08 *base;
    u32 used;
    DebugAllocations allocations;
};

struct DebugRegularAllocation
{
    u32 offset;
    u32 size;
};


// NOTE(marvin): This struct represents the node in the conceptual
// tree.
// Null-terminated.
struct DebugGeneralAllocation
{
    u32 id;  // For indexing into the pool.
    const char *debugID;
    const char *name;
    DebugGeneralAllocationType type;
    DebugGeneralAllocation *prev;
    DebugGeneralAllocation *next;
    union
    {
        DebugArena arena;
        DebugRegularAllocation regular;
    };
};

// If an allocation is free, its allocation type is none.
struct DebugAllocationsStorePool
{
    DebugGeneralAllocation *arena;
    u32 count;
};

// NOTE(marvin): For book-keeping DebugGeneralAllocation memory allocations.
// TODO(marvin): Abstract against ecs' entities pool + free indices stack maybe. Reference NewEntity and DestroyEntity for functionality.
struct DebugAllocationsStore
{
    // NOTE(marvin): In this memory scheme, note that there are two
    // redundant notions of free. One specified by the free indices,
    // and the other specified by what the object in the pool means
    // for it to be free. Those two notions must remain in sync at all
    // times. The reason for redundance is convenience. The free
    // indices is useful for figuring out the next available slot to
    // allocate without needing to iterate through the whole
    // thing. The other notion is useful if one would like to iterate
    // through the pool and knowing which indices are free without
    // needing to refer to the free indices.
    DebugAllocationsStorePool pool;
    FreeIndicesStack freeIndices;

    // TODO(marvin): The debug free indices stack is pretty useless. Suppose an allocation is removed, thus its position is pushed to the free indices stack. The push is recorded, taking that free index. The free index ends up getting used by the debug process to talk about the free index, which is a waste. This either suggests that the free indices stack is too heavy, or that some parts of the debug memory should just not be recorded... For now, we surpress this case...
    // TODO(marvin): The free indices stack never decreases in size. If an element is popped from the stack, that pop is recorded by the debug system, which requires an allocation object, thus taking that which was just popped. When there's frequent pushes and pops, then the free indices stack becomes a ticking time bomb. This is livable for now... but should re-consider the memory viewer's design...
};

// TODO(marvin): The XBuffer familiy could be abstracted maybe.
struct DebugIDsBuffer
{
    char **base;
    u32 count;
};

struct DebugState
{
    // NOTE(marvin): The non-store is the tree, the store is the memory arena holding the nodes of the tree.
    DebugAllocations targets;

    // NOTE(marvin): Reason for this flag is due to the TODO in the DebugAllocationsStore...
    b32 canUseFreeIndicesStack;
    DebugAllocationsStore targetsStore;

    // NOTE(marvin): Nothing gets freed in here! Will need to work out
    // a better memory management if we need to free stuff.
    MemoryArena miscArena;
    DebugIDsBuffer debugIDsIndex;

    // NOTE(marvin): For purposes of the initialization process.
    b32 readyToInitMemoryArena_;
    b32 readyToRegularAllocate_;
};

extern DebugState *globalDebugState;


// NOTE(marvin): The reason for the macros is to have them compiled
// away in non-internal release.

#define DebugInitialize(...) DebugInitialize_(__VA_ARGS__)
#define DebugUpdate(...) DebugUpdate_(__VA_ARGS__)
#define DebugRecordInitMemoryArena(...) DebugRecordInitMemoryArena_(__VA_ARGS__)
#define DebugRecordSubArena(...) DebugRecordSubArena_(__VA_ARGS__)
#define DebugRecordPushSize(...) DebugRecordPushSize_(__VA_ARGS__)
#define DebugRecordPopSize(...) DebugRecordPopSize_(__VA_ARGS__)

void DebugInitialize_(GameMemory gameMemory);
void DebugUpdate_(GameMemory gameMemory);

void DebugRecordInitMemoryArena_(const char *debugID, const char *name, MemoryArena source);

void DebugRecordSubArena_(const char *debugID, const char *name, MemoryArena *sourceContainingrena, MemoryArena subArenaSource);

void DebugRecordPushSize_(const char *debugID, MemoryArena *source, siz requestedSize, siz actalSize);

void DebugRecordPopSize_(MemoryArena *source, siz size);

#else

#define DebugInitialize(...)
#define DebugUpdate(...)
#define DebugRecordInitMemoryArena(...)
#define DebugRecordSubArena(...)
#define DebugRecordPushSize(...)
#define DebugRecordPopSize(...)

#endif

/**
 * TIME
 */




#if SKL_INTERNAL

#define NAMED_TIMED_BLOCK_(name, number, ...) TimedBlock timedBlock_##number = TimedBlock(__COUNTER__, __FILE__, __LINE__, #name, ## __VA_ARGS__)
#define NAMED_TIMED_BLOCK(name, ...) NAMED_TIMED_BLOCK_(name, __LINE__, ## __VA_ARGS__)

#define TIMED_BLOCK_(number, ...) NAMED_TIMED_BLOCK_(__FUNCTION__, number, ## __VA_ARGS__)
#define TIMED_BLOCK(...) TIMED_BLOCK_(__LINE__, ## __VA_ARGS__)

#else

#define NAMED_TIMED_BLOCK_(name, number, ...) 
#define NAMED_TIMED_BLOCK(name, ...) 

#define TIMED_BLOCK_(number, ...) 
#define TIMED_BLOCK(...) 

#endif

struct DebugRecord
{
    const char *fileName;
    const char *blockName;

    u32 lineNumber;
    u32 reserved;

    // NOTE(marvin): Top 32-bits hit count, bottom 32-bits cycle count.
    u64 hitCount_cycleCount;
};

extern DebugRecord debugRecordArray[];

struct TimedBlock
{
    DebugRecord *debugRecord;
    u64 startCycleCount;
    u32 hitCount;


    #if SKL_INTERNAL
    TimedBlock(u32 index, const char *fileName, u32 lineNumber,
               const char *blockName, u32 hitCount0 = 1)
    {
        debugRecord = debugRecordArray + index;
        debugRecord->fileName = fileName;
        debugRecord->lineNumber = lineNumber;
        debugRecord->blockName = blockName;
        startCycleCount = ReadCPUTimer();
        hitCount = hitCount0;
    }

    ~TimedBlock()
    {
        u64 cycleCountDelta = ReadCPUTimer() - startCycleCount;
        u64 hitCountDelta = (u64)hitCount;
        u64 delta = cycleCountDelta | (hitCountDelta << 32);
        AtomicAddU64(&debugRecord->hitCount_cycleCount, delta);
    }
    #endif
};

