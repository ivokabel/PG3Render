#pragma once

#include <vector>
#include <cmath>
#include "scene.hxx"
#include "framebuffer.hxx"
#include "config.hxx"
#include "types.hxx"
#include "hardsettings.hxx"

enum PathTerminationReason
{
    kTerminatedByRussianRoulette,

    // Background/environment map was hit
    kTerminatedByBackground,

    // Material with zero reflectance encountered (e.g. lights)
    kTerminatedByBlocker,    

    // Explicit maximal allowed path length was reached
    // (safety recursion limit doesn't count into this)
    kTerminatedByMaxLimit,

    // Stopped by hardwired safety recursion limit (e.g. PathTracer::kMaxPathLength)
    // to avoid stack overflow problems
    kTerminatedBySafetyLimit
};

class RendererIntrospectionDataBase
{
#ifdef COMPUTE_AND_PRINT_RENDERER_INTROSPECTION

public:
    RendererIntrospectionDataBase()
        :
        mCorePathsCount(0),
        mCorePathsMinLength(UINT32_MAX),
        mCorePathsMaxLength(0),
        mCorePathsTerminatedByRussianRoulette(0),
        mCorePathsTerminatedByBackground(0),
        mCorePathsTerminatedByBlocker(0),
        mCorePathsTerminatedByMaxLimit(0),
        mCorePathsTerminatedBySafetyLimit(0)
    {}

protected:
    uint32_t        mCorePathsCount;
    uint32_t        mCorePathsMinLength;
    uint32_t        mCorePathsMaxLength;

    // Histogram of path termination reasons, for more details see PathTerminationReason
    uint32_t        mCorePathsTerminatedByRussianRoulette;
    uint32_t        mCorePathsTerminatedByBackground;
    uint32_t        mCorePathsTerminatedByBlocker;
    uint32_t        mCorePathsTerminatedByMaxLimit;
    uint32_t        mCorePathsTerminatedBySafetyLimit; // Not contained in the lengths histogram

    // Histogram of lengths
    typedef std::vector<uint32_t> TPathHistogram;
    TPathHistogram  mCorePathsLengthHistogram; // Exponential size of bins with base2: 1, 2, 4, 8, ...

protected:
    uint32_t LengthToSegment(uint32_t aLength) const
    {
        return static_cast<uint32_t>(std::log2(aLength + 1));
    }

    uint32_t SegmentToShortestLength(uint32_t aSegment) const
    {
        return static_cast<uint32_t>(std::exp2(aSegment)) - 1u;
    }

#endif
};

class RendererIntrospectionData : public RendererIntrospectionDataBase
{
public:
    void AddCorePathLength(uint32_t aLength, const PathTerminationReason &aTerminationReason)
    {
        aLength; aTerminationReason; // potentially unused params

#ifdef COMPUTE_AND_PRINT_RENDERER_INTROSPECTION

        mCorePathsCount++;
        mCorePathsMinLength = std::min(mCorePathsMinLength, aLength);
        mCorePathsMaxLength = std::max(mCorePathsMaxLength, aLength);

        if (aTerminationReason != kTerminatedBySafetyLimit)
        {
            // Lenghts histogram
            const auto segment = LengthToSegment(aLength);

            const auto neededSize = segment + 1u;
            if (mCorePathsLengthHistogram.size() < neededSize)
                mCorePathsLengthHistogram.resize(neededSize, 0u);
            auto &histoVal = mCorePathsLengthHistogram[segment];
            histoVal++;

            // Termination reasons
            if (aTerminationReason == kTerminatedByRussianRoulette)
                mCorePathsTerminatedByRussianRoulette++;
            else if (aTerminationReason == kTerminatedByBackground)
                mCorePathsTerminatedByBackground++;
            else if (aTerminationReason == kTerminatedByBlocker)
                mCorePathsTerminatedByBlocker++;
            else if (aTerminationReason == kTerminatedByMaxLimit)
                mCorePathsTerminatedByMaxLimit++;
            else
                PG3_FATAL_ERROR("Unecognised path termination reason!");
        }
        else
            mCorePathsTerminatedBySafetyLimit++;

#endif
    }

    friend class RendererIntrospectionDataAggregator;
};

class RendererIntrospectionDataAggregator : public RendererIntrospectionDataBase
{
public:

