#pragma once

#include "em_cosine_sampler.hxx"

#include "light_sample.hxx"
#include "materials.hxx"
#include "math.hxx"
#include "spectrum.hxx"
#include "rng.hxx"
#include "em.hxx"
#include "types.hxx"
#include "hard_config.hxx"

#include <vector>
#include <cmath>
#include <cassert>

//////////////////////////////////////////////////////////////////////////
class AbstractLight
{
public:

    virtual ~AbstractLight() {};

    // Used in MC estimator of the planar version of the rendering equation. For a randomly sampled 
    // point on the light source surface it computes: outgoing radiance * geometric component
    virtual void SampleIllumination(
        const Vec3f             &aSurfPt,       // TODO: Shaded point data should be wrapped in a structure
        const Frame             &aSurfFrame,
        const AbstractMaterial  &aSurfMaterial,
        Rng                     &aRng,
        LightSample             &oSample) const = 0;

    // Returns amount of outgoing radiance from the point in the direction
    virtual SpectrumF GetEmmision(
        const Vec3f             &aLightPt,
        const Vec3f             &aWol,
        const Vec3f             &aSurfPt,
              float             *oPdfW = nullptr,
        const Frame             *aSurfFrame = nullptr,
        const AbstractMaterial  *aSurfMaterial = nullptr) const = 0;

    // Returns an estimate of light contribution of this light-source to the given point.
    // Used for picking one of all available light sources when doing light-source sampling.
    virtual float EstimateContribution(
        const Vec3f             &aSurfPt,
        const Frame             &aSurfFrame,
        const AbstractMaterial  &aSurfMaterial,
              Rng               &aRng) const = 0;
};

//////////////////////////////////////////////////////////////////////////
class AreaLight : public AbstractLight
{
public:

    AreaLight(
        const Vec3f &aP0,
        const Vec3f &aP1,
        const Vec3f &aP2)
    {
        mRadiance.MakeZero();

        mP0 = aP0;
        mE1 = aP1 - aP0;
        mE2 = aP2 - aP0;

        Vec3f normal = Cross(mE1, mE2);
        float len    = normal.Length();
        mArea        = len / 2.f;
        mInvArea     = 2.f / len;
        mFrame.SetFromZ(normal.Normalize());
    }

    // Returns amount of outgoing radiance from the point in the direction
    virtual SpectrumF GetEmmision(
        const Vec3f             &aLightPt,
        const Vec3f             &aWol,
        const Vec3f             &aSurfPt,
              float             *oPdfW = nullptr,
        const Frame             *aSurfFrame = nullptr,
        const AbstractMaterial  *aSurfMaterial = nullptr) const override
    {
        aSurfFrame, aSurfMaterial; // unused params

        // We don't check the point since we expect it to be within the light surface

        if (oPdfW != nullptr)
        {
            // Angular PDF. We use low epsilon boundary to avoid division by very small PDFs.
            // Replicated in ComputeSample()!
            Vec3f wig = aLightPt - aSurfPt;
            const float distSqr = wig.LenSqr();
            wig /= sqrt(distSqr);
            const float absCosThetaOut = abs(Dot(mFrame.Normal(), wig));
            *oPdfW = std::max(mInvArea * (distSqr / absCosThetaOut), Geom::kEpsDist);
        }

        if (aWol.z <= 0.)
            return SpectrumF().MakeZero();
        else
            return mRadiance;
    };

    virtual void SetPower(const SpectrumF& aPower)
    {
        // Radiance = Flux/(Pi*Area)  [W * sr^-1 * m^2]

        mRadiance = aPower * (mInvArea / Math::kPiF);
    }

    virtual void SampleIllumination(
        const Vec3f             &aSurfPt,
        const Frame             &aSurfFrame,
        const AbstractMaterial  &aSurfMaterial,
        Rng                     &aRng,
        LightSample             &oSample
        ) const override
    {
        // Sample the whole triangle surface uniformly
        const Vec2f rnd = aRng.GetVec2f();
        const Vec3f P1 = mP0 + mE1;
        const Vec3f P2 = mP0 + mE2;
        const Vec3f samplePoint = Sampling::SampleUniformTriangle(mP0, P1, P2, rnd);

        ComputeSample(aSurfPt, samplePoint, aSurfFrame, aSurfMaterial, oSample);
    }

