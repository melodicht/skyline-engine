#ifdef SKL_INTERNAL
// Inspired from
// https://github.com/cmuratori/computer_enhance/blob/main/perfaware/part3/listing_0108_platform_metrics.cpp
#include <meta_definitions.h>
#include <timer.h>

// Finds the frequency of ticks per second on OS
static u64 GetOSTimerFreq(void);

// Finds the amount of ticks passed
static u64 ReadOSTimer(void);

// Estimates the frequency of ticks per second on CPU
static u64 EstimateCPUTimerFreq(void)
{
    u64 MillisecondsToWait = 100;
    u64 OSFreq = GetOSTimerFreq();

    u64 CPUStart = ReadCPUTimer();
    u64 OSStart = ReadOSTimer();
    u64 OSEnd = 0;
    u64 OSElapsed = 0;
    u64 OSWaitTime = OSFreq * MillisecondsToWait / 1000;
    while (OSElapsed < OSWaitTime)
    {
        OSEnd = ReadOSTimer();
        OSElapsed = OSEnd - OSStart;
    }

    u64 CPUEnd = ReadCPUTimer();
    u64 CPUElapsed = CPUEnd - CPUStart;

    u64 CPUFreq = 0;
    if (OSElapsed)
    {
        CPUFreq = OSFreq * CPUElapsed / OSElapsed;
    }

    return CPUFreq;
}

#if defined(PLATFORM_UNIX)
// Uses POSiX time library to get nanoseconds
#include <time.h>
static u64 GetOSTimerFreq(void)
{
    return 1000000000ULL;
}

static u64 ReadOSTimer(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
}

#elif defined(_WIN32)

#include <intrin.h>
#include <windows.h>

static u64 GetOSTimerFreq(void)
{
    LARGE_INTEGER Freq;
    QueryPerformanceFrequency(&Freq);
    return Freq.QuadPart;
}

static u64 ReadOSTimer(void)
{
    LARGE_INTEGER Value;
    QueryPerformanceCounter(&Value);
    return Value.QuadPart;
}

#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))

#include <x86intrin.h>
#include <sys/time.h>

static u64 GetOSTimerFreq(void)
{
	return 1000000;
}

static u64 ReadOSTimer(void)
{
	struct timeval Value;
	gettimeofday(&Value, 0);
	
	u64 Result = GetOSTimerFreq()*(u64)Value.tv_sec + (u64)Value.tv_usec;
	return Result;
}
#elif EMSCRIPTEN
#include <emscripten.h>
static u64 GetOSTimerFreq(void)
{
	return 1000000000ULL;
}

static u64 ReadOSTimer(void)
{
	return static_cast<u64>(emscripten_get_now() * 1000000ULL);
}
#else
#warning "Platform is unsupported by skyline engine timer"
static u64 GetOSTimerFreq(void)
{
	return 0;
}

static u64 ReadOSTimer(void)
{
	return 0;
}
#endif
#endif