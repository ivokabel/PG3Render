#pragma once

#include "hardsettings.hxx"

// Unit testing if switched on by defining RUN_UNIT_TESTS_INSTEAD_OF_RENDERER in hard-wired settings

enum UnitTestBlockLevel
{
    eutblNone           = 0,
    eutblWholeTest      = 1,    // Main testing block
    eutblSubTest        = 2,    // Test can have sub-blocks
    eutblSingleStep     = 3,    // Tests can constist of small testing steps
};


#define UT_BEGIN( \
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


#define UT_END_PASSED( \
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


#define __UT_ERROR_INTERNAL( \
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


#define UT_END_FAILED( \
    _max_print_ut_level, \
    _ut_block_level, \
    _block_name, \
    _failure_descr, \
    ...) \
__UT_ERROR_INTERNAL( \
    _max_print_ut_level, _ut_block_level, _block_name, "FAILED", _failure_descr, \
    __VA_ARGS__)


#define UT_FATAL_ERROR( \
    _max_print_ut_level, \
    _ut_block_level, \
    _block_name, \
    _failure_descr, \
    ...) \
__UT_ERROR_INTERNAL( \
    _max_print_ut_level, _ut_block_level, _block_name, "UNIT TEST FATAL ERROR", _failure_descr, \
    __VA_ARGS__)

