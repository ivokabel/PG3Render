#pragma once

#include <ImfRgbaFile.h>    // OpenEXR
#include "filter.hxx"
#include "spectrum.hxx"
#include "geom.hxx"
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
    EnvironmentMapImage(
        const char *aFilename,
        uint32_t    aWidth,
        uint32_t    aHeight,
        bool        aDoBilinFiltering)
        :
        mFilename(aFilename),
        mWidth(aWidth),
        mHeight(aHeight),
        mDoBilinFiltering(aDoBilinFiltering),
#ifdef PG3_USE_EM_MORTON_MAPPING
        mImgDataSize(Math::MortonCode2D(aWidth - 1, aHeight - 1) + 1) // based on the last element 
#else
        mImgDataSize(mWidth * mHeight)
#endif
    {
        mImgData = new SpectrumF[mImgDataSize];
    }

    ~EnvironmentMapImage()
    {
        delete mImgData;
    }

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator
    // explicitly, the compiler may complain about not being able 
    // to create its default implementation.
    EnvironmentMapImage & operator=(const EnvironmentMapImage&) = delete;
    //EnvironmentMapImage(const EnvironmentMapImage&) = delete;

    // Loads, scales and rotates an environment map from an OpenEXR image on the given path.
    static EnvironmentMapImage* LoadImage(
        const char *aFilename,
        float       aAzimuthRotation = 0.0f,
        float       aScale = 1.0f,
        bool        aDoBilinFiltering = true)
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

        EnvironmentMapImage* image =
            new EnvironmentMapImage(aFilename, width, height, aDoBilinFiltering);

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

        delete[] rgbaData;

        return image;
    }

    SpectrumF Evaluate(const Vec2f &aUV) const
    {
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.x, 0.0f, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.y, 0.0f, 1.0f);

        // UV to image coords
        const float xFull = aUV.x * (float)mWidth;
        const float yFull = aUV.y * (float)mHeight;

        // Eval
        if (!mDoBilinFiltering)
        {
            // Box filter

            const uint32_t x0 = Math::Clamp((uint32_t)xFull, 0u, mWidth  - 1u);
            const uint32_t y0 = Math::Clamp((uint32_t)yFull, 0u, mHeight - 1u);
            
            return ElementAt(x0, y0);
        }
        else
        {
            // Triangle (tent) filter

            // Find the centre of the enclosing rectangle (vertices are middle points of EM pixels)
            const Vec2i centre(
                (int32_t)(xFull + 0.5f),
                (int32_t)(yFull + 0.5f));

            const Vec2i coords0 = centre - Vec2i(1, 1);
            const Vec2i coords1 = centre;

            const float xLocal = xFull - ((float)coords0.x + 0.5f);
            const float yLocal = yFull - ((float)coords0.y + 0.5f);

            PG3_ASSERT_FLOAT_IN_RANGE(xLocal, 0.f, 1.0f);
            PG3_ASSERT_FLOAT_IN_RANGE(yLocal, 0.f, 1.0f);

            const Vec2ui normCoords0 = NormalizeImgCoords(coords0);
            const Vec2ui normCoords1 = NormalizeImgCoords(coords1);

            return Filter::Triangle(
                xLocal, yLocal,
                ElementAt(normCoords0.x, normCoords0.y),
                ElementAt(normCoords1.x, normCoords0.y),
                ElementAt(normCoords0.x, normCoords1.y),
                ElementAt(normCoords1.x, normCoords1.y));
        }
    }

    SpectrumF Evaluate(const Vec3f &aDirection) const
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aDirection);

        const Vec2f uv = Geom::Dir2LatLong(aDirection);
        return Evaluate(uv);
    }

    float AveragePixelLuminance(uint32_t aX, uint32_t aY) const
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aX, 0, mWidth);
        PG3_ASSERT_INTEGER_IN_RANGE(aY, 0, mHeight);

        if (!mDoBilinFiltering)
        {
            // Box filter
            return ElementAt(aX, aY).Luminance();
        }
        else
        {
            // Triangle (tent) filter

            const Vec2ui coords0 = NormalizeImgCoords(Vec2i(aX - 1, aY - 1));
            const Vec2ui coords1 = Vec2ui(aX, aY);
            const Vec2ui coords2 = NormalizeImgCoords(Vec2i(aX + 1, aY + 1));

            const float integral = Filter::TriangleIntegral(
                ElementAt(coords0.x, coords0.y).Luminance(),
                ElementAt(coords1.x, coords0.y).Luminance(),
                ElementAt(coords2.x, coords0.y).Luminance(),
                ElementAt(coords0.x, coords1.y).Luminance(),
                ElementAt(coords1.x, coords1.y).Luminance(),
                ElementAt(coords2.x, coords1.y).Luminance(),
                ElementAt(coords0.x, coords2.y).Luminance(),
                ElementAt(coords1.x, coords2.y).Luminance(),
                ElementAt(coords2.x, coords2.y).Luminance());

            return integral;
        }
    }

    SpectrumF& ElementAt(uint32_t aX, uint32_t aY)
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aX, 0, mWidth);
        PG3_ASSERT_INTEGER_IN_RANGE(aY, 0, mHeight);

