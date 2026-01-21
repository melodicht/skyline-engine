#pragma once

#include <meta_definitions.h>

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

// TODO(marvin): Could generalize free indices stack to u32 stack, which also could be a generalized to any stack.
struct FreeIndicesStack
{
    MemoryArena arena;
};
