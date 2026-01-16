#pragma once

#ifndef _WIN32
#include <x86intrin.h>
#endif

#include <meta_definitions.h>

inline u64 ReadCPUTimer(void)
{
    #if defined(_WIN32) || defined(__x86_64__) || defined(__i386__)
    return __rdtsc();
    #elif __ARM_ARCH_ISA_A64
    uint64_t cntvct;
    asm volatile ("mrs %0, cntvct_el0; " : "=r"(cntvct) :: "memory");
    return cntvct;
    #else 
        #error "Timer unsupported"
    #endif
}
