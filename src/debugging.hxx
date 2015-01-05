#pragma once

#include <iostream>

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
    }

// Uncomment this to activate all asserts in the code
//#define PG3_ASSERT_ENABLED

#ifdef PG3_ASSERT_ENABLED
    #define PG3_ASSERT(__expr) \
        for(;;) \
            { \
            if(!(__expr)) \
                    { \
                std::cerr << std::endl << \
                    "Assertion\n\t'" #__expr "'\nfailed at " << __FILE__ << ":" << __LINE__ << \
                    std::endl << std::flush; \
                pg3_exit(); \
                    } \
            break; \
            }
    #include <cmath>
#else
    #define PG3_ASSERT
#endif


#define PG3_ASSERT_VALID_FLOAT(_val) \
    PG3_ASSERT(!std::isnan(_val))

#define PG3_ASSERT_VAL_NONNEGATIVE(_val) \
    PG3_ASSERT((_val) >= 0)

#define PG3_ASSERT_VAL_IN_RANGE(_val, _low, _up) \
    PG3_ASSERT(((_val) >= (_low)) && ((_val) <= (_up)))

#define PG3_ASSERT_FLOAT_EQUAL(_val1, _val2, _radius) \
    PG3_ASSERT(fabs((_val1) - (_val2)) <= (_radius))

#define PG3_ASSERT_FLOAT_LESS_THAN(_val1, _val2) \
    PG3_ASSERT((_val1) < (_val2))

// noinline for profiling purposes - it helps to better visualise a low-level code in the profiler
//#define PG3_USE_PROFILING_NOINLINE
#ifdef PG3_USE_PROFILING_NOINLINE
#define PG3_PROFILING_NOINLINE __declspec(noinline)
#else
#define PG3_PROFILING_NOINLINE
#endif