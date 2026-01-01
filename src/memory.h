#pragma once

struct MemoryArena
{
    siz size;
  
    u08 *base;
    siz used;
};

enum ArenaFlag
{
    clear_to_zero = 0b1,
};

typedef u32 ArenaFlags;

struct ArenaParams
{
    ArenaFlags flags;
    u32 alignment;  // In bytes
};

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

#define PushPrimitive(arena, T, ...) ((T *) PushSize_((arena), sizeof(T), ## __VA_ARGS__))
#define PushArray(arena, count, T, ...) ((T *) PushSize_((arena), (count)*sizeof(T), ## __VA_ARGS__))
#define PushStruct(arena, T, ...) ((T *) PushSize_((arena), sizeof(T), ## __VA_ARGS__))
#define PushSize(arena, size, ...) (PushSize_((arena), size, ## __VA_ARGS__))

// TODO(marvin): Have PopArray and PopStruct for completeness.

#define PopPrimitive(arena, T) \
    ({ T *address = ((T *) PopSize_((arena), sizeof(T), NoClearArenaParams())); \
       T result = *address; \
       ZeroSize(address, sizeof(T)); \
       result; })
      
#define PopSize(arena, size, ...) (PopSize_((arena), size, ## __VA_ARGS__))

inline siz GetAlignmentOffset(MemoryArena *arena, siz alignment)
{
    siz result = 0;

    u32 alignmentMask = alignment - 1;

    Assert(((alignment & alignmentMask) == 0) &&
           "Alignment must be a power of 2 due to C++ specifications.");

    siz targetAddress = (siz)arena->base + arena->used;
    siz masked = targetAddress & alignmentMask;
    if (masked != 0)
    {
        result = alignment - masked;
    }

    return result;
}

inline void *PushSize_(MemoryArena *arena, siz requestedSize, ArenaParams params = DefaultArenaParams())
{
    siz alignmentOffset = GetAlignmentOffset(arena, params.alignment);
    void *result = arena->base + arena->used + alignmentOffset;
    siz effectiveSize = requestedSize + alignmentOffset;
    Assert(effectiveSize >= requestedSize);
    arena->used += effectiveSize;
    Assert(arena->used <= arena->size)

    if (params.flags & clear_to_zero)
    {
        ZeroSize(result, requestedSize);
    }
  
    return result;
}

// Produces the address to what just got popped.
// NOTE(marvin): Pop doesn't have to do anything about alignment, right...?
inline void *PopSize_(MemoryArena *arena, siz size, ArenaParams params = DefaultArenaParams())
{
    arena->used -= size;
    Assert(arena->used >= 0);

    void *result = arena->base + arena->used;
    return result;
}

inline MemoryArena InitMemoryArena(void *base, siz size)
{
    MemoryArena result = {};
    result.size = size;
    result.base = static_cast<u8 *>(base);
    result.used = 0;
    return result;
}

inline b32 ArenaIsEmpty(MemoryArena *arena)
{
    b32 result = (arena->used == 0);
    return result;
}

// Cuts a new memory arena from the given memory arena.
inline MemoryArena SubArena(MemoryArena *arena, siz size, ArenaParams params = DefaultArenaParams())
{
    MemoryArena result = {};
    result.size = size;
    result.base = static_cast<u8 *>(PushSize_(arena, size, params));
    result.used = 0;
    return result;
}