    virtual float EstimateContribution(
        const Vec3f             &aSurfPt,
        const Frame             &aSurfFrame,
        const AbstractMaterial  &aSurfMaterial,
              Rng               &aRng
        ) const override
    {
        aRng; // unused param

        // Doesn't work: 
        // Estimate the contribution using a "sample" in the centre of gravity of the triangle
        // -> This cuts off the light if the center of gravity is below the surface while some part of triangle can still be visible!
        //const Vec3f centerOfGravity =
        //    // (mP0 + P1 + P2) / 3.0f
        //    // (mP0 + mP0 + mE1 + mP0 + mE2) / 3.0f
        //    // mP0 + (mE1 + mE2) / 3.0f
        //    mP0 + (mE1 + mE2) / 3.0f;

        // Combine the estimate from all vertices of the triangle
        const Vec3f P1 = mP0 + mE1;
        const Vec3f P2 = mP0 + mE2;
        const Vec3f P3 = mP0 + 0.33f * mE1 + 0.33f * mE2; // centre of mass
        LightSample sample0, sample1, sample2, sample3;
        ComputeSample(aSurfPt, mP0, aSurfFrame, aSurfMaterial, sample0);
        ComputeSample(aSurfPt, P1,  aSurfFrame, aSurfMaterial, sample1);
        ComputeSample(aSurfPt, P2,  aSurfFrame, aSurfMaterial, sample2);
        ComputeSample(aSurfPt, P3,  aSurfFrame, aSurfMaterial, sample3);
        return
            (   sample0.mSample.Luminance() / sample0.mPdfW
              + sample1.mSample.Luminance() / sample1.mPdfW
              + sample2.mSample.Luminance() / sample2.mPdfW
              + sample3.mSample.Luminance() / sample3.mPdfW
              )
            / 4.f;
    }

private:
    void ComputeSample(
        const Vec3f             &aSurfPt,
        const Vec3f             &aSamplePt,
        const Frame             &aSurfFrame,
        const AbstractMaterial  &aSurfMaterial,
              LightSample       &oSample
        ) const
    {
        // Replicated in GetEmmision()!

        oSample.mWig = aSamplePt - aSurfPt;
        const float distSqr = oSample.mWig.LenSqr();
        oSample.mDist = sqrt(distSqr);
        oSample.mWig /= oSample.mDist;
        const float cosThetaOut = -Dot(mFrame.Normal(), oSample.mWig); // for two-sided light use absf()
        float cosThetaIn        =  Dot(aSurfFrame.Normal(), oSample.mWig);

        const MaterialProperties matProps = aSurfMaterial.GetProperties();
        const bool sampleFrontSide  = Utils::IsMasked(matProps, kBsdfFrontSideLightSampling);
        const bool sampleBackSide   = Utils::IsMasked(matProps, kBsdfBackSideLightSampling);

        // Materials do this checking on their own, but since we use this code also 
        // for light contribution estimation, it is better to cut the light which is not going 
        // to be used by the material here too to get better contribution estimates.
        if (sampleFrontSide && sampleBackSide)
            cosThetaIn = std::abs(cosThetaIn);
        else if (sampleFrontSide)
            cosThetaIn = std::max(cosThetaIn, 0.0f);
        else if (sampleBackSide)
        {
            PG3_ERROR_CODE_NOT_TESTED("Materials of all types of light sampling should be tested.");
            cosThetaIn = std::max(-cosThetaIn, 0.0f);
        }
        else
            cosThetaIn = 0.0f;

        PG3_ASSERT_FLOAT_NONNEGATIVE(cosThetaIn);

        if (cosThetaOut > 0.f)
            // Planar version: BSDF * Li * ((cos_in * cos_out) / dist^2)
            oSample.mSample = mRadiance * cosThetaIn; // Angular version
        else
            oSample.mSample.MakeZero();

        // Angular PDF. We use low epsilon boundary to avoid division by very small PDFs.
        const float absCosThetaOut = abs(cosThetaOut);
        oSample.mPdfW = std::max(mInvArea * (distSqr / absCosThetaOut), Geom::kEpsDist);

        PG3_ASSERT(oSample.mPdfW >= Geom::kEpsDist);

        oSample.mLightProbability = 1.0f;
    }

public:
    Vec3f       mP0, mE1, mE2;
    Frame       mFrame;
    SpectrumF   mRadiance;    // Spectral radiance
    float       mArea;
    float       mInvArea;
};

