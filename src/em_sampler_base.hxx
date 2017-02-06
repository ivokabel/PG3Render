#pragma once

#include "em_image.hxx"
#include "types.hxx"

class EnvironmentMapSamplerBase
{
public:

    virtual bool Init(
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering
        ) = 0;

    virtual bool Sample(
        Vec3f           &oSampleDirection,
        float           &oSamplePdf,
        const Vec3f     &aNormal,
        Vec2f            aSample
        ) const = 0;
};