    void AddRendererData(const RendererIntrospectionData &aRendererData)
    {
        aRendererData;

#ifdef COMPUTE_AND_PRINT_RENDERER_INTROSPECTION

        mCorePathsCount += aRendererData.mCorePathsCount;

        mCorePathsMinLength = std::min(mCorePathsMinLength, aRendererData.mCorePathsMinLength);
        mCorePathsMaxLength = std::max(mCorePathsMaxLength, aRendererData.mCorePathsMaxLength);

        mCorePathsTerminatedByRussianRoulette   += aRendererData.mCorePathsTerminatedByRussianRoulette;
        mCorePathsTerminatedByBackground        += aRendererData.mCorePathsTerminatedByBackground;
        mCorePathsTerminatedByBlocker           += aRendererData.mCorePathsTerminatedByBlocker;
        mCorePathsTerminatedByMaxLimit          += aRendererData.mCorePathsTerminatedByMaxLimit;
        mCorePathsTerminatedBySafetyLimit       += aRendererData.mCorePathsTerminatedBySafetyLimit;

        const uint32_t rendererDataSize =
            static_cast<uint32_t>(aRendererData.mCorePathsLengthHistogram.size());
        if (mCorePathsLengthHistogram.size() < rendererDataSize)
            mCorePathsLengthHistogram.resize(rendererDataSize, 0u);

        for (uint32_t segment = 0; segment < rendererDataSize; segment++)
            mCorePathsLengthHistogram[segment] += aRendererData.mCorePathsLengthHistogram[segment];

#endif
    }

    void PrintIntrospection() const
    {
#ifdef COMPUTE_AND_PRINT_RENDERER_INTROSPECTION

        printf("\nIntrospection (core paths): ");
        if (mCorePathsCount > 0)
        {
            printf(
                "count %d, min path length %d, max path length %d\n",
                mCorePathsCount, mCorePathsMinLength, mCorePathsMaxLength);

            const uint32_t histoSize = static_cast<uint32_t>(mCorePathsLengthHistogram.size());
            for (uint32_t segment = 0; segment < histoSize; segment++)
            {
                auto lowerBound = SegmentToShortestLength(segment);
                auto upperBound = SegmentToShortestLength(segment + 1) - 1u;
                auto count = mCorePathsLengthHistogram[segment];
                PrintTerminatedPathsCountByLengths(lowerBound, upperBound, count);
            }

            // Normal termination reasons
            printf("\t----------------------------------------------\n");
            PrintTerminatedPathsCountByReason(mCorePathsTerminatedByRussianRoulette, "Russian roulette");
            PrintTerminatedPathsCountByReason(mCorePathsTerminatedByBackground, "Background");
            PrintTerminatedPathsCountByReason(mCorePathsTerminatedByBlocker, "Blocker");
            PrintTerminatedPathsCountByReason(mCorePathsTerminatedByMaxLimit, "Max limit");

            // Hard limit termination
            printf("\t----------------------------------------------\n");
            PrintTerminatedPathsCountByReason(mCorePathsTerminatedBySafetyLimit, "Cut (too long)");
        }
        else
            printf("no data!\n");

#endif
    }

protected:

    void PrintTerminatedPathsCountByLengths(
        const uint32_t aLowerBound,
        const uint32_t aUpperBound,
        const uint32_t aTerminatedCount) const
    {
        aLowerBound; aUpperBound;  aTerminatedCount; // potentially unused params

#ifdef COMPUTE_AND_PRINT_RENDERER_INTROSPECTION

        auto percentage = (100.f * aTerminatedCount) / mCorePathsCount;
        printf(
            "\tlengths %5d-%-5d: %7.4f%% %10d %s\n",
            aLowerBound, aUpperBound, percentage,
            aTerminatedCount, aTerminatedCount == 1 ? "path" : "paths");

#endif
    }

    void PrintTerminatedPathsCountByReason(
        const uint32_t aTerminatedCount,
        const char *aTermReasonDescr) const
    {
        aTerminatedCount; aTermReasonDescr; // potentially unused params

#ifdef COMPUTE_AND_PRINT_RENDERER_INTROSPECTION

        const auto percentage = (100.f * aTerminatedCount) / mCorePathsCount;
        printf(
            "\t%-19s: %7.4f%% %10d %s\n",
            aTermReasonDescr, percentage, aTerminatedCount,
            aTerminatedCount == 1 ? "path" : "paths");

#endif
    }

};


// Base renderer class
class AbstractRenderer
{
public:

    AbstractRenderer(const Config &aConfig) : mConfig(aConfig)
    {
        mIterations = 0;
        mFramebuffer.Setup(mConfig.mScene->mCamera.mResolution);
    }

    virtual ~AbstractRenderer(){}

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator and copy constructor 
    // explicitly, the compiler may complain about not being able 
    // to create their default implementations.
    AbstractRenderer & operator=(const AbstractRenderer&) = delete;
    AbstractRenderer(const AbstractRenderer&) = delete;

    virtual void RunIteration(
        const Algorithm     aAlgorithm,
        uint32_t            aIteration) = 0;

    void GetFramebuffer(Framebuffer& oFramebuffer)
    {
        oFramebuffer = mFramebuffer;

        if (mIterations > 0)
            oFramebuffer.Scale(FramebufferFloat(1.) / mIterations);
    }

    const RendererIntrospectionData &GetRendererIntrospectionData() const
    {
        return mIntrospectionData;
    }

    //! Whether this renderer was used at all
    bool WasUsed() const { return mIterations > 0; }

protected:

    uint32_t         mIterations;
    Framebuffer      mFramebuffer;
    const Config    &mConfig;

    RendererIntrospectionData   mIntrospectionData;
};