//////////////////////////////////////////////////////////////////////////
class PointLight : public AbstractLight
{
public:

    PointLight(const Vec3f& aPosition)
    {
        mIntensity.MakeZero();
        mPosition = aPosition;
    }

    virtual void SetPower(const SpectrumF& aPower)
    {
        mIntensity = aPower / (4 * Math::kPiF);
    }

    // Returns amount of outgoing radiance in the direction.
    // The point parameter is unused - it is a heritage of the abstract light interface
    virtual SpectrumF GetEmmision(
        const Vec3f             &aLightPt,
        const Vec3f             &aWol,
        const Vec3f             &aSurfPt,
              float             *oPdfW = nullptr,
        const Frame             *aSurfFrame = nullptr,
        const AbstractMaterial  *aSurfMaterial = nullptr) const override
    {
        aSurfPt; aLightPt; aSurfFrame; aSurfMaterial; aWol; // unused parameter

        if (oPdfW != nullptr)
            *oPdfW = Math::InfinityF();

        return SpectrumF().MakeZero();
    };

    virtual void SampleIllumination(
        const Vec3f             &aSurfPt,
        const Frame             &aSurfFrame,
        const AbstractMaterial  &aSurfMaterial,
        Rng                     &aRng,
        LightSample             &oSample
        ) const override
    {
        aRng; // unused param

        ComputeIllumination(aSurfPt, aSurfFrame, aSurfMaterial, oSample);
    }

    virtual float EstimateContribution(
        const Vec3f             &aSurfPt,
        const Frame             &aSurfFrame,
        const AbstractMaterial  &aSurfMaterial,
              Rng               &aRng
        ) const override
    {
        aRng; // unused param

        LightSample sample;
        ComputeIllumination(aSurfPt, aSurfFrame, aSurfMaterial, sample);
        return sample.mSample.Luminance();
    }

private:
    void ComputeIllumination(
        const Vec3f             &aSurfPt,
        const Frame             &aSurfFrame,
        const AbstractMaterial  &aSurfMaterial,
        LightSample             &oSample
        ) const
    {
        oSample.mWig  = mPosition - aSurfPt;
        const float distSqr = oSample.mWig.LenSqr();
        oSample.mDist = sqrt(distSqr);
        oSample.mWig /= oSample.mDist;

        float cosThetaIn = Dot(aSurfFrame.Normal(), oSample.mWig);

        const MaterialProperties matProps = aSurfMaterial.GetProperties();
        const bool sampleFrontSide  = Utils::IsMasked(matProps, kBsdfFrontSideLightSampling);
        const bool sampleBackSide   = Utils::IsMasked(matProps, kBsdfBackSideLightSampling);

        // Materials do this checking on their own, but since we use this code also 
        // for light contribution estimation, it is better to cut the light which is not going 
        // to be used by the material here too to get better contribution estimates.
        if (sampleFrontSide && sampleBackSide)
            cosThetaIn = std::abs(cosThetaIn);
        else if (sampleFrontSide)
            cosThetaIn = std::max(cosThetaIn, 0.0f);
        else if (sampleBackSide)
        {
            PG3_ERROR_CODE_NOT_TESTED("Materials of all types of light sampling should be tested.");
            cosThetaIn = std::max(-cosThetaIn, 0.0f);
        }
        else
            cosThetaIn = 0.0f;

        PG3_ASSERT_FLOAT_NONNEGATIVE(cosThetaIn);

        if (cosThetaIn > 0.f)
            oSample.mSample = mIntensity * cosThetaIn / distSqr;
        else
            oSample.mSample.MakeZero();

        oSample.mPdfW               = Math::InfinityF();
        oSample.mLightProbability   = 1.0f;
    }

public:

    Vec3f       mPosition;
    SpectrumF   mIntensity;   // Spectral radiant intensity
};


//////////////////////////////////////////////////////////////////////////
class BackgroundLight : public AbstractLight
{
public:
    BackgroundLight() :
        mEnvMap(nullptr)
    {
        SetConstantRadiance(SpectrumF().MakeZero());
    }

    virtual ~BackgroundLight() override
    {
        if (mEnvMap != nullptr)
            delete mEnvMap;
    }

    virtual void SetConstantRadiance(const SpectrumF &aRadiance)
    {
        mConstantRadiance = aRadiance;
        mCosineSampler.Init(std::make_shared<ConstEnvironmentValue>(mConstantRadiance));
    }

