#include "profiler.h"

#if EMSCRIPTEN && SKL_ENABLED_PROFILING
#include <emscripten.h>

extern "C" {
    EM_JS(void, web_zone_begin, (const char* n), { performance.mark(UTF8ToString(n)); });
    EM_JS(void, web_zone_end, (const char* n), {
    const s = UTF8ToString(n); performance.measure(s, s);
    });
}

void WebZoneBegin(const char* n) { web_zone_begin(n); }
void WebZoneEnd(const char* n) { web_zone_end(n); }
#endif