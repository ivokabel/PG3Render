#pragma once

#include "em_sampler.hxx"
#include "debugging.hxx"

// Samples requested (hemi)sphere(s) in a cosine-weighted fashion.
// Ignores the environment map completely.
template <typename TEmValues>
class EnvironmentMapCosineSampler : public EnvironmentMapSampler<TEmValues>
{
public:

    virtual bool Sample(
        Vec3f           &oDirection,
        float           &oPdfW,
        SpectrumF       &oRadianceCos, // radiance * abs(cos(thetaIn)
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const override
    {
        const Vec3f wil =
            Sampling::SampleCosSphereParamPdfW(
                aRng.GetVec3f(), aSampleFrontSide, aSampleBackSide, oPdfW);
        oDirection = aSurfFrame.ToWorld(wil);

        const SpectrumF radiance = mEmImage->Evaluate(oDirection, mEmUseBilinearFiltering);
        const float cosThetaIn = std::abs(wil.z);
        oRadianceCos = radiance * cosThetaIn;

        return true;
    }
};

typedef EnvironmentMapCosineSampler<EnvironmentMapImage>    CosineImageEmSampler;
typedef EnvironmentMapCosineSampler<ConstEnvironmentValue>  CosineConstEmSampler;
