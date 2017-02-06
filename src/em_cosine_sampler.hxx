#pragma once

#include "em_sampler_base.hxx"
#include "debugging.hxx"

class EnvironmentMapCosineSampler : public EnvironmentMapSamplerBase
{
public:

    virtual bool Sample(
        Vec3f           &oSampleDirection,
        float           &oSamplePdf,
        SpectrumF       &oRadianceCos, // radiance * abs(cos(thetaIn)
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const override
    {
        aSurfFrame; aSampleFrontSide; aSampleBackSide; aRng; // TODO
        oSampleDirection, oSamplePdf, oRadianceCos; // TODO

        Vec2f sample = aRng.GetVec2f();

        PG3_ASSERT_FLOAT_IN_RANGE(sample.x, 0.0f, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(sample.y, 0.0f, 1.0f);

        // TODO: ...

        return false;
    }
};
