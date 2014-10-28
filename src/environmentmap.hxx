#pragma once

#include "spectrum.hxx"
#include <ImfRgbaFile.h>    // OpenEXR
//#include "pdf.hxx"
#include "debugging.hxx"
#include "math.hxx"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Adopted from SmallUPBP project and used as a reference for my own implementations.
// The image-loading code was used directly without almost any significant change.
///////////////////////////////////////////////////////////////////////////////////////////////////

class InputImage
{
public:
    InputImage(int width, int height)
    {
        mWidth  = width;
        mHeight = height;

        mData = new Spectrum[mWidth * mHeight];
    }

    ~InputImage()
    {
        if (mData)
            delete mData;
        mData   = NULL;
        mWidth  = 0;
        mHeight = 0;
    }

    Spectrum& ElementAt(int x, int y)
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(x, 0, mWidth);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(y, 0, mHeight);

        return mData[mWidth*y + x];
    }

    const Spectrum& ElementAt(int x, int y) const
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(x, 0, mWidth);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(y, 0, mHeight);

        return mData[mWidth*y + x];
    }

    Spectrum& ElementAt(int idx)
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(idx, 0, mWidth*mHeight);

        return mData[idx];
    }

    const Spectrum& ElementAt(int idx) const
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(idx, 0, mWidth*mHeight);

        return mData[idx];
    }

    Vec2i Size() const
    {
        return Vec2i(mWidth, mHeight);
    }

    int Width() const
    {
        return mWidth;
    }

    int Height() const 
    { 
        return mHeight; 
    }

    Spectrum*   mData;
    int         mWidth;
    int         mHeight;
};

class EnvironmentMap
{
public:
    // Expects absolute path of an OpenEXR file with an environment map with latitude-longitude mapping.
    EnvironmentMap(const std::string filename, float rotate, float scale)
    {
        mImage = 0;
        //mDistribution = 0;
        //mNorm = 0.5f * INV_PI_F * INV_PI_F;

        try
        {
            std::cout << "Loading:   Environment map '" << filename << "'" << std::endl;
            mImage = LoadImage(filename.c_str(), rotate, scale);
        }
        catch (...)
        {
            PG3_FATAL_ERROR("Environment map load failed!");
        }

        //mDistribution = ConvertImageToPdf(mImage);
    }

    ~EnvironmentMap()
    {
        delete(mImage);
        //delete(mDistribution);
    }

    // Samples direction on unit sphere proportionally to the luminance of the map. Returns its PDF and optionally radiance.
    //Vec3f Sample(
    //    const Vec2f &aSamples,
    //    float       &oPdfW,
    //    Spectrum         *oRadiance = NULL) const
    //{
    //    float uv[2]; float pdf;
    //    mDistribution->SampleContinuous(aSamples[0], aSamples[1], uv, &pdf);

    //    PG3_DEBUG_ASSERT(pdf > 0);

    //    oPdfW = mNorm * pdf / sinTheta(uv[1], mImage->Height());

    //    Vec3f direction = LatLong2Dir(uv[0], uv[1]);

    //    if (oRadiance)
    //        *oRadiance = LookupRadiance(uv, mImage);

    //    return direction;
    //}

    // Gets radiance stored for the given direction and optionally its PDF. The direction
    // must be non-zero but not necessarily normalized.
    Spectrum Lookup(
        const Vec3f &aDirection,
        float       *oPdfW = NULL) const
    {
        PG3_DEBUG_ASSERT(!aDirection.IsZero());

        const Vec3f normDir     = aDirection / aDirection.Length();
        const Vec2f uv          = Dir2LatLong(normDir);
        const Spectrum radiance = LookupRadiance(uv, mImage);

        oPdfW; // unused parameter

        //if (oPdfW)
        //{
        //    //Vec2f uv = Dir2LatLong(normDir);
        //    *oPdfW = mNorm * mDistribution->Pdf(uv[0], uv[1]) / sinTheta(uv[1], mImage->Height());
        //    if (*oPdfW == 0.0f) radiance = Spectrum(0);
        //}

        return radiance;
    }

private:
    // Loads, scales and rotates an environment map from an OpenEXR image on the given path.
    InputImage* LoadImage(const char *filename, float rotate, float scale) const
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(rotate, 0.0f, 1.0f);

        Imf::RgbaInputFile file(filename, 1);
        Imath::Box2i dw = file.dataWindow();

        int width   = dw.max.x - dw.min.x + 1;
        int height  = dw.max.y - dw.min.y + 1;
        Imf::Rgba* rgbaData = new Imf::Rgba[height*width];

        file.setFrameBuffer(rgbaData - dw.min.x - dw.min.y * width, 1, width);
        file.readPixels(dw.min.y, dw.max.y);

        InputImage* image = new InputImage(width, height);

        int c = 0;
        int iRot = (int)(rotate * width);
        for (int j = 0; j < image->Height(); j++)
        {
            for (int i = 0; i < image->Width(); i++) 
            {
                int x = i + iRot;
                if (x >= width) x -= width;
                image->ElementAt(x, j).SetSRGBLight(
                    rgbaData[c].r * scale,
                    rgbaData[c].g * scale,
                    rgbaData[c].b * scale
                    );
                c++;
            }
        }

