#pragma once

//#include "debugging.hxx"

#include <cstdlib>

#ifdef _MSC_VER 
#include <malloc.h>
#endif

namespace Memory
{
    static const std::size_t kCacheLine = 64u;

    void* AlignedMalloc(std::size_t aSize, std::size_t aAlignment, bool aZeroMemory)
    {
        // Platform-dependent aligned allocation
        // TODO: In C++17 use std::aligned_alloc()
        void * result =
#ifdef _MSC_VER 
        _aligned_malloc(aSize, aAlignment);
#else 
        #error Aligned memory allocator is not implemented!
        std::malloc(aSize);
#endif
        if (result == nullptr)
            return nullptr;

        if (aZeroMemory)
            std::memset(result, 0, aSize);

        return result;
    }
    
    void AlignedFree(void* aData)
    {
#ifdef _MSC_VER 
        _aligned_free(aData);
#else 
        #error Aligned memory allocator is not implemented!
        std::free(aData);
#endif
    }
}
