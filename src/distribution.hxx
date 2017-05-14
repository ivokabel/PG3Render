/*
    pbrt source code Copyright(c) 1998-2012 Matt Pharr and Greg Humphreys.

    This file is part of pbrt.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#pragma once

#include "math.hxx"
#include "types.hxx"
#include "memory.hxx"

#include <algorithm>
#include <vector>

// Representation of a probability density function over the interval [0,1] - common ancestor class
class Distribution1DBase
{
protected:
    // Function does not have to be normalized
    static void ComputeCdf(
        float           *const oCdf,  // 0..count
        float           &oFuncIntegral,
        const float     *const aFunc, // 0..count-1
        std::size_t      const aSegmCount)
    {
        PG3_ASSERT(aSegmCount > 0);

        // Compute integral of step function at $x_i$.
        // Note that the whole integral spans over the interval [0,1].
        oCdf[0] = 0.; // TODO: Is the leading zero necessary??
        for (std::size_t i = 1; i < aSegmCount + 1; ++i)
        {
            PG3_ASSERT_FLOAT_NONNEGATIVE(aFunc[i - 1]);

            oCdf[i] = oCdf[i - 1] + aFunc[i - 1] / aSegmCount; // 1/aSegmCount = size of a segment
        }

        // Transform step function integral into CDF
        oFuncIntegral = oCdf[aSegmCount];
        if (oFuncIntegral == 0.f)
            // The function is zero; use uniform PDF as a fallback
            for (std::size_t i = 1; i < aSegmCount + 1; ++i)
                oCdf[i] = (float)i / aSegmCount;
        else
            for (std::size_t i = 1; i < aSegmCount + 1; ++i)
                oCdf[i] /= oFuncIntegral;

        PG3_ASSERT_FLOAT_EQUAL(oCdf[aSegmCount], 1.0f, 1e-7F);
    }
};

// Representation of a probability density function over the interval [0,1]
class Distribution1DSimple : public Distribution1DBase
{
public:

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator
    // explicitly, the compiler may complain about not being able 
    // to create its default implementation.
    Distribution1DSimple & operator=(const Distribution1DSimple&) = delete;
    //Distribution1DSimple(const Distribution1DSimple&) = delete;

    Distribution1DSimple(const float * const aFunc, std::size_t aCount) :
        mSegmCount(aCount)
    {
        PG3_ASSERT(aCount > 0);

        mCdf = new float[mSegmCount+1];
        if (mCdf != nullptr)
            ComputeCdf(mCdf, mFuncIntegral, aFunc, mSegmCount);
    }

    ~Distribution1DSimple()
    {
        delete[] mCdf;
    }

    std::size_t SegmCount() const
    {
        return mSegmCount;
    }

    float FuncIntegral() const
    {
        return mFuncIntegral;
    }

    PG3_PROFILING_NOINLINE
    void SampleContinuous(const float aUniSample, float &oX, std::size_t &oSegm, float &oPdf) const 
    {
        const float uniSampleTrim = aUniSample * 0.999999f/*solves the zero ending problem*/;

        // Find surrounding CDF segment
        float *itSegm = std::upper_bound(mCdf, mCdf + mSegmCount + 1, uniSampleTrim);
        oSegm = Math::Clamp<std::size_t>(itSegm - mCdf - 1, 0u, mSegmCount - 1u);

        PG3_ASSERT_INTEGER_IN_RANGE(oSegm, 0, mSegmCount - 1);
        PG3_ASSERT(
               uniSampleTrim >= mCdf[oSegm]
            && uniSampleTrim <  mCdf[oSegm + 1]);

        // Compute offset within CDF segment
        const float segmProbability = mCdf[oSegm + 1] - mCdf[oSegm];
        const float offset = (uniSampleTrim - mCdf[oSegm]) / segmProbability;

        PG3_ASSERT_FLOAT_VALID(offset);
        PG3_ASSERT_FLOAT_IN_RANGE(offset, 0.0f, 1.0f);

        // since the float random generator generates 1.0 from time to time, we need to ignore this one
        //PG3_ASSERT_FLOAT_LESS_THAN(offset, 1.0f);

        // Get the segment's constant PDF = P / Width
        oPdf = segmProbability * mSegmCount;
        PG3_ASSERT(oPdf > 0.f);

        // Return $x\in{}[0,1]$
        oX = (oSegm + offset) / mSegmCount;
    }

    float Pdf(const std::size_t aSegm) const
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aSegm, 0, mSegmCount - 1);

        // Segment's constant PDF = P / Width
        const float segmProbability = mCdf[aSegm + 1] - mCdf[aSegm];
        return segmProbability * mSegmCount;
    }

