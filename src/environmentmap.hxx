#pragma once

#include "environmentmapimage.hxx"
#include "environmentmapsteeringsampler.hxx"
#include "spectrum.hxx"
#include "debugging.hxx"
#include "geom.hxx"
#include "math.hxx"
#include "distribution.hxx"
#include "types.hxx"

///////////////////////////////////////////////////////////////////////////////////////////////////
// EnvironmentMap is adopted from SmallUPBP project and used as a reference for my own 
// implementation. The image-loading code was used directly without almost any significant change.
///////////////////////////////////////////////////////////////////////////////////////////////////

class EnvironmentMap
{
public:
    // Loads an OpenEXR image with an environment map with latitude-longitude mapping.
    EnvironmentMap(const std::string aFilename, float aRotate, float aScale) :
        mImage(nullptr),
        mDistribution(nullptr),
        mSteeringSampler(nullptr),
        mPlan2AngPdfCoeff(1.0f / (2.0f * Math::kPiF * Math::kPiF))
    {
        try
        {
            //std::cout << "Loading:   Environment map '" << aFilename << "'" << std::endl;
            mImage = EnvironmentMapImage::LoadImage(aFilename.c_str(), aRotate, aScale);
        }
        catch (...)
        {
            PG3_FATAL_ERROR("Environment map load failed! \"%s\"", aFilename.c_str());
        }

        mDistribution = GenerateImageDistribution(mImage);
    }

    ~EnvironmentMap()
    {
        delete(mImage);
        delete(mDistribution);
        delete(mSteeringSampler);
    }

    // Samples direction on unit sphere proportionally to the luminance of the map. 
    // Returns the sample PDF and optionally it's radiance.
    PG3_PROFILING_NOINLINE
    Vec3f Sample(
        const Vec2f &aSamples,
        float       &oPdfW,
        SpectrumF   *oRadiance = nullptr,
        float       *oSinMidTheta = nullptr
        ) const
    {
        PG3_ASSERT(mImage != nullptr);

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
        if (oSinMidTheta != nullptr)
            *oSinMidTheta = sinMidTheta;

        if (oRadiance)
            *oRadiance = EvalRadiance(segm);

        return direction;
    }

    // Gets radiance stored for the given direction and optionally its PDF. The direction
    // must be non-zero but not necessarily normalized.
    PG3_PROFILING_NOINLINE
    SpectrumF EvalRadiance(
        const Vec3f &aDirection, 
        bool         aDoBilinFiltering,
        float       *oPdfW = nullptr
        ) const
    {
        PG3_ASSERT(mDistribution != nullptr);
        PG3_ASSERT(!aDirection.IsZero());

        const Vec2f uv              = Dir2LatLong(aDirection);
        const SpectrumF radiance    = EvalRadiance(uv, aDoBilinFiltering);

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
        PG3_ASSERT(mDistribution != nullptr);

        const Vec2f uv = Dir2LatLong(aDirection);
        return mDistribution->Pdf(uv) * mPlan2AngPdfCoeff / SinMidTheta(mImage, uv.y);
    }

private:

    // Generates a 2D distribution with latitude-longitude mapping 
    // based on the luminance of the provided environment map image
    Distribution2D* GenerateImageDistribution(const EnvironmentMapImage* aImage) const
    {
        // Prepare source distribution data from the environment map image data, 
        // i.e. convert image values so that the probability of a pixel within 
        // the lattitute-longitude parametrization is equal to the angular probability of 
        // the projected segment on a unit sphere.

        const Vec2ui size   = aImage->Size();
        float *srcData      = new float[size.x * size.y];

        if (srcData == nullptr)
            return nullptr;

        for (uint32_t row = 0; row < size.y; ++row)
        {
            // We compute the projected surface area of the current segment on the unit sphere.
            // We can ommit the height of the segment because it only changes the result 
            // by a multiplication constant and thus doesn't affect the shape of the resulting PDF.
            const float sinAvgTheta = SinMidTheta(mImage, row);

            const uint32_t rowOffset = row * size.x;

            for (uint32_t column = 0; column < size.x; ++column)
            {
                const float luminance = aImage->ElementAt(column, row).Luminance();
                srcData[rowOffset + column] = sinAvgTheta * luminance;
            }
        }

        Distribution2D* distribution = new Distribution2D(srcData, size.x, size.y);

        delete srcData;

        return distribution;
    }

