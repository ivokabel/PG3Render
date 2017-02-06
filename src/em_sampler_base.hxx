#pragma once

#include "em_image.hxx"
#include "types.hxx"
#include <memory>

class EnvironmentMapSamplerBase
{
public:

    // ...
    virtual bool Init(
        std::shared_ptr<EnvironmentMapImage>     aEmImage,
        bool                                     aUseBilinearFiltering
        ) = 0;

    // ...
    virtual bool Sample(
        Vec3f           &oSampleDirection,
        float           &oSamplePdf,
        const Vec3f     &aNormal,
        Vec2f            aSample
        ) const = 0;

    // Releases all data structures
    virtual void ReleaseData() = 0;
};
