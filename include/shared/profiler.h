#pragma once

#ifndef SKL_ENABLED_PROFILING

#define SKL_PROFILE_FRAMEMARK(name) ((void)0)
#define SKL_PROFILE_FRAMEMARK_END(name) ((void)0)
#define SKL_PROFILE_ZONE(name) ((void)0)

#elif EMSCRIPTEN
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

#define SKL_PROFILE_FRAMEMARK(name) WebZoneBegin(name)
#define SKL_PROFILE_FRAMEMARK_END(name) WebZoneEnd(name)
#define SKL_PROFILE_ZONE(name) WebZone _pz{name}
#else
#include <tracy/Tracy.hpp>
#define SKL_PROFILE_FRAMEMARK(name) FrameMarkStart(name)
#define SKL_PROFILE_FRAMEMARK_END(name) FrameMarkEnd(name)
#define SKL_PROFILE_ZONE(name) ZoneScopedN(name)
#endif