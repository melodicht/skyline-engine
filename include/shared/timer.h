#include <meta_definitions.h>

inline u64 ReadCPUTimer(void)
{
    // If you were on ARM, you would need to replace __rdtsc with one of
    // their performance counter read instructions, depending on which
    // ones are available on your platform.
    return __rdtsc();
}
