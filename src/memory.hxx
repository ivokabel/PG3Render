#pragma once

//#include "debugging.hxx"

#include <cstdlib>

namespace Memory
{
    void* AlignedMalloc(std::size_t aAlignment, std::size_t aSize)
    {
        // TODO: Aligned allocation: platform-dependent
        aAlignment; // unused so far
        return std::malloc(aSize);

        // TODO: Later C++17 std::aligned_alloc()
    }
    
    void AlignedFree(void* aData)
    {
        std::free(aData);
    }
}
