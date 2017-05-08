#pragma once

//#include "debugging.hxx"

#include <cstdlib>

namespace Memory
{
    void* AlignedMalloc(std::size_t aAlignment, std::size_t aSize, bool aZeroMemory)
    {
        // TODO: Aligned allocation: platform-dependent
        aAlignment; // unused so far
        void * result = std::malloc(aSize);
        if (result == nullptr)
            return nullptr;

        // TODO: Later C++17 std::aligned_alloc()

        if (aZeroMemory)
            std::memset(result, 0, aSize);

        return result;
    }
    
    void AlignedFree(void* aData)
    {
        std::free(aData);
    }
}
