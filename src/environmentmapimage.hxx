#pragma once

#include "spectrum.hxx"
#include "debugging.hxx"
#include "types.hxx"

///////////////////////////////////////////////////////////////////////////////////////////////////
// EnvironmentMapImage is adopted from SmallUPBP project and used as a reference for my own 
// implementations.
///////////////////////////////////////////////////////////////////////////////////////////////////

// Image holding environment map image data in latitude-longitude coordinates
class EnvironmentMapImage
{
public:
    EnvironmentMapImage(uint32_t aWidth, uint32_t aHeight)
    {
        mWidth  = aWidth;
        mHeight = aHeight;

        mData = new SpectrumF[mWidth * mHeight];
    }

    ~EnvironmentMapImage()
    {
        if (mData)
            delete mData;
        mData   = nullptr;
        mWidth  = 0;
        mHeight = 0;
    }

    SpectrumF& ElementAt(uint32_t aX, uint32_t aY)
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aX, 0, mWidth);
        PG3_ASSERT_INTEGER_IN_RANGE(aY, 0, mHeight);

        return mData[mWidth*aY + aX];
    }

    const SpectrumF& ElementAt(uint32_t aX, uint32_t aY) const
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aX, 0, mWidth);
        PG3_ASSERT_INTEGER_IN_RANGE(aY, 0, mHeight);

        return mData[mWidth*aY + aX];
    }

    SpectrumF& ElementAt(uint32_t aIdx)
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aIdx, 0, mWidth*mHeight);

        return mData[aIdx];
    }

    const SpectrumF& ElementAt(uint32_t aIdx) const
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aIdx, 0, mWidth*mHeight);

        return mData[aIdx];
    }

    Vec2ui Size() const
    {
        return Vec2ui(mWidth, mHeight);
    }

    uint32_t Width() const
    {
        return mWidth;
    }

    uint32_t Height() const
    { 
        return mHeight; 
    }

    SpectrumF   *mData;
    uint32_t     mWidth;
    uint32_t     mHeight;
};
