#pragma once

#include <ImfRgbaFile.h>    // OpenEXR
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

    // Loads, scales and rotates an environment map from an OpenEXR image on the given path.
    static EnvironmentMapImage* LoadImage(
        const char *aFilename,
        float aAzimuthRotation = 0.0f,
        float aScale = 1.0f)
    {
        aAzimuthRotation = Math::FmodX(aAzimuthRotation, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(aAzimuthRotation, 0.0f, 1.0f);

        Imf::RgbaInputFile file(aFilename, 1);
        Imath::Box2i dw = file.dataWindow();

        int32_t width   = dw.max.x - dw.min.x + 1;
        int32_t height  = dw.max.y - dw.min.y + 1;
        Imf::Rgba* rgbaData = new Imf::Rgba[height*width];

        file.setFrameBuffer(rgbaData - dw.min.x - dw.min.y * width, 1, width);
        file.readPixels(dw.min.y, dw.max.y);

        EnvironmentMapImage* image = new EnvironmentMapImage(width, height);

        int32_t c = 0;
        int32_t iRot = (int32_t)(aAzimuthRotation * width);
        for (uint32_t j = 0; j < image->Height(); j++)
        {
            for (uint32_t i = 0; i < image->Width(); i++)
            {
                int32_t x = i + iRot;
                if (x >= width) x -= width;
                image->ElementAt(x, j).SetSRGBLight(
                    rgbaData[c].r * aScale,
                    rgbaData[c].g * aScale,
                    rgbaData[c].b * aScale
                    );
                c++;
            }
        }

        delete rgbaData;

        return image;
    }

    SpectrumF Evaluate(const Vec2f &aUV, bool aDoBilinFiltering) const
    {
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.x, 0.0f, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.y, 0.0f, 1.0f);

        // UV to image coords
        const float x = aUV.x * (float)mWidth;
        const float y = aUV.y * (float)mHeight;
        const uint32_t x0 = Math::Clamp((uint32_t)x, 0u, mWidth  - 1u);
        const uint32_t y0 = Math::Clamp((uint32_t)y, 0u, mHeight - 1u);

        // Eval
        if (!aDoBilinFiltering)
            return ElementAt(x0, y0);
        else
        {
            PG3_ERROR_NOT_IMPLEMENTED("");

            return SpectrumF();

            //const uint32_t x1 = x0 + 1u;
            //const uint32_t y1 = y0 + 1u;
            //const float xLocal = x - (float)x0;
            //const float yLocal = y - (float)y0;

            //return Math::BLerp(
            //    xLocal, yLocal,
            //    ElementAt(x0, y0),
            //    ElementAt(x1, y0),
            //    ElementAt(x0, y1),
            //    ElementAt(x1, y1));
            ////return (1 - ty) * ((1 - tx) * mImage->ElementAt(xi1, yi1) + tx * mImage->ElementAt(xi2, yi1))
            ////    + ty * ((1 - tx) * mImage->ElementAt(xi1, yi2) + tx * mImage->ElementAt(xi2, yi2));
        }
    }

    SpectrumF Evaluate(const Vec3f &aDirection, bool aDoBilinFiltering) const
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aDirection);

        const Vec2f uv = Geom::Dir2LatLong(aDirection);
        return Evaluate(uv, aDoBilinFiltering);
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
