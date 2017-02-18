#pragma once

#include "em_cosine_sampler.hxx"
#include "em_simple_spherical_sampler.hxx"
#include "em_steerable_sampler.hxx"
#include "em_image.hxx"
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
    EnvironmentMap(const std::string aFilename, float aRotate, float aScale, bool aDoBilinFiltering) :
        mDoBilinFiltering(aDoBilinFiltering),
        mCosineSampler(new CosineImageEmSampler()),
        mSimpleSphericalSampler(new SimpleSphericalImageEmSampler()),
        mSteerableSampler(new SteerableImageEmSampler())
    {
        try
        {
            //std::cout << "Loading:   Environment map '" << aFilename << "'" << std::endl;
            mImage.reset(EnvironmentMapImage::LoadImage(aFilename.c_str(), aRotate, aScale));
        }
        catch (...)
        {
            PG3_FATAL_ERROR("Environment map load failed! \"%s\"", aFilename.c_str());
        }

        if (mCosineSampler)
            mCosineSampler->Init(mImage, mDoBilinFiltering);
        if (mSimpleSphericalSampler)
            mSimpleSphericalSampler->Init(mImage, mDoBilinFiltering);
        if (mSteerableSampler)
            mSteerableSampler->Init(mImage, mDoBilinFiltering);
    }

    // Samples direction on unit sphere proportionally to the luminance of the map. 
    PG3_PROFILING_NOINLINE
    bool Sample(
        LightSample     &oLightSample,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const
    {
        mSimpleSphericalSampler->Sample(
            oLightSample, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng);
        mSteerableSampler->Sample(
            oLightSample, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng);
        return mCosineSampler->Sample(
            oLightSample, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng);
    }

    // Gets radiance stored for the given direction and optionally its PDF. The direction
    // must be non-zero but not necessarily normalized.
    PG3_PROFILING_NOINLINE
    void EvalRadiance(
        SpectrumF       &oRadiance,
        const Vec3f     &aDirection, 
        float           *oPdfW = nullptr,
        const Frame     *aSurfFrame = nullptr,
        const bool      *aSampleFrontSide = nullptr,
        const bool      *aSampleBackSide = nullptr
        ) const
    {
        PG3_ASSERT(!aDirection.IsZero());

        const Vec2f uv = Geom::Dir2LatLongFast(aDirection);
        oRadiance = EvalRadiance(uv);

        if (oPdfW && aSurfFrame && aSampleFrontSide && aSampleBackSide)
            *oPdfW = PdfW(aDirection, *aSurfFrame, *aSampleFrontSide, *aSampleBackSide);

        PG3_ASSERT((oPdfW == 0.f) == radiance.IsZero());

        //if (oPdfW == 0.0f)
        //    oRadiance = SpectrumF(0);
    }

    float PdfW(
        const Vec3f     &aDirection,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide) const
    {
        mSimpleSphericalSampler->PdfW(aDirection, aSurfFrame, aSampleFrontSide, aSampleBackSide);
        mSteerableSampler->PdfW(aDirection, aSurfFrame, aSampleFrontSide, aSampleBackSide);
        return mCosineSampler->PdfW(aDirection, aSurfFrame, aSampleFrontSide, aSampleBackSide);
    }

    // Estimate the contribution (irradiance) of the environment map: \int{L_e * f_r * \cos\theta}
    float EstimateIrradiance(
        const Vec3f     &aSurfPt,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const
    {
        // If the sampler can do this for us (and some can), we are done
        float irradianceEst = 0.f;
        if (mCosineSampler->EstimateIrradiance(
                irradianceEst, aSurfPt, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng))
            return irradianceEst;
        if (mSimpleSphericalSampler->EstimateIrradiance(
                irradianceEst, aSurfPt, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng))
            return irradianceEst;
        if (mSteerableSampler->EstimateIrradiance(
                irradianceEst, aSurfPt, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng))
            return irradianceEst;

        // Estimate using MIS Monte Carlo.
        // We need more iterations because the estimate has too high variance if there are 
        // very bright spot lights (e.g. direct sun) under the surface.
        // TODO: This should be done using a pre-computed diffuse map!
        const uint32_t count = 10;
        float sum = 0.f;
        for (uint32_t round = 0; round < count; round++)
        {
            // Strategy 1: Sample the sphere in the cosine-weighted fashion
            LightSample sample1;
            mCosineSampler->Sample(sample1, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng);
            const float pdf1Cos = sample1.mPdfW;
            const float pdf1Sph = 
                mSimpleSphericalSampler->PdfW(sample1.mWig, aSurfFrame, aSampleFrontSide, aSampleBackSide);

            // Strategy 2: Sample the environment map alone
            LightSample sample2;
            mSimpleSphericalSampler->Sample(sample2, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng);
            const float pdf2Sph = sample2.mPdfW;
            const float pdf2Cos =
                mCosineSampler->PdfW(sample2.mWig, aSurfFrame, aSampleFrontSide, aSampleBackSide);

            // Combine the two samples via MIS (balanced heuristics)
            const float part1 =
                sample1.mSample.Luminance()
                / (pdf1Cos + pdf1Sph);
            const float part2 =
                sample2.mSample.Luminance()
                / (pdf2Cos + pdf2Sph);
            sum += part1 + part2;
        }
        irradianceEst = sum / count;
        
        return irradianceEst;
    }

private:

    //// Returns radiance for the given segment of the image
    //SpectrumF EvalRadiance(const Vec2ui &aSegm) const
    //{
    //    PG3_ASSERT(mImage != nullptr);
    //    PG3_ASSERT_INTEGER_IN_RANGE(aSegm.x, 0u, mImage->Width());
    //    PG3_ASSERT_INTEGER_IN_RANGE(aSegm.y, 0u, mImage->Height());

    //    // FIXME: This interface shouldn't be used if bilinear or any smoother filtering is active!

    //    return mImage->ElementAt(aSegm.x, aSegm.y);
    //}

    // Returns radiance for the given lat long coordinates. Optionally does bilinear filtering.
    PG3_PROFILING_NOINLINE 
    SpectrumF EvalRadiance(const Vec2f &aUV) const
    {
        PG3_ASSERT(mImage != nullptr);
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.x, 0.0f, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.y, 0.0f, 1.0f);

        return mImage->Evaluate(aUV, mDoBilinFiltering);
    }

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator
    // explicitly, the compiler may complain about not being able 
    // to create its default implementation.
    EnvironmentMap & operator=(const EnvironmentMap&) = delete;
    //EnvironmentMap(const EnvironmentMap&) = delete;

private:


    std::shared_ptr<EnvironmentMapImage>            mImage;     // Environment image data
    const bool                                      mDoBilinFiltering;

    std::unique_ptr<CosineImageEmSampler>           mCosineSampler;             // TODO: Describe...
    std::unique_ptr<SimpleSphericalImageEmSampler>  mSimpleSphericalSampler;    // TODO: Describe...
    std::unique_ptr<SteerableImageEmSampler>        mSteerableSampler;          // TODO: Describe...
};
