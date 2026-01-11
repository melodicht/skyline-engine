#if SKL_INTERNAL
#include <meta_definitions.h>
#include <debug.h>
#include <memory.h>

constexpr u32 MAX_GENERAL_ALLOCATIONS = 8192;

global_variable DebugState *globalDebugState;

// NOTE(marvin): We store all of our strings (name and debug ID) in
// the miscArena. For name, we always create a fresh new
// one. Currently, names are only for new memory arenas or sub arenas,
// which (for now) is created once on game initialize. As the same
// debug IDs are repeatedly use, every unique debug ID is stored in
// our misc arena exactly once, and whenever a debug record call
// happens, it searches for the debug ID in our misc arena, which all
// of the debug instances use instead.

local DebugState *GetGlobalDebugState()
{
    DebugState *result = globalDebugState;
    return result;
}

local DebugAllocationsStorePool InitDebugAllocationsStorePool(MemoryArena *remainingArena)
{
    DebugAllocationsStorePool result = {};
    result.arena = PushArray(remainingArena, MAX_GENERAL_ALLOCATIONS, DebugGeneralAllocation);
    result.count = 0;
    return result;
}

local void InitDebugAllocationsStoreInPlace(DebugAllocationsStore *store, MemoryArena *remainingArena)
{
    store->pool = InitDebugAllocationsStorePool(remainingArena);
    store->freeIndices = InitFreeIndicesStack(remainingArena, MAX_GENERAL_ALLOCATIONS);
}
    
local DebugAllocations EmptyDebugAllocations()
{
    DebugAllocations result = {};
    result.first = 0;
    return result;
}

local DebugIDsBuffer InitDebugIDsBuffer(MemoryArena *remainingArena)
{
    DebugIDsBuffer result = {};
    result.base = PushArray(remainingArena, MAX_GENERAL_ALLOCATIONS, char *);
    result.count = 0;
    return result;
}

// Returns nullptr if no reference exists.
local char *TryFindReferenceToDebugID(DebugIDsBuffer *debugIDsBuffer, const char *debugID)
{
    for (u32 index = 0; index < debugIDsBuffer->count; ++index)
    {
        char *ourDebugID = debugIDsBuffer->base[index];
        if (strcmp(ourDebugID, debugID) == 0)
        {
            return ourDebugID;
        }
    }

    return 0;
}

local void AddReferenceToDebugID(DebugIDsBuffer *debugIDsBuffer, char *ourDebugID)
{
    char **next = debugIDsBuffer->base + debugIDsBuffer->count;
    *next = ourDebugID;
    ++debugIDsBuffer->count;
}

local void InitGlobalDebugState(MemoryArena *remainingArena)
{
    *globalDebugState = {};
    globalDebugState->targets = EmptyDebugAllocations();
    InitDebugAllocationsStoreInPlace(&globalDebugState->targetsStore, remainingArena);
    globalDebugState->debugIDsIndex = InitDebugIDsBuffer(remainingArena);
    globalDebugState->miscArena = SubArena(remainingArena, remainingArena->size - remainingArena->used);
    globalDebugState->readyToInitMemoryArena_ = true;
}

local void AddAllocation(DebugAllocations *allocations, DebugGeneralAllocation *toAdd)
{
    if (!allocations->first)
    {
        allocations->first = toAdd;
        allocations->last = toAdd;
    }
    else
    {
        DebugGeneralAllocation *secondLast = allocations->last;
        allocations->last = toAdd;
        secondLast->next = toAdd;
        toAdd->next = 0;
    }
}

local DebugGeneralAllocation *GetLastAllocation(DebugAllocations *allocations)
{
    DebugGeneralAllocation *result = allocations->last;
    return result;
}

// Gets the given debug ID in our misc arena. If it doesn't exist, then add it to our misc arena.
char *GetDebugIDFromOurArena(DebugState *debugState, const char *debugID)
{
    DebugIDsBuffer *debugIDsIndex = &debugState->debugIDsIndex;
    char *result = TryFindReferenceToDebugID(debugIDsIndex, debugID);
    if (result == 0)
    {
        b32 before = debugState->readyToRegularAllocate_;
        debugState->readyToRegularAllocate_ = false;
        char *ourDebugID = PushString(&debugState->miscArena, debugID);
        AddReferenceToDebugID(debugIDsIndex, ourDebugID);
        debugState->readyToRegularAllocate_ = before;
        // TODO(marvin): Again, how to know requested vs effective size after allocation has been made? 
        DebugRecordPushSize(MAKE_DEBUG_ID, &debugState->miscArena,
                            strlen(ourDebugID) + 1, strlen(ourDebugID) + 1);
        result = ourDebugID;
    }
    return result;
}