private:

    friend class Distribution2D;

    float              *mCdf;
    float               mFuncIntegral;
    const std::size_t   mSegmCount;
};


// Representation of a probability density function over the interval [0,1]
// An attempt to make the simple version more cache friendly.
class Distribution1DHierachical : public Distribution1DBase
{
public:

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator
    // explicitly, the compiler may complain about not being able 
    // to create its default implementation.
    Distribution1DHierachical & operator=(const Distribution1DHierachical&) = delete;
    //Distribution1DHierachical(const Distribution1DHierachical&) = delete;

    Distribution1DHierachical(const float * const aFunc, std::size_t aCount)
    {
        BuildHierachy(aFunc, aCount);
    }

    ~Distribution1DHierachical() {}

    static std::size_t GetFullCdfSize(std::size_t aSegmCount)
    {
        return aSegmCount + 1u;
    }

    std::size_t SegmCount() const
    {
        PG3_ASSERT(!IsInitialized());

        return mCdfLevels.back().valuesCount - 1u;
    }

    float FuncIntegral() const
    {
        PG3_ASSERT(!IsInitialized());

        return mFuncIntegral;
    }

private:

    class CdfLevel
    {
    public:

        CdfLevel() :
            cdf(nullptr),
            valuesCount(0),
            blockCount(0)
        {}

        ~CdfLevel() { Free(); }

        // Maximum number of elements in a block
        static std::size_t GetBlockMaxCount()
        {
            return Memory::kCacheLine / sizeof(float);
        }

        const float* GetLevelBegin() const
        {
            PG3_ASSERT(cdf != nullptr);

            return cdf;
        }

        void GetBlockBeginEnd(
            std::size_t      aBlockIdx,
            const float*    &oBegin,
            const float*    &oEnd) const
        {
            PG3_ASSERT(cdf != nullptr);
            PG3_ASSERT_INTEGER_LESS_THAN(aBlockIdx, blockCount);

            const auto blockMaxCount = CdfLevel::GetBlockMaxCount();

            oBegin = cdf + aBlockIdx * blockMaxCount;
            oEnd = oBegin + blockMaxCount; // blocks are guaranteed to be full
        }

        bool Alloc(const std::size_t aValuesCount)
        {
            Free();

            const auto blockMaxCount = CdfLevel::GetBlockMaxCount();

            const auto tmpBlockCount = (std::size_t)std::ceil((float)aValuesCount / blockMaxCount);
            const auto tmpAllocatedCount = tmpBlockCount * blockMaxCount;

            PG3_ASSERT_INTEGER_LARGER_THAN_OR_EQUAL_TO(tmpAllocatedCount, aValuesCount);

            cdf = static_cast<float*>(
                Memory::AlignedMalloc(sizeof(float) * tmpAllocatedCount, Memory::kCacheLine, true));
            if (cdf == nullptr)
                return false;

            valuesCount = aValuesCount;
            allocatedCount = tmpAllocatedCount;
            blockCount = tmpBlockCount;

            return true;
        }

        void Free()
        {
            Memory::AlignedFree(cdf);
            cdf = nullptr;
            valuesCount = 0u;
            allocatedCount = 0u;
            blockCount = 0u;
        }

        // Some levels can be finished with incomplete blocks.
        // Extend the values with the last value (presumably 1.0).
        // This helps avoiding clamping values when computing the "end" iterator
        void FillLastIncompleteBlock()
        {
            const auto lastVal = cdf[valuesCount - 1];
            for (auto idx = valuesCount; idx < allocatedCount; ++idx)
                cdf[idx] = lastVal;

            PG3_ASSERT_FLOAT_EQUAL(lastVal, 1.f, 0.f);
        }

    public:

        float       *cdf;
        std::size_t  valuesCount;
        std::size_t  allocatedCount;
        std::size_t  blockCount;
    };

private:

    static std::size_t ComputeLevelsCount(std::size_t aFullSegmCount)
    {
        const auto fullCdfSize      = GetFullCdfSize(aFullSegmCount);
        const auto blockMaxCount    = CdfLevel::GetBlockMaxCount();

        if ((fullCdfSize == 0u) || (blockMaxCount == 0u))
            return 0u;
        
        const auto logn = Math::LogN((float)blockMaxCount, (float)fullCdfSize);
        const auto levelCount = static_cast<std::size_t>(std::ceil(logn));

        return levelCount;
    }

