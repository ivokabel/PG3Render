#pragma once

#include "hard_config.hxx"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Unit testing
// Switched on by defining PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER in hard-wired settings
///////////////////////////////////////////////////////////////////////////////////////////////////

enum UnitTestBlockLevel
{
    eutblNone           = 0,
    eutblWholeTest      = 1,    // Main testing block
    eutblSubTestLevel1  = 2,    // Test can contain a hierarchy of sub-blocks...
    eutblSubTestLevel2  = 3,
};


#define PG3_UT_BEGIN( \
    _max_print_ut_level, \
    _ut_block_level, \
    _block_name, \
    ...) \
{ \
    if ((_max_print_ut_level) > eutblNone) \
    { \
        if ((_ut_block_level) <= (_max_print_ut_level)) \
        { \
            for (uint32_t level = (uint32_t)eutblWholeTest; level < (uint32_t)_ut_block_level; level++) \
                printf("\t"); \
            printf("Test \"" _block_name "\": ", __VA_ARGS__); \
        } \
        if ((_ut_block_level) < (_max_print_ut_level)) \
            printf("\n"); \
    } \
}


// Call only after PG3_UT_BEGIN
#define PG3_UT_INFO( \
    _max_print_ut_level, \
    _ut_block_level, \
    _block_name, \
    _message, \
    ...) \
{ \
    if ((_max_print_ut_level) > eutblNone) \
    { \
        if ((_ut_block_level) == (_max_print_ut_level)) \
            printf("%s\n", _message); \
        \
        if ((_ut_block_level) <= (_max_print_ut_level)) \
        { \
            for (uint32_t level = (uint32_t)eutblWholeTest; level < (uint32_t)_ut_block_level; level++) \
                printf("\t"); \
            printf("Test \"" _block_name "\": ", __VA_ARGS__); \
        } \
        \
        if ((_ut_block_level) < (_max_print_ut_level)) \
            printf("%s\n", _message); \
    } \
}


#define PG3_UT_PASSED( \
    _max_print_ut_level, \
    _ut_block_level, \
    _block_name, \
    ...) \
{ \
    if ((_max_print_ut_level) > eutblNone) \
    { \
        if ((_ut_block_level) < (_max_print_ut_level)) \
        { \
            for (uint32_t level = (uint32_t)eutblWholeTest; level < (uint32_t)_ut_block_level; level++) \
                printf("\t"); \
            printf("Test \"" _block_name "\" ", __VA_ARGS__); \
        } \
        \
        if ((_ut_block_level) <= (_max_print_ut_level)) \
            printf("PASSED\n"); \
    } \
}


#define __PG3_UT_ERROR_INTERNAL( \
    _max_print_ut_level, \
    _ut_block_level, \
    _block_name, \
    _error_header, \
    _failure_descr, \
    ...) \
{ \
    if ((_max_print_ut_level) > eutblNone) \
    { \
        if ((_ut_block_level) != (_max_print_ut_level)) \
        { \
            if (((_ut_block_level) > (_max_print_ut_level)) && ((_max_print_ut_level) > eutblNone)) \
                printf("\n"); \
            for (uint32_t level = (uint32_t)eutblWholeTest; \
                (level < (uint32_t)_ut_block_level) && (level <= (uint32_t)_max_print_ut_level); \
                level++) \
            { \
                printf("\t"); \
            } \
            printf("Test \"" _block_name "\" ", __VA_ARGS__); \
        } \
        \
        printf(_error_header ": %s\n", _failure_descr); \
    } \
}


#define PG3_UT_FAILED( \
    _max_print_ut_level, \
    _ut_block_level, \
    _block_name, \
    _failure_descr, \
    ...) \
__PG3_UT_ERROR_INTERNAL( \
    _max_print_ut_level, _ut_block_level, _block_name, "FAILED", _failure_descr, \
    __VA_ARGS__)


#define PG3_UT_FATAL_ERROR( \
    _max_print_ut_level, \
    _ut_block_level, \
    _block_name, \
    _failure_descr, \
    ...) \
__PG3_UT_ERROR_INTERNAL( \
    _max_print_ut_level, _ut_block_level, _block_name, "UNIT TEST FATAL ERROR", _failure_descr, \
    __VA_ARGS__)

