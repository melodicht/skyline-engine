#pragma once

#include <meta_definitions.h>

// Attempts to read machine specific cpu time
#if defined(_WIN32) || defined(__x86_64__) 

#include <x86intrin.h>

inline u64 ReadCPUTimer(void)
{
    return __rdtsc();
}
#elif __ARM_ARCH_ISA_A64

inline u64 ReadCPUTimer(void)
{
    uint64_t cntvct;
    asm volatile ("mrs %0, cntvct_el0; " : "=r"(cntvct) :: "memory");
    return cntvct;
}
#else 
#if !defined(EMSCRIPTEN)
    #warning CPU Timer not implemented for specific machine
#endif 

// This being hit means that the CPU Timer can't really be accessed for whatever reason
inline u64 ReadCPUTimer(void)
{
    return 0;
}

#endif