    // Returns direction on unit sphere such that its longitude equals 2*PI*u and its latitude equals PI*v.
    PG3_PROFILING_NOINLINE
    Vec3f LatLong2Dir(const Vec2f &aUV) const
    {
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.x, 0.f, 1.f);
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.y, 0.f, 1.f);

        const float phi   = -(aUV.x - 0.5f) * 2 * Math::kPiF; // we rotate in the opposite direction
        const float theta = aUV.y * Math::kPiF;

        return Geom::CreateDirection(theta, phi);
    }

    // Returns vector [u,v] in [0,1]x[0,1]. The direction must be non-zero and normalized.
    PG3_PROFILING_NOINLINE
    Vec2f Dir2LatLong(const Vec3f &aDirection) const
    {
        PG3_ASSERT(!aDirection.IsZero() /*(aDirection.Length() == 1.0f)*/);

        // minus sign because we rotate in the opposite direction
        // Math::FastAtan2 is 50 times faster than atan2f at the price of slightly horizontally distorted background
        const float phi     = -Math::FastAtan2(aDirection.y, aDirection.x);
        const float theta   = acosf(aDirection.z);

        // Convert from [-Pi,Pi] to [0,1]
        //const float uTemp = fmodf(phi * 0.5f * Math::kPiInvF, 1.0f);
        //const float u = Math::Clamp(uTemp, 0.f, 1.f); // TODO: maybe not necessary
        const float u = Math::Clamp(0.5f + phi * 0.5f * Math::kPiInvF, 0.f, 1.f);

        // Convert from [0,Pi] to [0,1]
        const float v = Math::Clamp(theta * Math::kPiInvF, 0.f, 1.0f);

        return Vec2f(u, v);
    }

    // Returns radiance for the given segment of the image
    SpectrumF EvalRadiance(const Vec2ui &aSegm) const
    {
        PG3_ASSERT(mImage != nullptr);
        PG3_ASSERT_INTEGER_IN_RANGE(aSegm.x, 0u, mImage->Width());
        PG3_ASSERT_INTEGER_IN_RANGE(aSegm.y, 0u, mImage->Height());

        return mImage->ElementAt(aSegm.x, aSegm.y);
    }

    // Returns radiance for the given lat long coordinates. Optionally does bilinear filtering.
    PG3_PROFILING_NOINLINE 
    SpectrumF EvalRadiance(const Vec2f &aUV, bool aDoBilinFiltering) const
    {
        PG3_ASSERT(mImage != nullptr);
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.x, 0.0f, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.y, 0.0f, 1.0f);

        const Vec2ui imageSize = mImage->Size();

        // Convert uv coords to mImage coordinates
        Vec2f xy = aUV * Vec2f((float)imageSize.x, (float)imageSize.y);

        return mImage->ElementAt(
            Math::Clamp((uint32_t)xy.x, 0u, imageSize.x - 1),
            Math::Clamp((uint32_t)xy.y, 0u, imageSize.y - 1));

        // TODO: bilinear filtering
        aDoBilinFiltering;



        //return (1 - ty) * ((1 - tx) * mImage->ElementAt(xi1, yi1) + tx * mImage->ElementAt(xi2, yi1))
        //    + ty * ((1 - tx) * mImage->ElementAt(xi1, yi2) + tx * mImage->ElementAt(xi2, yi2));
    }

    // The sine of latitude of the midpoint of the map pixel (a.k.a. segment)
    static float SinMidTheta(const EnvironmentMapImage* aImage, const uint32_t aSegmY)
    {
        PG3_ASSERT(aImage != nullptr);

        const uint32_t height = aImage->Height();

        PG3_ASSERT_INTEGER_LESS_THAN(aSegmY, height);

        const float result = sinf(Math::kPiF * (aSegmY + 0.5f) / height);

        PG3_ASSERT(result > 0.f && result <= 1.f);

        return result;
    }

    // The sine of latitude of the midpoint of the map pixel defined by the given v coordinate.
    static float SinMidTheta(const EnvironmentMapImage* aImage, const float aV)
    {
        PG3_ASSERT(aImage != nullptr);
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

    EnvironmentMapImage*            mImage;             // Environment map itself
    EnvironmentMapSteeringSampler*  mSteeringSampler;   // TODO: Describe...
    Distribution2D*                 mDistribution;      // 2D distribution of the environment map
    const float                     mPlan2AngPdfCoeff;  // Coefficient for conversion from planar to angular PDF
};
