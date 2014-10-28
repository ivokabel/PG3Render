#pragma once

void pg3_exit();

#define PG3_FATAL_ERROR( __message, ... ) \
    for(;;) \
    { \
        fprintf( \
            stderr, \
            "\n\n" \
            "Error:     " \
            __message, __VA_ARGS__ \
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

#define PG3_ASSERTS_ENABLED

#ifdef PG3_ASSERTS_ENABLED
#define PG3_ASSERT(__expr) \
    for(;;) \
    { \
        if(!(__expr)) \
        { \
            std::cerr << "Assertion '" #__expr "' failed at " << __FILE__ << ":" << __LINE__ << \
                std::endl << std::flush; \
            pg3_exit(); \
        } \
        break; \
    }
#else
#define PG3_ASSERT
#endif

#ifdef _DEBUG
#define PG3_DEBUG_ASSERT    PG3_ASSERT
#else
#define PG3_DEBUG_ASSERT
#endif

#define PG3_DEBUG_ASSERT_VAL_IN_RANGE(_val, _low, _up) \
    PG3_DEBUG_ASSERT(((_val) >= (_low)) && ((_val) <= (_up)))
