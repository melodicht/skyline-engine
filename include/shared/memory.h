#pragma once

#include <cstring>

#include <memory_types.h>
#include <debug.h>

#if SKL_INTERNAL
#define INTERNAL_MEMORY_PARAM const char *internalDebugID, 
#define INTERNAL_MEMORY_PASS  internalDebugID,
#define INTERNAL_MEMORY_PASS_NAME  internalDebugID, name,

// NOTE(marvin): The reason for aditional expansion is to give the `b`
// macro parameter a chance to expand before getting stringified.
#define MAKE_DEBUG_ID__(a, b) a "|" #b
#define MAKE_DEBUG_ID_(a, b) MAKE_DEBUG_ID__(a, b)
#define MAKE_DEBUG_ID MAKE_DEBUG_ID_(__FILE__, __LINE__)
#define MAKE_DEBUG_ID_COMMA MAKE_DEBUG_ID,

#else

#define INTERNAL_MEMORY_PARAM
#define INTERNAL_MEMORY_PASS

#define MAKE_DEBUG_ID__(...)
#define MAKE_DEBUG_ID_(...)
#define MAKE_DEBUG_ID
#define MAKE_DEBUG_ID_COMMA

#endif
inline ArenaParams NoClearArenaParams()
{
    ArenaParams result = {};
    result.alignment = 4;
    return result;
}

inline ArenaParams DefaultArenaParams()
{
    ArenaParams result = NoClearArenaParams();
    result.flags = clear_to_zero;
    return result;
}

inline void ZeroSize(void *base, siz size)
{
    u8 *cursor = static_cast<u8 *>(base);
    while(size--)
    {
        *cursor++ = 0;
    }
}

#define InitMemoryArena(...) InitMemoryArena_(MAKE_DEBUG_ID_COMMA __VA_ARGS__)
#define SubArena(...) SubArena_(MAKE_DEBUG_ID_COMMA __VA_ARGS__)

#define PushPrimitive(arena, T, ...) ((T *) PushSize_(MAKE_DEBUG_ID_COMMA (arena), sizeof(T), ## __VA_ARGS__))
#define PushArray(arena, count, T, ...) ((T *) PushSize_(MAKE_DEBUG_ID_COMMA (arena), (count)*sizeof(T), ## __VA_ARGS__))
#define PushStruct(arena, T, ...) ((T *) PushSize_(MAKE_DEBUG_ID_COMMA (arena), sizeof(T), ## __VA_ARGS__))
#define PushString(arena, source) (PushString_(MAKE_DEBUG_ID_COMMA (arena), (source)))
#define PushSize(arena, size, ...) (PushSize_(MAKE_DEBUG_ID_COMMA (arena), size, ## __VA_ARGS__))

// TODO(marvin): Have PopArray and PopStruct for completeness.

#define PopPrimitive(arena, T) \
    ({ T *address = ((T *) PopSize((arena), sizeof(T), NoClearArenaParams())); \
       T result = *address; \
       ZeroSize(address, sizeof(T)); \
       result; })
      
#define PopSize(arena, size, ...) (PopSize_((arena), size, ## __VA_ARGS__))

inline siz GetAlignmentOffset(MemoryArena *arena, siz alignment)
{
    siz result = 0;

    if (alignment > 0)
    {
        u32 alignmentMask = alignment - 1;

        ASSERT_PRINT(((alignment & alignmentMask) == 0),
               "Alignment must be a power of 2 due to C++ specifications.");

        siz targetAddress = (siz)arena->base + arena->used;
        siz masked = targetAddress & alignmentMask;
        if (masked != 0)
        {
            result = alignment - masked;
        }
    }
    
    return result;
}

inline void *PushSize_(INTERNAL_MEMORY_PARAM
                       MemoryArena *arena, siz requestedSize, ArenaParams params = DefaultArenaParams())
{
    siz alignmentOffset = GetAlignmentOffset(arena, params.alignment);
    void *result = arena->base + arena->used + alignmentOffset;
    siz effectiveSize = requestedSize + alignmentOffset;
    ASSERT(effectiveSize >= requestedSize);
    arena->used += effectiveSize;
    ASSERT(arena->used <= arena->size);

    if (params.flags & clear_to_zero)
    {
        ZeroSize(result, requestedSize);
    }

    DebugRecordPushSize(INTERNAL_MEMORY_PASS arena, requestedSize, effectiveSize);
    return result;
}

inline void *PushCopy_(INTERNAL_MEMORY_PARAM
                       MemoryArena *arena, const char *source, siz sourceSize)
{
    // NOTE(marvin): The +1 is for the null. We could in the future record the string's length instead.
    void *result = PushSize_(INTERNAL_MEMORY_PASS arena, sourceSize + 1);
    char *destination = static_cast<char *>(result);
    strncpy(destination, source, sourceSize + 1);
    return result;
}

inline char *PushString_(INTERNAL_MEMORY_PARAM
                         MemoryArena *arena, const char *source)
{
    siz sourceSize = strlen(source);
    void *rawPointer = PushCopy_(INTERNAL_MEMORY_PASS arena, source, sourceSize);
    char *result = static_cast<char *>(rawPointer);
    return result;
}

// Produces the address to what just got popped. If a sub arena has
// been allocated, followed by normal allocations, you could but
// shouldn't pop into the sub arena.
// TODO(marvin): Should the design of the memor data definitions prevent popping into child sub arena?
// NOTE(marvin): Pop doesn't have to do anything about alignment, right...?
inline void *PopSize_(MemoryArena *arena, siz size, ArenaParams params = DefaultArenaParams())
{
    arena->used -= size;
    ASSERT(arena->used >= 0);

    void *result = arena->base + arena->used;
    DebugRecordPopSize(arena, size);
    return result;
}

inline MemoryArena InitMemoryArena_(INTERNAL_MEMORY_PARAM
                                    void *base, siz size,
                                    const char *name = "(unnamed)")
{
    MemoryArena result = {};
    result.size = size;
    result.base = static_cast<u8 *>(base);
    result.used = 0;
    DebugRecordInitMemoryArena(INTERNAL_MEMORY_PASS_NAME result);
    return result;
}

inline b32 ArenaIsEmpty(MemoryArena *arena)
{
    b32 result = (arena->used == 0);
    return result;
}

// Cuts a new memory arena from the given memory arena.
inline MemoryArena SubArena_(INTERNAL_MEMORY_PARAM
                             MemoryArena *arena, siz size,
                             const char *name = "(unnamed)",
                             ArenaParams params = DefaultArenaParams())
{
    MemoryArena result = {};
    result.size = size;
    result.base = static_cast<u8 *>(PushSize_(INTERNAL_MEMORY_PASS arena, size, params));
    result.used = 0;
    DebugRecordSubArena(INTERNAL_MEMORY_PASS_NAME arena, result);
    return result;
}

inline FreeIndicesStack InitFreeIndicesStack(MemoryArena *remainingArena, u32 count)
{
    FreeIndicesStack result = {};
    result.arena = SubArena(remainingArena, count * sizeof(u32), "Free Indices Stack");
    return result;
}

inline void PushFreeIndicesStack(FreeIndicesStack *stack, u32 newIndex)
{
    u32 *index = PushPrimitive(&stack->arena, u32);
    *index = newIndex;
}

inline u32 PopFreeIndicesStack(FreeIndicesStack *stack)
{
    u32 result = PopPrimitive(&stack->arena, u32);
    return result;
}

inline b32 FreeIndicesStackIsEmpty(FreeIndicesStack *stack)
{
    b32 result = ArenaIsEmpty(&stack->arena);
    return result;
}
