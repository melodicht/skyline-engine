#pragma once

#if SKL_ENABLED_PROFILING

#include <array>
#include <chrono>
#include <meta_definitions.h>

// Gets current monotonic time in milliseconds.
inline f64 ProfileGetCurrentTime()
{
    return std::chrono::duration<f64, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Inline static members share storage across translation units. These stats
// belong to the module using them, not persistent game memory; hot reload may
// reset them. Update them only from the main thread.
struct ProfilerState
{
    inline static std::array<b8, 128> staggerWindow{ 0 };
    inline static u32 staggerCount{ 0 };
    inline static u32 staggerIter { 0 };

    inline static u32 movingWindow{ 60 };
    // Timestamp and average latency are both in milliseconds.
    inline static f64 startTime{ 0 };
    inline static f64 movingAverageLatency{ 0 };
};


static void profilerLogStagger() {
    auto& staggered = ProfilerState::staggerWindow[ProfilerState::staggerIter];
    if (!staggered) {
        staggered = true;
        ++ProfilerState::staggerCount;
    }
}
// Call once at the start of each frame, before recording any stagger.
static void profilerLogFrame() {
    ProfilerState::staggerIter = (1 + ProfilerState::staggerIter) % ProfilerState::staggerWindow.size();
    auto& staggered = ProfilerState::staggerWindow[ProfilerState::staggerIter];
    if (staggered) {
        --ProfilerState::staggerCount;
        staggered = false;
    }
}
#define PROFILE_LOG_STAGGER() profilerLogStagger()
#define PROFILE_LOG_FRAME() profilerLogFrame()

#define PROFILE_INPUT_POLLED() ProfilerState::startTime = ProfileGetCurrentTime()
static void profilerUpdateLatency(f64 extraTimeMs = 0.0) {
    f64 latency = (ProfileGetCurrentTime() - ProfilerState::startTime) + extraTimeMs;

    if (ProfilerState::movingAverageLatency == 0) {
        ProfilerState::movingAverageLatency = latency;
        return;
    }

    f64 mainRatio = (f64)1 / (f64)ProfilerState::movingWindow;
    f64 beforeRatio = (f64)(ProfilerState::movingWindow - 1) / (f64)ProfilerState::movingWindow;
    ProfilerState::movingAverageLatency = ProfilerState::movingAverageLatency * beforeRatio + latency * mainRatio;
}
#define PROFILE_SENT_FRAME(...) profilerUpdateLatency(__VA_ARGS__)

static void profilerLogState() {
    LOG("Staggered frames in the last " << ProfilerState::staggerWindow.size() << " frames: " << ProfilerState::staggerCount);
    LOG("Moving average of latency " << ProfilerState::movingAverageLatency << " ms");
}

#define PROFILE_OUTPUT_LOG() profilerLogState()

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

#define PROFILE_LOG_STAGGER() ((void)0)
#define PROFILE_LOG_FRAME() ((void)0)

#define PROFILE_INPUT_POLLED() ((void)0)
#define PROFILE_SENT_FRAME(...) ((void)0)

#define PROFILE_OUTPUT_LOG() ((void)0)

#endif
