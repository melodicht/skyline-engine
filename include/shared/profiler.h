#pragma once

#if SKL_ENABLED_PROFILING

#include <deque>
#include <array>
#include <chrono>
#include <meta_definitions.h>
#include <render_game.h>

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
    inline static f64 pollStartStamp{ 0 };
    inline static f64 pollEndStamp{ 0 };
    inline static std::deque<f64> pollStartTimestamp{ };
    inline static std::deque<f64> pollEndTimestamp{ };
    inline static std::deque<f64> sendToGpuTimestamp{ };
    // How many fixed steps each entry of sendToGpuTimestamp shows.
    inline static std::deque<u32> sendStepCount{ };
    // Fixed steps that have run since the last send to the GPU.
    inline static u32 stepsSinceSend{ 0 };
    inline static std::array<b8, 128> staggerWindow{ 0 };
    inline static u32 staggerCount{ 0 };
    inline static u32 staggerIter { 0 };

    inline static u32 movingWindow{ 60 };
    // Timestamps and average latencies are all in milliseconds.
    inline static f64 movingAverageStartLatency{ 0 };
    inline static f64 movingAverageEndLatency{ 0 };
    inline static f64 movingAverageMiddleLatency{ 0 };
    inline static f64 movingAverageGpuTime{ 0 };
    inline static b8 hasGpuTime{ false };
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

#define PROFILE_INPUT_POLLED() ProfilerState::pollEndStamp = ProfileGetCurrentTime()
static void finalizePollFrame() {
    // Later steps in the same frame have no new poll, so they push an invalid (0, 0) window.
    if (ProfilerState::pollEndStamp == 0) {
        ProfilerState::pollStartTimestamp.push_back(0);
        ProfilerState::pollEndTimestamp.push_back(0);
        return;
    }

    ProfilerState::pollStartTimestamp.push_back(ProfilerState::pollStartStamp);
    ProfilerState::pollEndTimestamp.push_back(ProfilerState::pollEndStamp);

    ProfilerState::pollStartStamp = ProfilerState::pollEndStamp;
    ProfilerState::pollEndStamp = 0;
}
#define PROFILE_INPUT_POLLED_FINISH() finalizePollFrame()
#define PROFILE_FIXED_STEP() ++ProfilerState::stepsSinceSend
static void profilerSentFrame() {
    ProfilerState::sendToGpuTimestamp.push_back(ProfileGetCurrentTime());
    ProfilerState::sendStepCount.push_back(ProfilerState::stepsSinceSend);
    ProfilerState::stepsSinceSend = 0;
}
#define PROFILE_SENT_FRAME() profilerSentFrame()

// Seeds the average with the first sample, then applies it over movingWindow.
static void profilerUpdateMovingAverage(f64& average, f64 sample) {
    if (average == 0) {
        average = sample;
        return;
    }

    average += (sample - average) / ProfilerState::movingWindow;
}

// Read completed frames before submitting new work; GPU results arrive in batches.
static void profilerUpdateGpuTime(const PlatformRenderer& renderer) {
    for (const auto& frame : renderer.FlushProfilingZones()) {
        for (siz zone = 0; zone < frame.zoneNames.size() && zone < frame.zoneTimes.size(); ++zone) {
            // Use the enclosing Frame zone so nested zones aren't counted twice.
            if (frame.zoneNames[zone] != "Frame") {
                continue;
            }
            f64 timeMs = static_cast<f64>(frame.zoneTimes[zone]) / 1000000.0;

            if (!ProfilerState::sendToGpuTimestamp.empty()) {
                f64 sendTime = ProfilerState::sendToGpuTimestamp.front() + timeMs;
                u32 stepCount = ProfilerState::sendStepCount.front();
                ProfilerState::sendToGpuTimestamp.pop_front();
                ProfilerState::sendStepCount.pop_front();

                // This trace shows all of its steps, so it uses up one window per step. Together
                // they span from the oldest window's start to the newest window's end.
                f64 pollStart = 0;
                f64 pollEnd = 0;
                for (u32 step = 0; step < stepCount && !ProfilerState::pollStartTimestamp.empty(); ++step) {
                    f64 windowStart = ProfilerState::pollStartTimestamp.front();
                    f64 windowEnd = ProfilerState::pollEndTimestamp.front();
                    ProfilerState::pollStartTimestamp.pop_front();
                    ProfilerState::pollEndTimestamp.pop_front();

                    // Skips (0, 0) windows, and the first sample, which has no previous poll
                    // to start its window from.
                    if (windowStart == 0) {
                        continue;
                    }
                    if (pollStart == 0) {
                        pollStart = windowStart;
                    }
                    pollEnd = windowEnd;
                }

                if (pollStart != 0) {
                    profilerUpdateMovingAverage(ProfilerState::movingAverageStartLatency, sendTime - pollStart);
                    profilerUpdateMovingAverage(ProfilerState::movingAverageMiddleLatency, sendTime - (pollEnd + pollStart) / 2);
                    profilerUpdateMovingAverage(ProfilerState::movingAverageEndLatency, sendTime - pollEnd);
                }
            }

            if (!ProfilerState::hasGpuTime) {
                ProfilerState::movingAverageGpuTime = timeMs;
                ProfilerState::hasGpuTime = true;
            }
            else {
                ProfilerState::movingAverageGpuTime +=
                    (timeMs - ProfilerState::movingAverageGpuTime) / ProfilerState::movingWindow;
            }
            break;
        }
    }
}
#define PROFILE_GPU_FRAME(renderer) profilerUpdateGpuTime(renderer)

static void profilerLogState() {
    LOG("Staggered frames in the last " << ProfilerState::staggerWindow.size() << " frames: " << ProfilerState::staggerCount);
    LOG("Moving average of start latency " << ProfilerState::movingAverageStartLatency << " ms");
    LOG("Moving average of middle latency " << ProfilerState::movingAverageMiddleLatency << " ms");
    LOG("Moving average of end latency " << ProfilerState::movingAverageEndLatency << " ms");
    if (ProfilerState::hasGpuTime) {
        LOG("Moving average of GPU processing time " << ProfilerState::movingAverageGpuTime << " ms");
    }
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
#define PROFILE_INPUT_POLLED_FINISH() ((void)0)
#define PROFILE_FIXED_STEP() ((void)0)
#define PROFILE_SENT_FRAME() ((void)0)
#define PROFILE_GPU_FRAME(renderer) ((void)0)

#define PROFILE_OUTPUT_LOG() ((void)0)

#endif
