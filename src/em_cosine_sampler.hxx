#pragma once

#include "em_sampler.hxx"
#include "debugging.hxx"

// Samples requested hemi-sphere(s) in a cosine-weighted fashion.
// Ignores the environment map completely.
template <typename TEmValues>
class EnvironmentMapCosineSampler : public EnvironmentMapSampler<TEmValues>
{
public:

    virtual bool SampleImpl(
        Vec3f           &oDirGlobal,
        float           &oPdfW,
        SpectrumF       &oRadianceCos, // radiance * abs(cos(thetaIn)
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const override
    {
        if (!mEmImage)
            return false;

        const Vec3f wil =
            Sampling::SampleCosSphereParamPdfW(
                aRng.GetVec3f(), aSampleFrontSide, aSampleBackSide, oPdfW);
        oDirGlobal = aSurfFrame.ToWorld(wil);

        const SpectrumF radiance = mEmImage->Evaluate(oDirGlobal);
        const float cosThetaIn = std::abs(wil.z);
        oRadianceCos = radiance * cosThetaIn;

        return true;
    }


    virtual float PdfW(
        const Vec3f     &aDirection,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide) const override
    {
        const Vec3f &dirLocal = aSurfFrame.ToLocal(aDirection);
        const float pdfW = Sampling::CosSpherePdfW(aSampleFrontSide, aSampleBackSide, dirLocal);
        return pdfW;
    }
};

typedef EnvironmentMapCosineSampler<EnvironmentMapImage>    CosineImageEmSampler;
typedef EnvironmentMapCosineSampler<ConstEnvironmentValue>  CosineConstEmSampler;
