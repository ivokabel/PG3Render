#pragma once

#include "em_cosine_sampler.hxx"
#include "em_simple_spherical_sampler.hxx"
#include "em_steerable_sampler.hxx"
#include "em_image.hxx"
#include "spectrum.hxx"
#include "geom.hxx"
#include "distribution.hxx"
#include "aux_dbg_params.hxx"
#include "types.hxx"
#include "math.hxx"
#include "types.hxx"
#include "debugging.hxx"

///////////////////////////////////////////////////////////////////////////////////////////////////
// EnvironmentMap is adopted from SmallUPBP project and used as a reference for my own 
// implementation. The image-loading code was used directly without almost any significant change.
///////////////////////////////////////////////////////////////////////////////////////////////////

class EnvironmentMap
{
public:
    // Loads an OpenEXR image with an environment map with latitude-longitude mapping.
    EnvironmentMap(
        const std::string    aFilename,
        float                aRotate,
        float                aScale,
        bool                 aDoBilinFiltering,
        const AuxDbgParams  &aAuxDbgParams = AuxDbgParams())
    {
        aAuxDbgParams; // sometimes unused param

        try
        {
            mEmImage.reset(
                EnvironmentMapImage::LoadImage(
                    aFilename.c_str(), aRotate, aScale, aDoBilinFiltering));
        }
        catch (...)
        {
            PG3_FATAL_ERROR("Environment map load failed! \"%s\"", aFilename.c_str());
        }

        mTmpCosineSampler           = std::make_shared<CosineImageEmSampler>();
        mTmpSimpleSphericalSampler  = std::make_shared<SimpleSphericalImageEmSampler>();

        if (mTmpCosineSampler)
            mTmpCosineSampler->Init(mEmImage);
        if (mTmpSimpleSphericalSampler)
            mTmpSimpleSphericalSampler->Init(mEmImage);

#if defined PG3_USE_ENVMAP_SIMPLE_SPHERICAL_SAMPLER
        mSampler = mTmpSimpleSphericalSampler;
#elif defined PG3_USE_ENVMAP_STEERABLE_SAMPLER
        auto steerableSampler =
            std::make_shared<SteerableImageEmSampler>(
                SteerableImageEmSampler::BuildParameters(
                    aAuxDbgParams.float1,
                    aAuxDbgParams.float2,
                    aAuxDbgParams.float3,
                    aAuxDbgParams.float4));
        if (steerableSampler)
            steerableSampler->Init(mEmImage);
        mSampler = steerableSampler;
#else
        mSampler = mTmpCosineSampler;
#endif
    }

    // Samples direction on unit sphere proportionally to the luminance of the map. 
    bool Sample(
        LightSample     &oLightSample,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const
    {
        return mSampler->Sample(
            oLightSample, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng);
    }

    // Gets radiance stored for the given direction and optionally its PDF.
    void EvalRadiance(
        SpectrumF       &oRadiance,
        const Vec3f     &aDirection, 
        float           *oPdfW = nullptr,
        const Frame     *aSurfFrame = nullptr,
        const bool      *aSampleFrontSide = nullptr,
        const bool      *aSampleBackSide = nullptr
        ) const
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aDirection);

        const Vec2f uv = Geom::Dir2LatLong(aDirection);
        oRadiance = EvalRadiance(uv);

        if (oPdfW && aSurfFrame && aSampleFrontSide && aSampleBackSide)
            *oPdfW = PdfW(aDirection, *aSurfFrame, *aSampleFrontSide, *aSampleBackSide);

        PG3_ASSERT_MSG(
               (oPdfW == nullptr) || (aSampleFrontSide == nullptr) || (aSampleBackSide == nullptr)
            || (!*aSampleFrontSide && !*aSampleBackSide)                             // no light sampling needed
            || (!*aSampleFrontSide && (Dot(aDirection, aSurfFrame->Normal()) > 0.f)) // samples in upper not needed
            || (!*aSampleBackSide  && (Dot(aDirection, aSurfFrame->Normal()) < 0.f)) // samples in lower not needed
            || (oRadiance.IsZero() || (*oPdfW > 0.f)),
            "oRadiance.Luminance() %.12f, "
            "oPdfW %.12f",
            oRadiance.Luminance(),
            (oPdfW != nullptr) ? *oPdfW : -1.f);
    }

    float PdfW(
        const Vec3f     &aDirection,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide) const
    {
        return mSampler->PdfW(aDirection, aSurfFrame, aSampleFrontSide, aSampleBackSide);
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
        if (mSampler->EstimateIrradiance(
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
            mTmpCosineSampler->Sample(sample1, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng);
            const float pdf1Cos = sample1.pdfW;
            const float pdf1Sph = 
                mTmpSimpleSphericalSampler->PdfW(sample1.wig, aSurfFrame, aSampleFrontSide, aSampleBackSide);

            // Strategy 2: Sample the environment map alone
            LightSample sample2;
            mTmpSimpleSphericalSampler->Sample(sample2, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng);
            const float pdf2Sph = sample2.pdfW;
            const float pdf2Cos =
                mTmpCosineSampler->PdfW(sample2.wig, aSurfFrame, aSampleFrontSide, aSampleBackSide);

            // Combine the two samples via MIS (balanced heuristics)
            const float part1 =
                  sample1.sample.Luminance()
                / (pdf1Cos + pdf1Sph);
            const float part2 =
                  sample2.sample.Luminance()
                / (pdf2Cos + pdf2Sph);
            sum += part1 + part2;
        }
        irradianceEst = sum / count;
        
        return irradianceEst;
    }

private:

    // Returns radiance for the given lat long coordinates. Optionally does bilinear filtering.
    SpectrumF EvalRadiance(const Vec2f &aUV) const
    {
        PG3_ASSERT(mEmImage != nullptr);
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.x, 0.0f, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.y, 0.0f, 1.0f);

        return mEmImage->Evaluate(aUV);
    }

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator
    // explicitly, the compiler may complain about not being able 
    // to create its default implementation.
    EnvironmentMap & operator=(const EnvironmentMap&) = delete;
    //EnvironmentMap(const EnvironmentMap&) = delete;

private:


    std::shared_ptr<EnvironmentMapImage>        mEmImage; // Environment image data

    std::shared_ptr<ImageEmSampler>             mSampler; // Sampler for usual spherical sampling

    // Temporary samplers for contribution estimation
    std::shared_ptr<ImageEmSampler>             mTmpCosineSampler;
    std::shared_ptr<ImageEmSampler>             mTmpSimpleSphericalSampler;
};
