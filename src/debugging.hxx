#pragma once

#include "hardsettings.hxx"

#include <iostream>

void init_debugging();

#ifndef RUN_UNIT_TESTS_INSTEAD_OF_RENDERER
// Uncomment this to activate all asserts in the code
//#define PG3_ASSERT_ENABLED
#endif

void pg3_exit();

#define PG3_WARNING( _message, ... ) \
    for(;;) \
    { \
        fprintf( \
            stderr, \
            "\n\n" \
            "Warning:     " \
            _message, __VA_ARGS__ \
            ); \
        \
        fprintf( \
            stderr, \
            "\n" \
            "Location:  " __FILE__ " line %d\n\n", \
            __LINE__ \
            ); \
        \
        fflush(stderr); \
        break; \
        }

#define PG3_FATAL_ERROR( _message, ... ) \
    for(;;) \
    { \
        fprintf( \
            stderr, \
            "\n\n" \
            "Error:     " \
            _message, __VA_ARGS__ \
            ); \
        \
        fprintf( \
            stderr, \
            "\n" \
            "Location:  " __FILE__ " line %d\n\n", \
            __LINE__ \
            ); \
        \
        fflush(stderr); \
        \
        pg3_exit(); \
        \
        break; \
    }

#ifdef PG3_ASSERT_ENABLED

    #define PG3_ASSERT(__expr) \
        for(;;) \
        { \
            if(!(__expr)) \
            { \
                std::cerr << std::endl << std::endl << \
                    "Assertion\n\t'" #__expr "'\nfailed at " << __FILE__ << ":" << __LINE__ << \
                    std::endl << std::flush; \
                pg3_exit(); \
            } \
            break; \
        }

    #define PG3_ASSERT_MSG(__expr, __msg, ...) \
        for(;;) \
        { \
            if(!(__expr)) \
            { \
                fprintf( \
                    stderr, \
                    "\n\n" \
                    "Assertion\n\t'" #__expr "'\nfailed at %s:%d\n" \
                    "Detail: " __msg \
                    "\n" \
                    , __FILE__, __LINE__, __VA_ARGS__ \
                    ); \
                fflush(stderr); \
                pg3_exit(); \
            } \
            break; \
        }

    #include <cmath>

#else

    #define PG3_ASSERT(...)
    #define PG3_ASSERT_MSG(...)

#endif


// Float asserts

#define PG3_ASSERT_FLOAT_VALID(_val) \
    PG3_ASSERT(!std::isnan(_val)); \
    PG3_ASSERT( std::isfinite(_val))

#define PG3_ASSERT_FLOAT_NONNEGATIVE(_val) \
    PG3_ASSERT_FLOAT_VALID(_val); \
    PG3_ASSERT_MSG(((_val) >= 0.0f), "%.12f >= 0.0f", (_val))

#define PG3_ASSERT_FLOAT_POSITIVE(_val) \
    PG3_ASSERT_FLOAT_VALID(_val); \
    PG3_ASSERT_MSG(((_val) > 0.0f), "%.12f > 0.0f", (_val))

#define PG3_ASSERT_FLOAT_IN_RANGE(_val, _low, _up) \
    PG3_ASSERT_FLOAT_VALID(_val); \
    PG3_ASSERT_FLOAT_VALID(_low); \
    PG3_ASSERT_FLOAT_VALID(_up); \
    PG3_ASSERT_MSG(((_val) >= (_low)) && ((_val) <= (_up)), "%.12f <= %.12f <= %.12f", (_low), (_val), (_up))

#ifdef PG3_ASSERT_ENABLED
#define PG3_ASSERT_FLOAT_EQUAL(_val1, _val2, _radius) \
{ \
    const auto _val1_const      = (_val1); \
    const auto _val2_const      = (_val2); \
    const auto _radius_const    = (_radius); \
    \
    PG3_ASSERT_FLOAT_VALID(_val1_const); \
    PG3_ASSERT_FLOAT_VALID(_val2_const); \
    PG3_ASSERT_FLOAT_VALID(_radius_const); \
    PG3_ASSERT_MSG( \
        fabs((_val1_const) - (_val2_const)) <= (_radius_const), \
        "fabs((%.12f) - (%.12f)) <= (%.12f)", \
        (_val1_const), (_val2_const), (_radius_const)) \
}
#else
#define PG3_ASSERT_FLOAT_EQUAL(_val1, _val2, _radius)
#endif

