#pragma once

#include "spectrum.hxx"
#include <ImfRgbaFile.h>    // OpenEXR
//#include "pdf.hxx"
#include "debugging.hxx"
#include "math.hxx"
#include "distribution.hxx"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Adopted from SmallUPBP project and used as a reference for my own implementations.
// The image-loading code was used directly without almost any significant change.
///////////////////////////////////////////////////////////////////////////////////////////////////

class InputImage
{
public:
    InputImage(unsigned int aWidth, unsigned int aHeight)
    {
        mWidth  = aWidth;
        mHeight = aHeight;

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

    Spectrum& ElementAt(unsigned int aX, unsigned int aY)
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(aX, 0, mWidth);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(aY, 0, mHeight);

        return mData[mWidth*aY + aX];
    }

    const Spectrum& ElementAt(unsigned int aX, unsigned int aY) const
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(aX, 0, mWidth);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(aY, 0, mHeight);

        return mData[mWidth*aY + aX];
    }

    Spectrum& ElementAt(unsigned int aIdx)
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(aIdx, 0, mWidth*mHeight);

        return mData[aIdx];
    }

    const Spectrum& ElementAt(unsigned int aIdx) const
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(aIdx, 0, mWidth*mHeight);

        return mData[aIdx];
    }

    Vec2ui Size() const
    {
        return Vec2ui(mWidth, mHeight);
    }

    unsigned int Width() const
    {
        return mWidth;
    }

    unsigned int Height() const
    { 
        return mHeight; 
    }

    Spectrum*       mData;
    unsigned int    mWidth;
    unsigned int    mHeight;
};

class EnvironmentMap
{
public:
    // Loads an OpenEXR image with an environment map with latitude-longitude mapping.
    EnvironmentMap(const std::string aFilename, float aRotate, float aScale) :
        mPlan2AngPdfCoeff(1.0f / (2.0f * PI_F * PI_F))
    {
        mImage = NULL;
        mDistribution = NULL;

        try
        {
            std::cout << "Loading:   Environment map '" << aFilename << "'" << std::endl;
            mImage = LoadImage(aFilename.c_str(), aRotate, aScale);
        }
        catch (...)
        {
            PG3_FATAL_ERROR("Environment map load failed!");
        }

        mDistribution = ConvertImageToPdf(mImage);
    }

    ~EnvironmentMap()
    {
        delete(mImage);
        delete(mDistribution);
    }

    // Samples direction on unit sphere proportionally to the luminance of the map. 
    // Returns the sample PDF and optionally it's radiance.
    Vec3f Sample(
        const Vec2f &aSamples,
        float       &oPdfW,
        Spectrum    *oRadiance = NULL,
        float       *oSinMidTheta = NULL
        ) const
    {
        PG3_DEBUG_ASSERT(mImage != NULL);

        const Vec2ui imageSize = mImage->Size();
        Vec2f uv;
        Vec2ui segm;
        float pdfW;

        mDistribution->SampleContinuous(aSamples, uv, segm, &pdfW);
        PG3_DEBUG_ASSERT(pdfW > 0.f);

        const Vec3f direction = LatLong2Dir(uv);

#ifdef PG3_DEBUG_ASSERT_ENABLED
        {
            Vec2f uvdbg = Dir2LatLong(direction);
            PG3_DEBUG_ASSERT_FLOAT_EQUAL(uvdbg.y, uv.y, 1E-4f);
            // the closer we get to the poles, the closer the points are in the x coordinate
            float densityCompensation = uv.y * (1.0f - uv.y);
            float distanceXDirect = fabs(uvdbg.x - uv.x);
            float distanceXOverZero = -distanceXDirect + 1.0f;
            float distanceX = std::min(distanceXDirect, distanceXOverZero) * densityCompensation;
            PG3_DEBUG_ASSERT_FLOAT_LESS_THAN(distanceX, 1E-7f);
        }
#endif

        // Convert the sample's planar PDF over the rectangle [0,1]x[0,1] to 
        // the angular PDF on the unit sphere over the appropriate trapezoid
        //
        // angular pdf = planar pdf * planar segment surf. area / sphere segment surf. area
        //             = planar pdf * (1 / (width*height)) / (2*Pi*Pi*Sin(MidTheta) / (width*height))
        //             = planar pdf / (2*Pi*Pi*Sin(MidTheta))
        //
        // FIXME: Uniform sampling of a segment of the 2D distribution doesn't yield 
        //        uniform sampling of a corresponding segment on a sphere 
        //        - the closer we are to the poles, the denser the sampling will be 
        //        (even though the overall probability of the segment is correct).
        // \int_a^b{1/hdh} = [ln(h)]_a^b = ln(b) - ln(a)
        const float sinMidTheta = SinMidTheta(mImage, segm.y);
        oPdfW = pdfW * mPlan2AngPdfCoeff / sinMidTheta;
        if (oSinMidTheta != NULL)
            *oSinMidTheta = sinMidTheta;

        if (oRadiance)
            *oRadiance = LookupRadiance(segm);

        return direction;
    }

