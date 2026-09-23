#pragma once

#if SKL_ENABLED_PROFILING

#include <meta_definitions.h>

#define PROFILE_LOG_STAGGER(memory) (++(memory).profilerState.staggerCount)
#define PROFILE_LOG_FRAME(memory) (++(memory).profilerState.frameCount)
#define PROFILE_OUTPUT_LOG(memory) LOG("Average stagger per frame " << ((f32)(memory).profilerState.staggerCount / (f32)(memory).profilerState.frameCount))

#if EMSCRIPTEN
#define PROFILE_INITIALIZE() ((void)0)
void WebZoneBegin(const char* n);
void WebZoneEnd(const char* n);
struct WebZone {
    const char* n;
    explicit WebZone(const char* n) : n(n) { WebZoneBegin(n); }
    ~WebZone() { WebZoneEnd(n); }

    WebZone(const WebZone& z) = delete;
    WebZone(WebZone&& z) = delete;
    WebZone& operator=(const WebZone& z) = delete;
    WebZone& operator=(WebZone&& z) = delete;
};

#define PROFILE_FRAMEMARK(name) WebZoneBegin(name)
#define PROFILE_FRAMEMARK_END(name) WebZoneEnd(name)
#define PROFILE_ZONE(name) WebZone _pz{name}
#else
#include <tracy/Tracy.hpp>
#define PROFILE_INITIALIZE() ((void)tracy::GetProfiler())
// Named frame strings must outlive every reload: emit these from the platform,
// not from the unloadable game module. Tracy does not copy frame names.
#define PROFILE_FRAMEMARK(name) FrameMarkStart(name)
#define PROFILE_FRAMEMARK_END(name) FrameMarkEnd(name)
#if !SKL_STATIC_MONOLITHIC
// Copy source/name strings before the module containing them can be unloaded.
#define PROFILE_ZONE(name) ZoneTransientN(TracyConcat(_sklProfileZone, __LINE__), name, true)
#else
#define PROFILE_ZONE(name) ZoneScopedN(name)
#endif
#endif

#else

#define PROFILE_INITIALIZE() ((void)0)
#define PROFILE_FRAMEMARK(name) ((void)0)
#define PROFILE_FRAMEMARK_END(name) ((void)0)
#define PROFILE_ZONE(name) ((void)0)

#define PROFILE_LOG_STAGGER(memory) ((void)0)
#define PROFILE_LOG_FRAME(memory) ((void)0)
#define PROFILE_OUTPUT_LOG(memory) ((void)0)

#endif