void DebugInitialize_(GameMemory gameMemory)
{
    Assert(sizeof(DebugState) <= gameMemory.debugStorageSize);
    DebugState *debugState = static_cast<DebugState *>(gameMemory.debugStorage);
    globalDebugState = debugState;
    debugState->readyToInitMemoryArena_ = false;
    debugState->readyToRegularAllocate_ = false;
    
    u8 *pastDebugStateAddress = static_cast<u8 *>(gameMemory.debugStorage) + sizeof(DebugState);
    MemoryArena memoryArena = InitMemoryArena(pastDebugStateAddress, gameMemory.debugStorageSize - sizeof(DebugState));
    InitGlobalDebugState(&memoryArena);

    // NOTE(marvin): We do 0 debug records until the debug state is completely initialized.
    DebugRecordInitMemoryArena(MAKE_DEBUG_ID, "DebugArena", memoryArena);
    // TODO(marvin): How to determine if misc arena got an offset?
    u32 miscArenaSize = debugState->miscArena.size;

    // NOTE(marvin): All this is to get around the fact that in order
    // to debug record the misc sub arena, the debug ID strings must
    // be stored in the misc sub arena. We need to store the debug ID
    // string without debug recording that because the misc sub arena
    // hasn't been debug recorded yet.
    const char *miscArenaDebugID = MAKE_DEBUG_ID;
    GetDebugIDFromOurArena(debugState, miscArenaDebugID);
    debugState->readyToRegularAllocate_ = true;
    
    DebugRecordPushSize(miscArenaDebugID, &memoryArena, miscArenaSize, miscArenaSize);
    DebugRecordSubArena(miscArenaDebugID, "DebugMiscArena", &memoryArena, debugState->miscArena);
}

void DebugUpdate_(GameMemory gameMemory)
{
    Assert(sizeof(DebugState) <= gameMemory.debugStorageSize);
    globalDebugState = static_cast<DebugState *>(gameMemory.debugStorage);
}

local DebugGeneralAllocation *GetFromDebugGeneralAllocationPool(DebugAllocationsStorePool *pool, u32 index)
{
    Assert(index < MAX_GENERAL_ALLOCATIONS);
    DebugGeneralAllocation *result = pool->arena + index;
    return result;
}

local DebugGeneralAllocation *AddNewDebugGeneralAllocation(DebugAllocationsStorePool *pool)
{
    Assert(pool->count < MAX_GENERAL_ALLOCATIONS);
    DebugGeneralAllocation *result = pool->arena + pool->count;
    ++pool->count;
    return result;
}

local DebugArena InitDebugArena(MemoryArena sourceArena)
{
    DebugArena result = {};
    result.totalSize = sourceArena.size;
    result.base = sourceArena.base;
    result.used = sourceArena.used;
    result.allocations = EmptyDebugAllocations();
    return result;
}

local DebugRegularAllocation InitDebugRegularAllocation(u32 requestedSize, u32 actualSize)
{
    DebugRegularAllocation result = {};
    result.offset = actualSize - requestedSize;
    result.size = actualSize;
    return result;
}

local DebugGeneralAllocation *NewDebugGeneralAllocation(DebugAllocationsStore *store)
{
    DebugGeneralAllocation *result = 0;
    FreeIndicesStack *freeIndices = &store->freeIndices;
    DebugAllocationsStorePool *pool = &store->pool;
    
    if (!FreeIndicesStackIsEmpty(freeIndices))
    {
        u32 newIndex = PopFreeIndicesStack(freeIndices);
        result = GetFromDebugGeneralAllocationPool(pool, newIndex);
    }
    else
    {
        result = AddNewDebugGeneralAllocation(pool);
    }

    result->type = allocationType_none;
    return result;
}

local void MapSourceArenaToTarget(DebugGeneralAllocation *target, MemoryArena source, MemoryArena *miscArena, const char *ourDebugID, const char *name)
{
    // NOTE(marvin): These have to come before PushString below because
    // the arena needs to be registered in the allocations pool so
    // that all the pushes can find it!
    target->arena = InitDebugArena(source);
    target->type = allocationType_arena;

    target->debugID = ourDebugID;
    target->name = PushString(miscArena, name);
}

local void MapSourceAllocationToTarget(DebugGeneralAllocation *target, u32 requestedSize, u32 actualSize, MemoryArena *miscArena, const char *ourDebugID)
{
    target->debugID = ourDebugID;
    target->name = "";
    target->type = allocationType_regular;
    target->next = 0;
    target->regular = InitDebugRegularAllocation(requestedSize, actualSize);
}

