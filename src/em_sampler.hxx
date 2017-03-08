#pragma once

#include "em_image.hxx"
#include "light_sample.hxx"
#include "rng.hxx"
#include "spectrum.hxx"
#include "types.hxx"

#include <memory>
#include <limits>

template <typename TEmValues>
class EnvironmentMapSampler
{
public:

    virtual ~EnvironmentMapSampler()
    {
        ReleaseData();
    }


    // Builds needed internal data structures
    virtual bool Init(std::shared_ptr<TEmValues> aEmImage)
    {
        ReleaseData();

        if (!aEmImage)
            return false;

        mEmImage = aEmImage;

        return true;
    }


    // Generates random direction, PDF and EM value
    virtual bool SampleImpl(
        Vec3f           &oDirGlobal,
        float           &oPdfW,
        SpectrumF       &oRadianceCos, // radiance * abs(cos(thetaIn)
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const = 0;


    // Generates random direction, PDF and EM value in form of a LightSample
    virtual bool Sample(
        LightSample     &oLightSample,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const
    {
        PG3_ASSERT(mEmImage != nullptr);

        oLightSample.mLightProbability = 1.0f;
        oLightSample.mDist             = std::numeric_limits<float>::max();

        return SampleImpl(
            oLightSample.mWig, oLightSample.mPdfW, oLightSample.mSample,
            aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng);
    }


    virtual float PdfW(
        const Vec3f     &aDirection,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide) const = 0;


    // Optionally estimates the incomming irradiance for the given configuration:
    //      \int{L_e * f_r * \cos\theta}
    virtual bool EstimateIrradiance(
        float           &oIrradianceEstimate,
        const Vec3f     &aSurfPt,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const
    {
        oIrradianceEstimate, aSurfPt, aSurfFrame, aSampleFrontSide, aSampleBackSide, aRng; // unused params
        return false;
    }


    // Releases all data structures.
    // Can be called explicitly if necessary. Is called automatically in Init() and in Destructor.
    virtual void ReleaseData()
    {
        mEmImage.reset();
    }

protected:

    std::shared_ptr<TEmValues>  mEmImage;
};

typedef EnvironmentMapSampler<EnvironmentMapImage>    ImageEmSampler;
typedef EnvironmentMapSampler<ConstEnvironmentValue>  ConstantEmSampler;