    static std::size_t ComputeLevelValuesCount(
        const std::size_t    aLevel,
        const std::size_t    aFullSegmCount)
    {
        PG3_ASSERT_INTEGER_LARGER_THAN(aFullSegmCount, 0);

        const auto levelCount       = ComputeLevelsCount(aFullSegmCount);
        const auto fullCdfSize      = GetFullCdfSize(aFullSegmCount);
        const auto blockMaxCount    = CdfLevel::GetBlockMaxCount();

        auto currentLevel       = levelCount - 1u;
        auto currentValuesCount = fullCdfSize;
        while (currentLevel > aLevel)
        {
            currentValuesCount = (std::size_t)std::ceil((float)currentValuesCount / blockMaxCount);
            --currentLevel;
        }

        PG3_ASSERT_INTEGER_LARGER_THAN(currentValuesCount, 0);

        return currentValuesCount;
    }

    bool BuildHierachy(const float * const aFunc, std::size_t aFullSegmCount)
    {
        const auto levelCount = ComputeLevelsCount(aFullSegmCount);
        if (levelCount == 0)
            return false;

        // Allocate all levels
        mCdfLevels.resize(levelCount);
        for (std::size_t level = levelCount - 1u; level < levelCount/*unsigned counter!*/; --level)
        {
            const std::size_t valuesCount = ComputeLevelValuesCount(level, aFullSegmCount);
            if (!mCdfLevels[level].Alloc(valuesCount))
                return false;
        }

        PG3_ASSERT(mCdfLevels.back().valuesCount == (aFullSegmCount + 1));

        // Compute last level
        auto &lastLevel = mCdfLevels.back();
        ComputeCdf(lastLevel.cdf, mFuncIntegral, aFunc, aFullSegmCount);
        lastLevel.FillLastIncompleteBlock();

        // Build hierarchy
        for (auto level = levelCount - 1u; level > 0u; --level)
        {
            auto &currentLevel = mCdfLevels[level];
            auto &higherLevel  = mCdfLevels[level - 1];

            PG3_ASSERT(higherLevel.valuesCount == currentLevel.blockCount);

            for (std::size_t block = 0u; block < currentLevel.blockCount; ++block)
            {
                const float *blockBegin, *blockEnd;
                currentLevel.GetBlockBeginEnd(block, blockBegin, blockEnd);

                PG3_ASSERT(blockBegin < blockEnd); blockBegin; // empty block

                auto lastBlockValue = blockEnd - 1;
                higherLevel.cdf[block] = *lastBlockValue;
            }

            higherLevel.FillLastIncompleteBlock();
        }

        PG3_ASSERT(mCdfLevels[0u].blockCount == 1u);

        return true;
    }

    bool IsInitialized() const
    {
        return mCdfLevels.empty();
    }

    PG3_PROFILING_NOINLINE
    void SampleContinuous(const float aUniSample, float &oX, std::size_t &oSegm, float &oPdf) const 
    {
        PG3_ASSERT(!IsInitialized());
        PG3_ASSERT_FLOAT_IN_RANGE(aUniSample, 0.f, 1.f);

        const float uniSampleTrim = aUniSample * 0.999999f/*solves the zero ending problem*/;

        // Find surrounding CDF segment using the CDF hierarchy
        std::size_t segPos = 0u;
        const auto levelsCount = mCdfLevels.size();
        for (std::size_t levelIdx = 0u, block = 0u; levelIdx < levelsCount; ++levelIdx)
        {
            auto &currentLevel = mCdfLevels[levelIdx];
            const float *levelBegin = currentLevel.GetLevelBegin();
            const float *blockBegin, *blockEnd;
            currentLevel.GetBlockBeginEnd(block, blockBegin, blockEnd);

            PG3_ASSERT(block < currentLevel.blockCount);

            const auto itSegm = std::upper_bound(blockBegin, blockEnd, uniSampleTrim);
            
            segPos = itSegm - levelBegin;

            PG3_ASSERT(segPos < currentLevel.valuesCount);

            auto nextBlock = segPos;

            PG3_ASSERT((levelIdx >= levelsCount - 1u) || (nextBlock < mCdfLevels[levelIdx + 1].blockCount));

            block = nextBlock;
        }

        PG3_ASSERT(segPos > 0u);
        PG3_ASSERT(segPos < mCdfLevels.back().valuesCount);

        //oSegm = Math::Clamp<std::size_t>(segPos - 1u, 0u, mCdfLevels.back().valuesCount - 1u);
        oSegm = segPos - 1u; // Full CDF is shifted by 1 (starts with 0)

        PG3_ASSERT(
               uniSampleTrim >= mCdfLevels.back().cdf[oSegm]
            && uniSampleTrim <  mCdfLevels.back().cdf[oSegm + 1]);

        auto fullCdf   = mCdfLevels.back().cdf;
        auto segmCount = mCdfLevels.back().valuesCount - 1u;

        // Compute offset within CDF segment
        const float segmProbability = fullCdf[oSegm + 1] - fullCdf[oSegm];
        const float offset = (uniSampleTrim - fullCdf[oSegm]) / segmProbability;

        PG3_ASSERT_FLOAT_IN_RANGE(offset, 0.0f, 1.0f);

        // since the float random generator generates 1.0 from time to time, we need to ignore this one
        // TODO: Is this still a problem after the "zero-samples problem" fix?
        PG3_ASSERT_FLOAT_LESS_THAN(offset, 1.0f);

        // Get the segment's constant PDF = P / Width
        oPdf = segmProbability * segmCount;

        PG3_ASSERT(oPdf > 0.f);

        // Return $x\in{}[0,1]$
        oX = (oSegm + offset) / segmCount;

        PG3_ASSERT_FLOAT_IN_RANGE(oX, 0.0f, 1.0f);
    }