// Assumes that exist.
local DebugArena *FindTargetOfSourceArenaFromPool(DebugAllocationsStorePool *pool, MemoryArena *source)
{
    for (u32 index = 0; index < pool->count; ++index)
    {
        DebugGeneralAllocation *candidate = pool->arena + index;
        if (candidate->type == allocationType_arena)
        {
            DebugArena *candidateTarget = &candidate->arena;
            if (candidateTarget->base == source->base)
            {
                return candidateTarget;
            }
        }
    }

    Assert(false && "Target not found.");
    return 0;
}

local DebugArena *FindTargetOfSourceArena(DebugAllocationsStore *store, MemoryArena *source)
{
    // TODO(marvin): Simple algorithm, O(number of allocated nodes in pool). Maybe a hashing scheme using the debug ID.
    DebugArena *result = FindTargetOfSourceArenaFromPool(&store->pool, source);
    return result;
}

// Assumes that the last allocation is regular.
local void ForceLastAllocationToArena(DebugAllocations *allocations, DebugAllocationsStore *targetStore, MemoryArena subArenaSource, MemoryArena *miscArena, const char *ourDebugID, const char *name)
{
    DebugGeneralAllocation *last = allocations->last;
    Assert(last->type == allocationType_regular);

    DebugRegularAllocation *regular = &last->regular;

    // NOTE(marvin): If there is an offset, the regular allocation
    // object stays, but size gets cleared and move to new arena
    // object. Otherwise, the regular allocation becomes an arena
    // allocation.
    if (regular->offset > 0)
    {
        Assert(regular->size == subArenaSource.size);
        regular->size = 0;
        DebugGeneralAllocation *subArenaTarget = NewDebugGeneralAllocation(targetStore);
        AddAllocation(allocations, subArenaTarget);
        MapSourceArenaToTarget(subArenaTarget, subArenaSource, miscArena, ourDebugID, name);
    }
    else
    {
        last->debugID = ourDebugID;
        last->name = name;
        last->type = allocationType_arena;
        last->arena = InitDebugArena(subArenaSource);
    }
}
 
void DebugRecordInitMemoryArena_(const char *debugID, const char *name, MemoryArena source)
{
    DebugState *debugState = GetGlobalDebugState();
    if (debugState->readyToInitMemoryArena_)
    {
        DebugGeneralAllocation *target = NewDebugGeneralAllocation(&debugState->targetsStore);
        AddAllocation(&debugState->targets, target);
        // NOTE(marvin): A workaround to fill in Debug ID after. See note in MapSourceArenaToTarget.
        MapSourceArenaToTarget(target, source, &debugState->miscArena, "", name);
        target->debugID = GetDebugIDFromOurArena(debugState, debugID);
        
        // TODO(marvin): Record the debugID and name string. Both are available in target here. All under the condition that not ready to regular allocate, and flip that true in that condition.

    }
}

void DebugRecordSubArena_(const char *debugID, const char *name, MemoryArena *sourceContainingArena, MemoryArena subArenaSource)
{
    // NOTE(marvin): The target of the sub arena source doesn't exist
    // yet and is created by this procedure.
    // NOTE(marvin): Current implementation is that
    // DebugRecordPushSize_ is called first, and we have to find that
    // resulting regular allocation, assumed to be last allocation,
    // and turn it into an arena.
    DebugState *debugState = GetGlobalDebugState();
    // NOTE(marvin): As DebugRecordPushSize_ must happen before this
    // call, this call is temporally dependent on that.
    if (debugState->readyToRegularAllocate_)
    {
        DebugArena *targetContainingArena = FindTargetOfSourceArena(&debugState->targetsStore, sourceContainingArena);
        const char *ourDebugID = GetDebugIDFromOurArena(debugState, debugID);
        ForceLastAllocationToArena(&targetContainingArena->allocations, &debugState->targetsStore, subArenaSource, &debugState->miscArena, ourDebugID, name);
    }
}

void DebugRecordPushSize_(const char *debugID, MemoryArena *source, siz requestedSize, siz actualSize)
{
    DebugState *debugState = GetGlobalDebugState();
    if (debugState->readyToRegularAllocate_)
    {
        DebugArena *targetArena = FindTargetOfSourceArena(&debugState->targetsStore, source);
        targetArena->used += actualSize;

        DebugGeneralAllocation *targetAllocation = NewDebugGeneralAllocation(&debugState->targetsStore);
        AddAllocation(&targetArena->allocations, targetAllocation);
        const char *ourDebugID = GetDebugIDFromOurArena(debugState, debugID);
        MapSourceAllocationToTarget(targetAllocation, requestedSize, actualSize, &debugState->miscArena, ourDebugID);
    }
}

void DebugRecordPopSize_(MemoryArena *source, siz size)
{
    // TODO(marvin): Implementation of DebugRecordPopSize_
    // DebugState *debugState = GetGlobalDebugState();
}

#endif
