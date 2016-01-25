#pragma once

#include <vector>
#include <cmath>
#include "scene.hxx"
#include "framebuffer.hxx"
#include "config.hxx"
#include "types.hxx"
#include "hardsettings.hxx"

class RendererIntrospectionDataBase
{
#ifdef COMPUTE_AND_PRINT_RENDERER_INTROSPECTION

public:
    RendererIntrospectionDataBase()
        :
        mCorePathCount(0),
        mMinCorePathLength(UINT32_MAX),
        mMaxCorePathLength(0),
        mCutTooLongCorePaths(0)
    {}

protected:
    typedef std::vector<uint32_t> TPathHistogram;

    uint32_t        mCorePathCount;
    uint32_t        mMinCorePathLength;
    uint32_t        mMaxCorePathLength;

    // Paths which where cut by hard limit (e.g. PATH_TRACER_MAX_PATH_LENGTH).
    // These are not contained in the lengths histogram
    uint32_t        mCutTooLongCorePaths;

    TPathHistogram  mCorePathLengthsHistogram; // Exponential size of bins with base2: 1, 2, 4, 8, ...

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
    void AddCorePathLength(uint32_t aLength, bool aCutTooLong = false)
    {
        aLength;

#ifdef COMPUTE_AND_PRINT_RENDERER_INTROSPECTION

        mCorePathCount++;
        mMinCorePathLength = std::min(mMinCorePathLength, aLength);
        mMaxCorePathLength = std::max(mMaxCorePathLength, aLength);

        if (!aCutTooLong)
        {
            // Histogram
            const auto segment = LengthToSegment(aLength);

            const auto neededSize = segment + 1u;
            if (mCorePathLengthsHistogram.size() < neededSize)
                mCorePathLengthsHistogram.resize(neededSize, 0u);
            auto &histoVal = mCorePathLengthsHistogram[segment];
            histoVal++;
        }
        else
            mCutTooLongCorePaths++;

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

        mCorePathCount          += aRendererData.mCorePathCount;
        mMinCorePathLength       = std::min(mMinCorePathLength, aRendererData.mMinCorePathLength);
        mMaxCorePathLength       = std::max(mMaxCorePathLength, aRendererData.mMaxCorePathLength);
        mCutTooLongCorePaths    += aRendererData.mCutTooLongCorePaths;

        const uint32_t rendererDataSize =
            static_cast<uint32_t>(aRendererData.mCorePathLengthsHistogram.size());
        if (mCorePathLengthsHistogram.size() < rendererDataSize)
            mCorePathLengthsHistogram.resize(rendererDataSize, 0u);

        for (uint32_t segment = 0; segment < rendererDataSize; segment++)
            mCorePathLengthsHistogram[segment] += aRendererData.mCorePathLengthsHistogram[segment];

#endif
    }

    void PrintIntrospection() const
    {
#ifdef COMPUTE_AND_PRINT_RENDERER_INTROSPECTION

        printf("\nIntrospection - ");
        if (mCorePathCount > 0)
        {
            printf(
                "core paths: count %d, min path length %d, max path length %d\n",
                mCorePathCount, mMinCorePathLength, mMaxCorePathLength);

            const uint32_t histoSize = static_cast<uint32_t>(mCorePathLengthsHistogram.size());
            for (uint32_t segment = 0; segment < histoSize; segment++)
            {
                auto lowerBound = SegmentToShortestLength(segment);
                auto upperBound = SegmentToShortestLength(segment + 1) - 1u;
                auto count = mCorePathLengthsHistogram[segment];
                auto percentage = (100.f * count) / mCorePathCount;
                printf(
                    "\tlengths %5d-%-5d: %7.4f%% %10d %s\n",
                    lowerBound, upperBound, percentage, count, count == 1 ? "path" : "paths");
            }

            auto percentage = (100.f * mCutTooLongCorePaths) / mCorePathCount;
            printf(
                "\tCut (too long)     : %7.4f%% %10d %s\n",
                percentage, mCutTooLongCorePaths, mCutTooLongCorePaths == 1 ? "path" : "paths");
        }
        else
            printf("no data!\n");

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
