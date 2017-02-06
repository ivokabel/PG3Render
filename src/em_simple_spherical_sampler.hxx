#pragma once

#include "em_sampler_base.hxx"

//#include "debugging.hxx"
//#include "spectrum.hxx"

class EnvironmentMapSimpleSphericalSampler : public EnvironmentMapSamplerBase
{
public:

    virtual bool Init(
        std::shared_ptr<EnvironmentMapImage>    aEmImage,
        bool                                    aUseBilinearFiltering
        ) override
    {
        aEmImage; aUseBilinearFiltering; // TODO

        return true;
    }

    virtual bool Sample(
        Vec3f           &oSampleDirection,
        float           &oSamplePdf,
        const Vec3f     &aNormal,
        Vec2f            aSample
        ) const override
    {
        aNormal; // unused param

        aSample; oSampleDirection; oSamplePdf; // TODO:

        return false;
    }

    virtual void ReleaseData() override {};
};
