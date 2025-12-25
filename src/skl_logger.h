#pragma once

#if SKL_LOGGING_ENABLED
    #include <iostream>
    #include <cassert>

    #define LOG(x) std::cout << x << std::endl
    #define LOG_ERROR(x) std::cerr << x << std::endl

#else
    #define LOG(x) (void)(0)
    #define LOG_ERROR(x) (void)(0)
#endif