#include <SDL3/SDL_stdinc.h>

#include <meta_definitions.h>

// Inclusive min, exclusive max.
f32 RandF32InRange(f32 min, f32 max)
{
    // Doesn't include 1.0f.
    f32 fraction = SDL_randf();
    f32 result = min + ((max - min) * fraction);
    return result;
}