    // Gets radiance stored for the given direction and optionally its PDF. The direction
    // must be non-zero but not necessarily normalized.
    Spectrum Lookup(
        const Vec3f &aDirection, 
        bool         aDoBilinFiltering,
        float       *oPdfW = NULL) const
    {
        PG3_DEBUG_ASSERT(!aDirection.IsZero());

        const Vec3f normDir     = aDirection / aDirection.Length();
        const Vec2f uv          = Dir2LatLong(normDir);
        const Spectrum radiance = LookupRadiance(uv, aDoBilinFiltering);

        oPdfW; // unused parameter
        //if (oPdfW)
        //{
        //    Vec2f uv = Dir2LatLong(normDir);
        //    *oPdfW = mPlan2AngPdfCoeff * mDistribution->Pdf(uv) / SinMidTheta(mImage, uv[1]);
        //    if (*oPdfW == 0.0f) radiance = Spectrum(0);
        //}

        return radiance;
    }

private:
    // Loads, scales and rotates an environment map from an OpenEXR image on the given path.
    InputImage* LoadImage(const char *aFilename, float aRotate, float aScale) const
    {
        aRotate = FmodX(aRotate, 1.0f);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(aRotate, 0.0f, 1.0f);

        Imf::RgbaInputFile file(aFilename, 1);
        Imath::Box2i dw = file.dataWindow();

        int width   = dw.max.x - dw.min.x + 1;
        int height  = dw.max.y - dw.min.y + 1;
        Imf::Rgba* rgbaData = new Imf::Rgba[height*width];

        file.setFrameBuffer(rgbaData - dw.min.x - dw.min.y * width, 1, width);
        file.readPixels(dw.min.y, dw.max.y);

        InputImage* image = new InputImage(width, height);

        int c = 0;
        int iRot = (int)(aRotate * width);
        for (unsigned int j = 0; j < image->Height(); j++)
        {
            for (unsigned int i = 0; i < image->Width(); i++)
            {
                int x = i + iRot;
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

    // Generates a 2D distribution with latitude-longitude mapping 
    // based on the luminance of the provided environment map image
    Distribution2D* ConvertImageToPdf(const InputImage* aImage) const
    {
        // Prepare source distribution data from the original environment map image data, 
        // i.e. convert image values so that the probability of a pixel within 
        // the lattitute-longitude parametrization is equal to the angular probability of 
        // the projected segment on a unit sphere.

        const Vec2ui size   = aImage->Size();
        float *srcData      = new float[size.x * size.y];

        for (unsigned int row = 0; row < size.y; ++row)
        {
            // We compute the relative surface area of the current segment on the unit sphere.
            // We can ommit the height of the segment because it only changes the result 
            // by a multiplication constant and thus doesn't affect the shape of the resulting PDF.
            const float sinAvgTheta = SinMidTheta(mImage, row);

            const unsigned int rowOffset = row * size.x;

            for (unsigned int column = 0; column < size.x; ++column)
            {
                const float luminance = 
                    Luminance(aImage->ElementAt(column, row));
                srcData[rowOffset + column] =
                    sinAvgTheta * luminance;
            }
        }

        return new Distribution2D(srcData, size.x, size.y);
    }

    // Returns direction on unit sphere such that its longitude equals 2*PI*u and its latitude equals PI*v.
    PG3_PROFILING_NOINLINE
    Vec3f LatLong2Dir(const Vec2f &uv) const
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(uv.x, 0.f, 1.f);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(uv.y, 0.f, 1.f);

        const float phi   = -(uv.x - 0.5f) * 2 * PI_F; // we rotate in the opposite direction
        const float theta = uv.y * PI_F;

        const float sinTheta = sin(theta);

        return Vec3f(sinTheta * cos(phi), sinTheta * sin(phi), cos(theta));
    }

    // Returns vector [u,v] in [0,1]x[0,1]. The direction must be non-zero and normalized.
    PG3_PROFILING_NOINLINE
    Vec2f Dir2LatLong(const Vec3f &aDirection) const
    {
        PG3_DEBUG_ASSERT(!aDirection.IsZero() /*(aDirection.Length() == 1.0f)*/);

        // TODO: Use optimized formulae from Jarda's writeout

        const float phi   = -atan2f(aDirection.y, aDirection.x); // we rotate in the opposite direction
        const float theta = acosf(aDirection.z);

        // Convert from [-Pi,Pi] to [0,1]
        //const float uTemp = fmodf(phi * 0.5f * INV_PI_F, 1.0f);
        //const float u = Clamp(uTemp, 0.f, 1.f); // TODO: maybe not necessary
        const float u = Clamp(0.5f + phi * 0.5f * INV_PI_F, 0.f, 1.f);

        // Convert from [0,Pi] to [0,1]
        const float v = Clamp(theta * INV_PI_F, 0.f, 1.0f);

        return Vec2f(u, v);
    }

    // Returns radiance for the given segment of the image
    Spectrum LookupRadiance(const Vec2ui &aSegm) const
    {
        PG3_DEBUG_ASSERT(mImage != NULL);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(aSegm.x, 0u, mImage->Width());
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(aSegm.y, 0u, mImage->Height());

        return mImage->ElementAt(aSegm.x, aSegm.y);
    }

    // Returns radiance for the given lat long coordinates. Optionally does bilinear filtering.
    PG3_PROFILING_NOINLINE 
    Spectrum LookupRadiance(const Vec2f &uv, bool aDoBilinFiltering) const
    {
        PG3_DEBUG_ASSERT(mImage != NULL);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(uv.x, 0.0f, 1.0f);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(uv.y, 0.0f, 1.0f);

        const Vec2ui imageSize = mImage->Size();

        // Convert uv coords to mImage coordinates
        Vec2f xy = uv * Vec2f((float)imageSize.x, (float)imageSize.y);

        return mImage->ElementAt(
            Clamp((unsigned int)xy.x, 0u, imageSize.x - 1),
            Clamp((unsigned int)xy.y, 0u, imageSize.y - 1));

        // TODO: bilinear filtering
        aDoBilinFiltering;








        //int width = mImage->Width();
        //int height = mImage->Height();

        //float xf = u * width;
        //float yf = v * height;

        //int xi1 = Utils::clamp<int>((int)xf, 0, width - 1);
        //int yi1 = Utils::clamp<int>((int)yf, 0, height - 1);

        //int xi2 = xi1 == width - 1 ? xi1 : xi1 + 1;
        //int yi2 = yi1 == height - 1 ? yi1 : yi1 + 1;

        //float tx = xf - (float)xi1;
        //float ty = yf - (float)yi1;

        //return (1 - ty) * ((1 - tx) * mImage->ElementAt(xi1, yi1) + tx * mImage->ElementAt(xi2, yi1))
        //    + ty * ((1 - tx) * mImage->ElementAt(xi1, yi2) + tx * mImage->ElementAt(xi2, yi2));
    }

    // The sine of latitude of the midpoint of the map pixel (a.k.a. segment)
    float SinMidTheta(const InputImage* aImage, const unsigned int aSegmY) const
    {
        PG3_DEBUG_ASSERT(aImage != NULL);

        const unsigned int height = aImage->Height();

        PG3_DEBUG_ASSERT_FLOAT_LESS_THAN(aSegmY, height);

        const float result = sinf(PI_F * (aSegmY + 0.5f) / height);

        PG3_DEBUG_ASSERT(result > 0.f && result <= 1.f);

        return result;
    }

    // The sine of latitude of the midpoint of the map pixel defined by the given v coordinate.
    float SinMidTheta(const InputImage* aImage, const float aV) const
    {
        PG3_DEBUG_ASSERT(aImage != NULL);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(aV, 0.0f, 1.0f);
        PG3_DEBUG_ASSERT_FLOAT_LESS_THAN(aV, 1.0f);

        const unsigned int height   = aImage->Height();
        const unsigned int segment  = std::min((unsigned int)(aV * height), height - 1u);

        return SinMidTheta(aImage, segment);
    }

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator and copy constructor 
    // explicitly, the compiler may complain about not being able 
    // to create default implementations.
    EnvironmentMap & operator=(const EnvironmentMap&) = delete;
    EnvironmentMap(const EnvironmentMap&) = delete;

    InputImage*     mImage;             // Environment map itself
    Distribution2D* mDistribution;      // 2D distribution of the environment map
    const float     mPlan2AngPdfCoeff;  // Coefficient for conversion from planar to angular PDF
};
