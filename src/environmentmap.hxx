#pragma once

#include "spectrum.hxx"
#include <ImfRgbaFile.h>    // OpenEXR
//#include "pdf.hxx"
#include "debugging.hxx"
#include "math.hxx"
#include "distribution.hxx"
#include "types.hxx"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Adopted from SmallUPBP project and used as a reference for my own implementations.
// The image-loading code was used directly without almost any significant change.
///////////////////////////////////////////////////////////////////////////////////////////////////

class InputImage
{
public:
    InputImage(uint32_t aWidth, uint32_t aHeight)
    {
        mWidth  = aWidth;
        mHeight = aHeight;

        mData = new SpectrumF[mWidth * mHeight];
    }

    ~InputImage()
    {
        if (mData)
            delete mData;
        mData   = NULL;
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
            //std::cout << "Loading:   Environment map '" << aFilename << "'" << std::endl;
            mImage = LoadImage(aFilename.c_str(), aRotate, aScale);
        }
        catch (...)
        {
            PG3_FATAL_ERROR("Environment map load failed! \"%s\"", aFilename.c_str());
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
    PG3_PROFILING_NOINLINE
    Vec3f Sample(
        const Vec2f &aSamples,
        float       &oPdfW,
        SpectrumF   *oRadiance = NULL,
        float       *oSinMidTheta = NULL
        ) const
    {
        PG3_ASSERT(mImage != NULL);

        const Vec2ui imageSize = mImage->Size();
        Vec2f uv;
        Vec2ui segm;
        float pdf;

        mDistribution->SampleContinuous(aSamples, uv, segm, &pdf);
        PG3_ASSERT(pdf > 0.f);

        const Vec3f direction = LatLong2Dir(uv);

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
        oPdfW = pdf * mPlan2AngPdfCoeff / sinMidTheta;
        if (oSinMidTheta != NULL)
            *oSinMidTheta = sinMidTheta;

        if (oRadiance)
            *oRadiance = LookupRadiance(segm);

        return direction;
    }

    // Gets radiance stored for the given direction and optionally its PDF. The direction
    // must be non-zero but not necessarily normalized.
    PG3_PROFILING_NOINLINE
    SpectrumF Lookup(
        const Vec3f &aDirection, 
        bool         aDoBilinFiltering,
        float       *oPdfW = NULL
        ) const
    {
        PG3_ASSERT(mDistribution != NULL);
        PG3_ASSERT(!aDirection.IsZero());

        const Vec2f uv              = Dir2LatLong(aDirection);
        const SpectrumF radiance    = LookupRadiance(uv, aDoBilinFiltering);

        if (oPdfW)
        {
            *oPdfW = mDistribution->Pdf(uv) * mPlan2AngPdfCoeff / SinMidTheta(mImage, uv.y);

            PG3_ASSERT((*oPdfW == 0.f) == radiance.IsZero());

            //if (*oPdfW == 0.0f)
            //    radiance = SpectrumF(0);
        }

        return radiance;
    }

    float PdfW(const Vec3f &aDirection) const
    {
        PG3_ASSERT(mDistribution != NULL);

        const Vec2f uv = Dir2LatLong(aDirection);
        return mDistribution->Pdf(uv) * mPlan2AngPdfCoeff / SinMidTheta(mImage, uv.y);
    }

private:
    // Loads, scales and rotates an environment map from an OpenEXR image on the given path.
    InputImage* LoadImage(const char *aFilename, float aRotate, float aScale) const
    {
        aRotate = FmodX(aRotate, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(aRotate, 0.0f, 1.0f);

        Imf::RgbaInputFile file(aFilename, 1);
        Imath::Box2i dw = file.dataWindow();

        int32_t width   = dw.max.x - dw.min.x + 1;
        int32_t height  = dw.max.y - dw.min.y + 1;
        Imf::Rgba* rgbaData = new Imf::Rgba[height*width];

        file.setFrameBuffer(rgbaData - dw.min.x - dw.min.y * width, 1, width);
        file.readPixels(dw.min.y, dw.max.y);

        InputImage* image = new InputImage(width, height);

        int32_t c = 0;
        int32_t iRot = (int32_t)(aRotate * width);
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

        for (uint32_t row = 0; row < size.y; ++row)
        {
            // We compute the relative surface area of the current segment on the unit sphere.
            // We can ommit the height of the segment because it only changes the result 
            // by a multiplication constant and thus doesn't affect the shape of the resulting PDF.
            const float sinAvgTheta = SinMidTheta(mImage, row);

            const uint32_t rowOffset = row * size.x;

            for (uint32_t column = 0; column < size.x; ++column)
            {
                const float luminance =
                    aImage->ElementAt(column, row).Luminance();
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
        PG3_ASSERT_FLOAT_IN_RANGE(uv.x, 0.f, 1.f);
        PG3_ASSERT_FLOAT_IN_RANGE(uv.y, 0.f, 1.f);

        const float phi   = -(uv.x - 0.5f) * 2 * PI_F; // we rotate in the opposite direction
        const float theta = uv.y * PI_F;

        const float sinTheta = sin(theta);

        return Vec3f(sinTheta * cos(phi), sinTheta * sin(phi), cos(theta));
    }

    // Returns vector [u,v] in [0,1]x[0,1]. The direction must be non-zero and normalized.
    PG3_PROFILING_NOINLINE
    Vec2f Dir2LatLong(const Vec3f &aDirection) const
    {
        PG3_ASSERT(!aDirection.IsZero() /*(aDirection.Length() == 1.0f)*/);

        // minus sign because we rotate in the opposite direction
        // fast_atan2f is 50 times faster than atan2f at the price of slightly horizontally distorted background
        const float phi     = -fast_atan2f(aDirection.y, aDirection.x);
        const float theta   = acosf(aDirection.z);

        // Convert from [-Pi,Pi] to [0,1]
        //const float uTemp = fmodf(phi * 0.5f * INV_PI_F, 1.0f);
        //const float u = Clamp(uTemp, 0.f, 1.f); // TODO: maybe not necessary
        const float u = Clamp(0.5f + phi * 0.5f * INV_PI_F, 0.f, 1.f);

        // Convert from [0,Pi] to [0,1]
        const float v = Clamp(theta * INV_PI_F, 0.f, 1.0f);

        return Vec2f(u, v);
    }

    // Returns radiance for the given segment of the image
    SpectrumF LookupRadiance(const Vec2ui &aSegm) const
    {
        PG3_ASSERT(mImage != NULL);
        PG3_ASSERT_INTEGER_IN_RANGE(aSegm.x, 0u, mImage->Width());
        PG3_ASSERT_INTEGER_IN_RANGE(aSegm.y, 0u, mImage->Height());

        return mImage->ElementAt(aSegm.x, aSegm.y);
    }

    // Returns radiance for the given lat long coordinates. Optionally does bilinear filtering.
    PG3_PROFILING_NOINLINE 
    SpectrumF LookupRadiance(const Vec2f &uv, bool aDoBilinFiltering) const
    {
        PG3_ASSERT(mImage != NULL);
        PG3_ASSERT_FLOAT_IN_RANGE(uv.x, 0.0f, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(uv.y, 0.0f, 1.0f);

        const Vec2ui imageSize = mImage->Size();

        // Convert uv coords to mImage coordinates
        Vec2f xy = uv * Vec2f((float)imageSize.x, (float)imageSize.y);

        return mImage->ElementAt(
            Clamp((uint32_t)xy.x, 0u, imageSize.x - 1),
            Clamp((uint32_t)xy.y, 0u, imageSize.y - 1));

        // TODO: bilinear filtering
        aDoBilinFiltering;








        //int32_t width = mImage->Width();
        //int32_t height = mImage->Height();

        //float xf = u * width;
        //float yf = v * height;

        //int32_t xi1 = Utils::clamp<int32_t>((int32_t)xf, 0, width - 1);
        //int32_t yi1 = Utils::clamp<int32_t>((int32_t)yf, 0, height - 1);

        //int32_t xi2 = xi1 == width - 1 ? xi1 : xi1 + 1;
        //int32_t yi2 = yi1 == height - 1 ? yi1 : yi1 + 1;

        //float tx = xf - (float)xi1;
        //float ty = yf - (float)yi1;

        //return (1 - ty) * ((1 - tx) * mImage->ElementAt(xi1, yi1) + tx * mImage->ElementAt(xi2, yi1))
        //    + ty * ((1 - tx) * mImage->ElementAt(xi1, yi2) + tx * mImage->ElementAt(xi2, yi2));
    }

    // The sine of latitude of the midpoint of the map pixel (a.k.a. segment)
    float SinMidTheta(const InputImage* aImage, const uint32_t aSegmY) const
    {
        PG3_ASSERT(aImage != NULL);

        const uint32_t height = aImage->Height();

        PG3_ASSERT_FLOAT_LESS_THAN(aSegmY, height);

        const float result = sinf(PI_F * (aSegmY + 0.5f) / height);

        PG3_ASSERT(result > 0.f && result <= 1.f);

        return result;
    }

    // The sine of latitude of the midpoint of the map pixel defined by the given v coordinate.
    float SinMidTheta(const InputImage* aImage, const float aV) const
    {
        PG3_ASSERT(aImage != NULL);
        PG3_ASSERT_FLOAT_IN_RANGE(aV, 0.0f, 1.0f);

        const uint32_t height   = aImage->Height();
        const uint32_t segment  = std::min((uint32_t)(aV * height), height - 1u);

        return SinMidTheta(aImage, segment);
    }

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator and copy constructor 
    // explicitly, the compiler may complain about not being able 
    // to create their default implementations.
    EnvironmentMap & operator=(const EnvironmentMap&) = delete;
    EnvironmentMap(const EnvironmentMap&) = delete;

    InputImage*     mImage;             // Environment map itself
    Distribution2D* mDistribution;      // 2D distribution of the environment map
    const float     mPlan2AngPdfCoeff;  // Coefficient for conversion from planar to angular PDF
};