#define PG3_ASSERT_FLOAT_LESS_THAN(_val1, _val2) \
    PG3_ASSERT_FLOAT_VALID(_val1); \
    PG3_ASSERT_FLOAT_VALID(_val2); \
    PG3_ASSERT_MSG((_val1) < (_val2), "%.12f < %.12f", (_val1), (_val2))

#define PG3_ASSERT_FLOAT_LARGER_THAN(_val1, _val2) \
    PG3_ASSERT_FLOAT_VALID(_val1); \
    PG3_ASSERT_FLOAT_VALID(_val2); \
    PG3_ASSERT_MSG((_val1) > (_val2), "%.12f > %.12f", (_val1), (_val2))

#define PG3_ASSERT_FLOAT_LESS_THAN_OR_EQUAL_TO(_val1, _val2) \
    PG3_ASSERT_FLOAT_VALID(_val1); \
    PG3_ASSERT_FLOAT_VALID(_val2); \
    PG3_ASSERT_MSG((_val1) <= (_val2), "%.12f <= %.12f", (_val1), (_val2))

#define PG3_ASSERT_FLOAT_LARGER_THAN_OR_EQUAL_TO(_val1, _val2) \
    PG3_ASSERT_FLOAT_VALID(_val1); \
    PG3_ASSERT_FLOAT_VALID(_val2); \
    PG3_ASSERT_MSG((_val1) >= (_val2), "%.12f >= %.12f", (_val1), (_val2))

// Integer asserts

#define PG3_ASSERT_INTEGER_NONNEGATIVE(_val) \
    PG3_ASSERT((_val) >= 0)

#define PG3_ASSERT_INTEGER_POSITIVE(_val) \
    PG3_ASSERT((_val) > 0)

#define PG3_ASSERT_INTEGER_IN_RANGE(_val, _low, _up) \
    PG3_ASSERT_MSG(((_val) >= (_low)) && ((_val) <= (_up)), "%d <= %d <= %d", (_low), (_val), (_up))

#define PG3_ASSERT_INTEGER_LESS_THAN(_val1, _val2) \
    PG3_ASSERT_MSG((_val1) < (_val2), "%d < %d", (_val1), (_val2))

#define PG3_ASSERT_INTEGER_LARGER_THAN(_val1, _val2) \
    PG3_ASSERT_MSG((_val1) > (_val2), "%d > %d", (_val1), (_val2))

#define PG3_ASSERT_INTEGER_LESS_THAN_OR_EQUAL_TO(_val1, _val2) \
    PG3_ASSERT_MSG((_val1) <= (_val2), "%d <= %d", (_val1), (_val2))

#define PG3_ASSERT_INTEGER_LARGER_THAN_OR_EQUAL_TO(_val1, _val2) \
    PG3_ASSERT_MSG((_val1) >= (_val2), "%d >= %d", (_val1), (_val2))

// Vector2 asserts

#define PG3_ASSERT_VEC2F_VALID(_vec2) \
    PG3_ASSERT_FLOAT_VALID(_vec2.x); \
    PG3_ASSERT_FLOAT_VALID(_vec2.y);

// Vector3 asserts

#define PG3_ASSERT_VEC3F_VALID(_vec3) \
    PG3_ASSERT_FLOAT_VALID(_vec3.x); \
    PG3_ASSERT_FLOAT_VALID(_vec3.y); \
    PG3_ASSERT_FLOAT_VALID(_vec3.z);

#define PG3_ASSERT_VEC3F_NORMALIZED(_vec3) \
    PG3_ASSERT_VEC3F_VALID(_vec3); \
    PG3_ASSERT_FLOAT_EQUAL((_vec3).LenSqr(), 1.0f, 0.0005f)

// noinline for profiling purposes - it helps to better visualise low-level code in the profiler
//#define PG3_USE_PROFILING_NOINLINE
#ifdef PG3_USE_PROFILING_NOINLINE
#define PG3_PROFILING_NOINLINE __declspec(noinline)
#else
#define PG3_PROFILING_NOINLINE
#endif

// Code markers
#define PG3_ERROR_NOT_IMPLEMENTED(_msg) \
    PG3_FATAL_ERROR( \
        "This code has not been implemented!\n" \
        "Details:   %s", _msg);
#define PG3_ERROR_CODE_NOT_TESTED(_msg) \
    PG3_FATAL_ERROR( \
        "This code has not been tested!\n" \
        "Details:   %s", _msg);