        delete rgbaData;

        return image;
    }

    // Converts luminance of the given environment map to 2D distribution with latitude-longitude mapping.
    //Distribution2D* ConvertImageToPdf(const InputImage* image) const
    //{
    //    int height = image->Height();
    //    int width = height + height; // height maps to PI, width maps to 2PI		

    //    float *data = new float[width * height];

    //    for (int r = 0; r < height; ++r)
    //    {
    //        float v = (float)(r + 0.5f) / (float)height;
    //        float sinTheta = sinf(PI_F * v);
    //        int colOffset = r * width;

    //        for (int c = 0; c < width; ++c)
    //        {
    //            float u = (float)(c + 0.5f) / (float)width;
    //            data[c + colOffset] = sinTheta * Luminance(image->ElementAt(c, r));
    //        }
    //    }

    //    return new Distribution2D(data, width, height);
    //}

    // Returns direction on unit sphere such that its longitude equals 2*PI*u and its latitude equals PI*v.
    //Vec3f LatLong2Dir(float u, float v) const
    //{
    //    float phi = u * 2 * PI_F;
    //    float theta = v * PI_F;

    //    float sinTheta = sin(theta);

    //    return Vec3f(-sinTheta * cos(phi), sinTheta * sin(phi), cos(theta));
    //}

    // Returns vector [u,v] in [0,1]x[0,1]. The direction must be non-zero and normalized.
    Vec2f Dir2LatLong(const Vec3f &aDirection) const
    {
        PG3_DEBUG_ASSERT(!aDirection.IsZero() /*(aDirection.Length() == 1.0f)*/);

        const float phi   = atan2f(aDirection.x, aDirection.y);
        const float theta = acosf(aDirection.z);

        // Convert from [-Pi,Pi] to [0,1]
        //const float uTemp = fmodf(phi * 0.5f * INV_PI_F, 1.0f);
        //const float u = Clamp(uTemp, 0.f, 1.f); // TODO: maybe not necessary
        const float u = Clamp(0.5f + phi * 0.5f * INV_PI_F, 0.f, 1.f);

        // Convert from [0,Pi] to [0,1]
        const float v = Clamp(theta * INV_PI_F, 0.f, 1.0f);

        return Vec2f(u, v);

        //float phi = (aDirection.x() != 0 || aDirection.y() != 0) ? atan2f(aDirection.y(), aDirection.x()) : 0;
        //float theta = acosf(aDirection.z());
        //float u = Utils::clamp<float>(0.5 - phi * 0.5f * INV_PI_F, 0, 1);
        //float v = Utils::clamp<float>(theta * INV_PI_F, 0, 1);
    }

    // Returns radiance for the given lat long coordinates. Does bilinear filtering.
    Spectrum LookupRadiance(const Vec2f &uv, const InputImage* image) const
    {
        //int width  = image->Width();
        //int height = image->Height();
        const Vec2i imageSize = image->Size();

        // Convert uv coords to image coordinates
        Vec2f xy = uv * Vec2f((float)(imageSize.x - 1), (float)(imageSize.y - 1));

        return image->ElementAt(
            Clamp((int)xy.x, 0, imageSize.x - 1),
            Clamp((int)xy.y, 0, imageSize.y - 1));

        // TODO: bilinear filtering?














        //int width = image->Width();
        //int height = image->Height();

        //float xf = u * width;
        //float yf = v * height;

        //int xi1 = Utils::clamp<int>((int)xf, 0, width - 1);
        //int yi1 = Utils::clamp<int>((int)yf, 0, height - 1);

        //int xi2 = xi1 == width - 1 ? xi1 : xi1 + 1;
        //int yi2 = yi1 == height - 1 ? yi1 : yi1 + 1;

        //float tx = xf - (float)xi1;
        //float ty = yf - (float)yi1;

        //return (1 - ty) * ((1 - tx) * image->ElementAt(xi1, yi1) + tx * image->ElementAt(xi2, yi1))
        //    + ty * ((1 - tx) * image->ElementAt(xi1, yi2) + tx * image->ElementAt(xi2, yi2));
    }

    // Returns sine of latitude for a midpoint of a pixel in a map of the given height corresponding to v. Never returns zero.
    //float sinTheta(const float v, const float height) const
    //{
    //    float result;

    //    if (v < 1)
    //        result = sinf(PI_F * (float)((int)(v * height) + 0.5f) / (float)height);
    //    else
    //        result = sinf(PI_F * (float)((height - 1) + 0.5f) / (float)height);

    //    PG3_DEBUG_ASSERT(result > 0 && result <= 1);

    //    return result;

    //    return 0.f;
    //}

    InputImage* mImage;				    // The environment map
    //Distribution2D* mDistribution;	// Environment map converted to 2D distribution	
    //float mNorm;					// PDF normalization factor
};