#ifdef PG3_USE_EM_MORTON_MAPPING
        const uint32_t offset = Math::MortonCode2D(aX, aY);
#else
        const uint32_t offset = mWidth*aY + aX;
#endif

        PG3_ASSERT(offset < mImgDataSize);

        return mImgData[offset];
    }

    const SpectrumF& ElementAt(uint32_t aX, uint32_t aY) const
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aX, 0, mWidth);
        PG3_ASSERT_INTEGER_IN_RANGE(aY, 0, mHeight);

#ifdef PG3_USE_EM_MORTON_MAPPING
        const uint32_t offset = Math::MortonCode2D(aX, aY);
#else
        const uint32_t offset = mWidth*aY + aX;
#endif

        PG3_ASSERT(offset < mImgDataSize);

        return mImgData[offset];
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

    std::string Filename() const
    {
        return mFilename;
    }

    bool IsUsingBilinearFiltering() const
    {
        return mDoBilinFiltering;
    }

private:

    // Wraps coordinates around the edges of the environment image
    Vec2ui NormalizeImgCoords(const Vec2i &aCoords) const
    {
        // Note that this will not work properly if the y coordinate is further than one over 
        // the edge (-2, height + 1, etc). However, since we use this function only for 
        // handling corner cases, it doesn't matter.

        int32_t resultX, resultY;

        if (aCoords.y >= (int32_t)mHeight)
        {
            // Wrap around the south pole
            resultX = aCoords.x + mWidth / 2;
            resultY = mHeight - 1;
        }
        else if (aCoords.y < 0)
        {
            // Wrap around the north pole
            resultX = aCoords.x + mWidth / 2;
            resultY = 0;
        }
        else
        {
            resultX = aCoords.x;
            resultY = aCoords.y;
        }

        // Wrap x around the prime meridian
        resultX = Math::ModX(resultX, (int32_t)mWidth);

        PG3_ASSERT_INTEGER_IN_RANGE(resultX, 0, (int32_t)mWidth - 1);
        PG3_ASSERT_INTEGER_IN_RANGE(resultY, 0, (int32_t)mHeight - 1);

        return Vec2ui((uint32_t)resultX, (uint32_t)resultY);
    }

private:

    const std::string    mFilename;
    const uint32_t       mWidth;
    const uint32_t       mHeight;
    const bool           mDoBilinFiltering;
    SpectrumF           *mImgData;
    const uint32_t       mImgDataSize;
};

// Wrapper for constant environment
class ConstEnvironmentValue
{
public:
    ConstEnvironmentValue(SpectrumF aConstantValue) : mConstantValue(aConstantValue) {}

    SpectrumF Evaluate(const Vec3f &aDirection) const
    {
        aDirection; //unused params
        
        PG3_ASSERT_VEC3F_NORMALIZED(aDirection);

        return mConstantValue;
    }

protected:

    SpectrumF mConstantValue;
};
