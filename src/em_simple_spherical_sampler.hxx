#pragma once

#include "em_sampler.hxx"
#include "debugging.hxx"

template <typename TEmValues>
class EnvironmentMapSimpleSphericalSampler : public EnvironmentMapSampler<TEmValues>
{
public:

    virtual bool Init(
        std::shared_ptr<EnvironmentMapImage>    aEmImage,
        bool                                    aUseBilinearFiltering) override
    {
        if (!EnvironmentMapSampler::Init(aEmImage, aUseBilinearFiltering))
            return false;

        // TODO: Distribution...

        return false; // debug
    }


    virtual bool Sample(
        Vec3f           &oDirection,
        float           &oPdfW,
        SpectrumF       &oRadianceCos, // radiance * abs(cos(thetaIn)
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const override
    {
        aSurfFrame; // unused param

        aSampleFrontSide; aSampleBackSide; aRng; // TODO
        oSampleDirection; oSamplePdf; oRadianceCos; // TODO

        Vec2f sample = aRng.GetVec2f();

        PG3_ASSERT_FLOAT_IN_RANGE(sample.x, 0.0f, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(sample.y, 0.0f, 1.0f);

        return false;
    }


    virtual void ReleaseData() override
    {
        // TODO: Distribution...

        EnvironmentMapSampler::ReleaseData();
    };
};

typedef EnvironmentMapSimpleSphericalSampler<EnvironmentMapImage>   SimpleSphericalImageEmSampler;
typedef EnvironmentMapSimpleSphericalSampler<ConstEnvironmentValue> SimpleSphericalConstEmSampler;
