#pragma once

// SKL_SLOW
// 0 - No slow code allowed
// 1 - Slow code allowed

// SKL_INTERNAL
// 0 - For release
// 1 - Not for release

// Project Fixed Width Numerical Types
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

typedef s32 b32;

// Project Keywords
#define global_variable static
#define local static
#define local_persist static

// Logging
#if SKL_LOGGING_ENABLED

#include <iostream>
#include <cassert>

#define LOG(x) std::cout << x << std::endl
#define LOG_ERROR(x) std::cerr << x << std::endl

#else

#define LOG(x) (void)(0)
#define LOG_ERROR(x) (void)(0)

#endif

// Misc

// Assumes that the Expression evaluates to an array.
#define ArrayCount(Expression) (sizeof(Expression)/sizeof((Expression)[0]))

#if SKL_SLOW

#if defined(_MSC_VER)
#define DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define DEBUG_BREAK() __builtin_trap()
#else
#define DEBUG_BREAK() (*(volatile int*)0 = 0)
#endif

#define Assert(Expression) if(!(Expression)) { DEBUG_BREAK(); }

#else
#define Assert(Expression)
#endif

// Streamlines process of passing platform funcs to game module api.

// Helper macros
#define SKL_AS_FIELD(r, n, p) r (* n) p;
#ifndef SKL_GAME_MODULE
#define SKL_AS_HEADER_FUNC(r, n, p) r n p;
#define SKL_AS_CONSTRUCT_INSERT(r, n, p) ret.n = n;
#define SKL_AS_CONSTRUCT_FUNC(name, methods)\
    inline name construct##name() {\
        name ret{};\
        methods(SKL_AS_CONSTRUCT_INSERT)\
        return ret;\
    }
#else
#define SKL_AS_HEADER_FUNC(r, n, p);
#define SKL_AS_CONSTRUCT_INSERT(r, n, p);
#define SKL_AS_CONSTRUCT_FUNC(name, methods);
#endif

// Allows for funcs to be passed into the game module as a part of an api struct 
// while only being declared once
// @param name     the name of the api struct constructed by the macro, 
//                 if not in game module, use construct<name>() method to create instance of struct with declared methods.
// @param methods  defines which methods that will be in the api struct constructed by macro, 
//                 if not in game module, it also declares these functions.
#define DEFINE_GAME_MODULE_API(name, methods) \
    methods(SKL_AS_HEADER_FUNC) \
    struct name { \
        methods(SKL_AS_FIELD) \
    };\
    \
    SKL_AS_CONSTRUCT_FUNC(name, methods)