    float Pdf(const std::size_t aSegm) const
    {
        PG3_ASSERT(!IsInitialized());
        PG3_ASSERT_INTEGER_IN_RANGE(aSegm, 0, mCdfLevels.back().valuesCount - 1);

        const auto &lastLevel = mCdfLevels.back();
        const auto segmCount = lastLevel.valuesCount - 1u;

        // Segment's constant PDF = P / Width
        const float segmProbability = lastLevel.cdf[aSegm + 1] - lastLevel.cdf[aSegm];
        return segmProbability * segmCount;
    }

private:

    friend class Distribution2D;

    std::vector<CdfLevel>   mCdfLevels; // The last level is the full one
    float                   mFuncIntegral;
};

#ifndef PG3_USE_HIERARCHICAL_1D_DISTRIBUTION
typedef Distribution1DSimple Distribution1D;
#else
typedef Distribution1DHierachical Distribution1D;
#endif

class Distribution2D
{
public:

    Distribution2D(const float *aFunc, int32_t sCountU, int32_t sCountV)
    {
        mConditionals.reserve(sCountV);
        for (int32_t v = 0; v < sCountV; ++v) 
            // Compute conditional sampling distributions for $\tilde{v}$
            mConditionals.push_back(new Distribution1D(&aFunc[v*sCountU], sCountU));

        // Compute marginal sampling distribution $p[\tilde{v}]$
        std::vector<float> marginalFunc;
        marginalFunc.reserve(sCountV);
        for (int32_t v = 0; v < sCountV; ++v)
            marginalFunc.push_back(mConditionals[v]->FuncIntegral());
        mMarginal = new Distribution1D(&marginalFunc[0], sCountV);
    }

    ~Distribution2D()
    {
        delete mMarginal;
        for (auto& conditional : mConditionals)
            delete conditional;
    }

    void SampleContinuous(const Vec2f &rndSamples, Vec2f &oUV, Vec2ui &oSegm, float *oPdf) const
    {
        float margPdf, condPdf;
        std::size_t segmX, segmY;

        mMarginal->SampleContinuous(rndSamples.x, oUV.y, segmY, margPdf);
        mConditionals[segmY]->SampleContinuous(rndSamples.y, oUV.x, segmX, condPdf);

        oSegm.x = (uint32_t)segmX;
        oSegm.y = (uint32_t)segmY;

        if (oPdf != nullptr)
            *oPdf = margPdf * condPdf;
    }

    float Pdf(const Vec2f &aUV) const
    {
        // Find u and v segments
        std::size_t iu = Math::Clamp<std::size_t>(
            (std::size_t)(aUV.x * mConditionals[0]->SegmCount()), 0u, mConditionals[0]->SegmCount() - 1);
        std::size_t iv = Math::Clamp<std::size_t>(
            (std::size_t)(aUV.y * mMarginal->SegmCount()), 0u, mMarginal->SegmCount() - 1);

        // Compute probabilities
        return mConditionals[iv]->Pdf(iu) * mMarginal->Pdf(iv);
    }

private:

    std::vector<Distribution1D*>     mConditionals;
    Distribution1D                  *mMarginal;
};
