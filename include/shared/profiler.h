#pragma once

// PROFILE_ZONE_BEGIN(handle, "name") starts a zone that can be ended early
// with PROFILE_ZONE_END(handle). It also ends automatically on scope exit.
// Use distinct handles in the same scope. End nested zones innermost-first,
// on the same thread where they began.
// PROFILE_FRAME_END() marks one application-frame boundary, not a zone end.

#if SKL_ENABLED_PROFILING

#include <meta_definitions.h>

#define PROFILE_LOG_STAGGER(memory) (++(memory).profilerState.staggerCount)
#define PROFILE_LOG_FRAME(memory) (++(memory).profilerState.frameCount)
#define PROFILE_OUTPUT_LOG(memory) LOG("Average stagger per frame " << ((f32)(memory).profilerState.staggerCount / (f32)(memory).profilerState.frameCount))

#if EMSCRIPTEN
#define PROFILE_INITIALIZE() ((void)0)
// The browser provides the application-frame timeline; this is a Tracy marker.
#define PROFILE_FRAME_END() ((void)0)
void WebZoneBegin(const char* n);
void WebZoneEnd(const char* n);
struct WebZone {
    const char* n;
    bool active{true};
    explicit WebZone(const char* n) : n(n) { WebZoneBegin(n); }
    ~WebZone() { End(); }

    void End() {
        if (active) {
            WebZoneEnd(n);
            active = false;
        }
    }

    WebZone(const WebZone& z) = delete;
    WebZone(WebZone&& z) = delete;
    WebZone& operator=(const WebZone& z) = delete;
    WebZone& operator=(WebZone&& z) = delete;
};

#define PROFILE_FRAMEMARK(name) WebZoneBegin(name)
#define PROFILE_FRAMEMARK_END(name) WebZoneEnd(name)
#define PROFILE_ZONE(name) WebZone _pz{name}
#define PROFILE_ZONE_BEGIN(handle, name) WebZone handle{name}
#define PROFILE_ZONE_END(handle) ((handle).End())
#else
#include <optional>
#include <tracy/Tracy.hpp>

class ProfileZone
{
public:
    ProfileZone(uint32_t line, const char* source, const char* function, const char* name)
        : zone(std::in_place, line, source, strlen(source), function, strlen(function),
               name, strlen(name), uint32_t{0}, TRACY_CALLSTACK, true)
    {
    }

    // Destroying the optional ends Tracy's zone exactly once. Leaving scope
    // without an explicit End() still closes it, including on early returns.
    void End() { zone.reset(); }

    ProfileZone(const ProfileZone&) = delete;
    ProfileZone(ProfileZone&&) = delete;
    ProfileZone& operator=(const ProfileZone&) = delete;
    ProfileZone& operator=(ProfileZone&&) = delete;

private:
    // The transient constructor copies metadata out of reloadable code and
    // retains Tracy's on-demand connection checks when the zone is ended.
    std::optional<tracy::ScopedZone> zone;
};

#define PROFILE_ZONE_BEGIN(handle, name) ProfileZone handle{TracyLine, TracyFile, TracyFunction, name}
#define PROFILE_ZONE_END(handle) ((handle).End())
#define PROFILE_INITIALIZE() ((void)tracy::GetProfiler())
#define PROFILE_FRAME_END() FrameMark
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
#define PROFILE_FRAME_END() ((void)0)
#define PROFILE_FRAMEMARK(name) ((void)0)
#define PROFILE_FRAMEMARK_END(name) ((void)0)
#define PROFILE_ZONE(name) ((void)0)
#define PROFILE_ZONE_BEGIN(handle, name) ((void)0)
#define PROFILE_ZONE_END(handle) ((void)0)

#define PROFILE_LOG_STAGGER(memory) ((void)0)
#define PROFILE_LOG_FRAME(memory) ((void)0)
#define PROFILE_OUTPUT_LOG(memory) ((void)0)

#endif
