#pragma once

#include "em_sampler_base.hxx"

//#include "debugging.hxx"
//#include "spectrum.hxx"

class EnvironmentMapCosineSampler : public EnvironmentMapSamplerBase
{
public:

    virtual bool Init(
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering
        ) override
    {
        aEmImage; aUseBilinearFiltering; // unused parameters

        return true;
    }

    virtual bool Sample(
        Vec3f           &oSampleDirection,
        float           &oSamplePdf,
        const Vec3f     &aNormal,
        Vec2f            aSample
        ) const override
    {
        aNormal; aSample; oSampleDirection; oSamplePdf; // TODO

        return false;
    }
};