    virtual void LoadEnvironmentMap(
        const std::string   filename,
        float               rotate = 0.0f,
        float               scale = 1.0f,
        bool                doBilinFiltering = false)
    {
        mEnvMap = new EnvironmentMap(filename, rotate, scale, doBilinFiltering);
    }

    // Returns amount of incoming radiance from the direction.
    SpectrumF GetEmmision(
        const Vec3f             &aWig,
        float                   *oPdfW = nullptr,
        const Frame             *aSurfFrame = nullptr,
        const AbstractMaterial  *aSurfMaterial = nullptr
        ) const
    {
        bool sampleFrontSide = false;
        bool sampleBackSide  = false;
        if (aSurfMaterial)
        {
            const MaterialProperties matProps = aSurfMaterial->GetProperties();
            sampleFrontSide = Utils::IsMasked(matProps, kBsdfFrontSideLightSampling);
            sampleBackSide  = Utils::IsMasked(matProps, kBsdfBackSideLightSampling);
        }

        if (mEnvMap != nullptr)
        {
            SpectrumF radiance;
            mEnvMap->EvalRadiance(radiance, aWig, oPdfW, aSurfFrame, &sampleFrontSide, &sampleBackSide);
            return radiance;
        }
        else
        {
            if (oPdfW && aSurfFrame && aSurfMaterial)
                *oPdfW = mCosineSampler.PdfW(aWig, *aSurfFrame, sampleFrontSide, sampleBackSide);
            return mConstantRadiance;
        }
    };

    // Returns amount of outgoing radiance in the direction.
    // The point parameter is unused - it is an heritage of the abstract light interface
    virtual SpectrumF GetEmmision(
        const Vec3f             &aLightPt,
        const Vec3f             &aWol,
        const Vec3f             &aSurfPt,
              float             *oPdfW = nullptr,
        const Frame             *aSurfFrame = nullptr,
        const AbstractMaterial  *aSurfMaterial = nullptr) const override
    {
        aSurfPt;  aLightPt; // unused params

        return GetEmmision(-aWol, oPdfW, aSurfFrame, aSurfMaterial);
    };

    PG3_PROFILING_NOINLINE
    virtual void SampleIllumination(
        const Vec3f             &aSurfPt, 
        const Frame             &aSurfFrame, 
        const AbstractMaterial  &aSurfMaterial,
        Rng                     &aRng,
        LightSample             &oSample
        ) const override
    {
        aSurfPt; // unused parameter

        const MaterialProperties matProps = aSurfMaterial.GetProperties();
        const bool sampleFrontSide = Utils::IsMasked(matProps, kBsdfFrontSideLightSampling);
        const bool sampleBackSide  = Utils::IsMasked(matProps, kBsdfBackSideLightSampling);

        if (mEnvMap != nullptr)
            mEnvMap->Sample(oSample, aSurfFrame, sampleFrontSide, sampleBackSide, aRng);
        else
            // Constant environment illumination
            // Sample the requested hemisphere(s) in a cosine-weighted fashion
            mCosineSampler.Sample(
                oSample, aSurfFrame, sampleFrontSide, sampleBackSide, aRng);
    }

    virtual float EstimateContribution(
        const Vec3f             &aSurfPt,
        const Frame             &aSurfFrame,
        const AbstractMaterial  &aSurfMaterial,
              Rng               &aRng
        ) const override
    {
        if (mEnvMap != nullptr)
        {
            const MaterialProperties matProps = aSurfMaterial.GetProperties();
            const bool sampleFrontSide  = Utils::IsMasked(matProps, kBsdfFrontSideLightSampling);
            const bool sampleBackSide   = Utils::IsMasked(matProps, kBsdfBackSideLightSampling);

            return mEnvMap->EstimateIrradiance(
                aSurfPt, aSurfFrame, sampleFrontSide, sampleBackSide, aRng);
        }
        else
        {
            // A constant environment illumination.
            // Assuming constant BSDF, we can compute the integral analytically.
            // \int{f_r * L_i * \cos{theta_i} d\omega} = f_r * L_i * \int{\cos{theta_i} d\omega} = f_r * L_i * \pi
            return mConstantRadiance.Luminance() * Math::kPiF;
        }
    }

public:

    SpectrumF                mConstantRadiance;
    CosineConstEmSampler     mCosineSampler;
    EnvironmentMap          *mEnvMap;
};
