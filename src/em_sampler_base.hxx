#pragma once

#include "rng.hxx"
#include "em_image.hxx"
#include "light_sample.hxx"
#include "spectrum.hxx"
#include "types.hxx"
#include <memory>
#include <limits>

class EnvironmentMapSamplerBase
{
public:

    // ...
    virtual bool Init(
        std::shared_ptr<EnvironmentMapImage>     aEmImage,
        bool                                     aUseBilinearFiltering) = 0;

    // ...
    virtual bool Sample(
        Vec3f           &oSampleDirection,
        float           &oSamplePdf,
        SpectrumF       &oRadianceCos, // radiance * abs(cos(thetaIn)
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const = 0;

    // ...
    bool Sample(
        LightSample     &oLightSample,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng)
    {
        oLightSample.mLightProbability  = 1.0f;
        oLightSample.mDist              = std::numeric_limits<float>::max();

        return Sample(
            oLightSample.mWig, oLightSample.mPdfW, oLightSample.mSample,
            aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng);
    }

    // Releases all data structures
    virtual void ReleaseData() = 0;
};
