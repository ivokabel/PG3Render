#pragma once

#include "math.hxx"
#include "spectrum.hxx"
#include "rng.hxx"
#include "environmentmap.hxx"
#include "types.hxx"
#include "hardsettings.hxx"

#include <vector>
#include <cmath>
#include <cassert>

//////////////////////////////////////////////////////////////////////////
// Used for generating samples on a light source
class LightSample
{
public:
    // (outgoing radiance * cosine theta_in) or it's equivalent (e.g. for point lights)
    // Note that this structure is designed for the angular version of the rendering equation 
    // to allow convenient combination of multiple sampling strategies in multiple-importance schema.
    SpectrumF   mSample;

    float       mPdfW;              // Angular PDF. Equals infinity for point lights.
    float       mLightProbability;  // Probability of picking the light source which generated this sample
                                    // Should equal 1.0 if there's just one light source in the scene
    Vec3f       mWig;
    float       mDist;
};

//////////////////////////////////////////////////////////////////////////
class AbstractLight
{
public:

    // Used in MC estimator of the planar version of the rendering equation. For a randomly sampled 
    // point on the light source surface it computes: outgoing radiance * geometric component
    virtual void SampleIllumination(
        const Vec3f     &aSurfPt, 
        const Frame     &aSurfFrame, 
        Rng             &aRng,
        LightSample     &oSample
        ) const = 0;

    // Returns amount of outgoing radiance from the point in the direction
    virtual SpectrumF GetEmmision(
        const Vec3f &aLightPt,
        const Vec3f &aWol,
        const Vec3f &aSurfPt,
              float *oPdfW = NULL,
        const Frame *aSurfFrame = NULL
        ) const = 0;

    // Returns an estimate of light contribution of this light-source to the given point.
    // Used for picking one of all available light sources when doing light-source sampling.
    virtual float EstimateContribution(
        const Vec3f &aSurfPt,
        const Frame &aSurfFrame,
        Rng         &aRng
        ) const = 0;
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
        mFrame.SetFromZ(normal);
    }

    // Returns amount of outgoing radiance from the point in the direction
    virtual SpectrumF GetEmmision(
        const Vec3f &aLightPt,
        const Vec3f &aWol,
        const Vec3f &aSurfPt,
              float *oPdfW = NULL,
        const Frame *aSurfFrame = NULL
        ) const
    {
        aSurfFrame; // unused param

        // We don't check the point since we expect it to be within the light surface

        if (oPdfW != NULL)
        {
            // Angular PDF. We use low epsilon boundary to avoid division by very small PDFs.
            // Replicated in ComputeSample()!
            Vec3f wig = aLightPt - aSurfPt;
            const float distSqr = wig.LenSqr();
            wig /= sqrt(distSqr);
            const float absCosThetaOut = abs(Dot(mFrame.mZ, wig));
            *oPdfW = std::max(mInvArea * (distSqr / absCosThetaOut), EPS_DIST);
        }

        if (aWol.z <= 0.)
            return SpectrumF().MakeZero();
        else
            return mRadiance;
    };

    virtual void SetPower(const SpectrumF& aPower)
    {
        // Radiance = Flux/(Pi*Area)  [W * sr^-1 * m^2]

        mRadiance = aPower * (mInvArea / PI_F);
    }

    virtual void SampleIllumination(
        const Vec3f     &aSurfPt, 
        const Frame     &aSurfFrame, 
        Rng             &aRng,
        LightSample     &oSample
        ) const
    {
        // Sample the whole triangle surface
        const Vec2f rnd = aRng.GetVec2f();
        const Vec2f baryCoords = SampleUniformTriangle(rnd);
        const Vec3f P1 = mP0 + mE1;
        const Vec3f P2 = mP0 + mE2;
        const Vec3f samplePoint =
              baryCoords.x * P1
            + baryCoords.y * P2
            + (1.0f - baryCoords.x - baryCoords.y) * mP0;

        ComputeSample(aSurfPt, samplePoint, aSurfFrame, oSample);
    }

    virtual float EstimateContribution(
        const Vec3f &aSurfPt,
        const Frame &aSurfFrame,
        Rng         &aRng
        ) const
    {
        aRng; // unused param

        // TODO: The result has to be cached because the estimate is computed twice 
        //       for the same point during MIS (once when picking a light randomly and 
        //       once when computing the probability of picking a light after BRDF sampling).

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
        ComputeSample(aSurfPt, mP0, aSurfFrame, sample0);
        ComputeSample(aSurfPt, P1,  aSurfFrame, sample1);
        ComputeSample(aSurfPt, P2,  aSurfFrame, sample2);
        ComputeSample(aSurfPt, P3,  aSurfFrame, sample3);
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
        const Vec3f     &aSurfPt,
        const Vec3f     &aSamplePt,
        const Frame     &aSurfFrame,
        LightSample     &oSample
        ) const
    {
        // Replicated in GetEmmision()!

        oSample.mWig = aSamplePt - aSurfPt;
        const float distSqr = oSample.mWig.LenSqr();
        oSample.mDist = sqrt(distSqr);
        oSample.mWig /= oSample.mDist;
        const float cosThetaOut = -Dot(mFrame.mZ, oSample.mWig); // for two-sided light use absf()
        const float cosThetaIn = Dot(aSurfFrame.mZ, oSample.mWig);

        if ((cosThetaIn > 0.f) && (cosThetaOut > 0.f))
            // Planar version: BRDF * Li * ((cos_in * cos_out) / dist^2)
            oSample.mSample = mRadiance * cosThetaIn; // Angular version
        else
            oSample.mSample.MakeZero();

        // Angular PDF. We use low epsilon boundary to avoid division by very small PDFs.
        const float absCosThetaOut = abs(cosThetaOut);
        oSample.mPdfW = std::max(mInvArea * (distSqr / absCosThetaOut), EPS_DIST);

        PG3_ASSERT(oSample.mPdfW >= EPS_DIST);

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
        mIntensity = aPower / (4 * PI_F);
    }

    // Returns amount of outgoing radiance in the direction.
    // The point parameter is unused - it is a heritage of the abstract light interface
    virtual SpectrumF GetEmmision(
        const Vec3f &aLightPt,
        const Vec3f &aWol,
        const Vec3f &aSurfPt,
              float *oPdfW = NULL,
        const Frame *aSurfFrame = NULL
        ) const
    {
        aSurfPt; aLightPt; aSurfFrame; aWol; // unused parameter

        if (oPdfW != NULL)
            *oPdfW = INFINITY_F;

        return SpectrumF().MakeZero();
    };

    virtual void SampleIllumination(
        const Vec3f     &aSurfPt,
        const Frame     &aSurfFrame,
        Rng             &aRng,
        LightSample     &oSample
        ) const
    {
        aRng; // unused param

        ComputeIllumination(aSurfPt, aSurfFrame, oSample);
    }

    virtual float EstimateContribution(
        const Vec3f &aSurfPt,
        const Frame &aSurfFrame,
        Rng         &aRng
        ) const
    {
        aRng; // unused param

        LightSample sample;
        ComputeIllumination(aSurfPt, aSurfFrame, sample);
        return sample.mSample.Luminance();
    }

private:
    void ComputeIllumination(
        const Vec3f     &aSurfPt,
        const Frame     &aSurfFrame,
        LightSample     &oSample
        ) const
    {
        oSample.mWig  = mPosition - aSurfPt;
        const float distSqr = oSample.mWig.LenSqr();
        oSample.mDist = sqrt(distSqr);
        oSample.mWig /= oSample.mDist;

        const float cosTheta = Dot(aSurfFrame.mZ, oSample.mWig);
        if (cosTheta > 0.f)
            oSample.mSample = mIntensity * cosTheta / distSqr;
        else
            oSample.mSample.MakeZero();

        oSample.mPdfW               = INFINITY_F;
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
        mEnvMap(NULL)
    {
        mConstantRadiance.MakeZero();
    }

    virtual void SetConstantRadiance(const SpectrumF &aRadiance)
    {
        mConstantRadiance = aRadiance;
    }

    virtual void LoadEnvironmentMap(const std::string filename, float rotate = 0.0f, float scale = 1.0f)
    {
        mEnvMap = new EnvironmentMap(filename, rotate, scale);
    }

    // Returns amount of incoming radiance from the direction.
    SpectrumF GetEmmision(
        const Vec3f &aWig,
              bool   bDoBilinFiltering,
              float *oPdfW = NULL,
        const Frame *aSurfFrame = NULL
        ) const
    {
        if (mEnvMap != NULL)
            return mEnvMap->Lookup(aWig, bDoBilinFiltering, oPdfW);
        else
        {
            if ((oPdfW != NULL) && (aSurfFrame != NULL))
                *oPdfW = CosHemispherePdfW(aSurfFrame->Normal(), aWig);
            return mConstantRadiance;
        }
    };

    // Returns amount of outgoing radiance in the direction.
    // The point parameter is unused - it is an heritage of the abstract light interface
    virtual SpectrumF GetEmmision(
        const Vec3f &aLightPt,
        const Vec3f &aWol,
        const Vec3f &aSurfPt,
              float *oPdfW = NULL,
        const Frame *aSurfFrame = NULL
        ) const
    {
        aSurfPt;  aLightPt; // unused params

        return GetEmmision(-aWol, false, oPdfW, aSurfFrame);
    };

    PG3_PROFILING_NOINLINE
    virtual void SampleIllumination(
        const Vec3f     &aSurfPt, 
        const Frame     &aSurfFrame, 
        Rng             &aRng,
        LightSample     &oSample
        ) const
    {
        aSurfPt; // unused parameter

        if (mEnvMap != NULL)
        {
            #ifdef ENVMAP_USE_IMPORTANCE_SAMPLING
                SampleEnvMap(aRng, aSurfFrame, oSample);
            #else
                SampleCosHemisphere(aRng, aSurfFrame, oSample);
            #endif
        }
        else
        {
            // Constant environment illumination
            // Sample the hemisphere in the normal direction in a cosine-weighted fashion
            Vec3f wil = SampleCosHemisphereW(aRng.GetVec2f(), &oSample.mPdfW);
            oSample.mLightProbability = 1.0f;

            oSample.mWig    = aSurfFrame.ToWorld(wil);
            oSample.mDist   = std::numeric_limits<float>::max();

            const float cosThetaIn = wil.z;
            oSample.mSample = mConstantRadiance * cosThetaIn;
        }
    }

    virtual float EstimateContribution(
        const Vec3f &aSurfPt,
        const Frame &aSurfFrame,
        Rng         &aRng
        ) const
    {
        aSurfPt; // unused param

        // TODO: The result has to be cached because the estimate is computed twice 
        //       for the same point during MIS (once when picking a light randomly and 
        //       once when computing the probability of picking a light after BRDF sampling).

        if (mEnvMap != NULL)
        {
            // Estimate the contribution of the environment map: \int{L_e * f_r * \cos\theta}

            const uint32_t count = 10;
            float sum = 0.f;

            // We need more iterations because the estimate has too high variance if there are 
            // very bright spot lights (e.g. fully dynamic direct sun) under the surface.
            // TODO: This should be done using a pre-computed diffuse map.
            for (uint32_t round = 0; round < count; round++)
            {
                // Strategy 1: Sample the hemisphere in the cosine-weighted fashion
                LightSample sample1;
                SampleCosHemisphere(aRng, aSurfFrame, sample1);
                const float pdf1Cos = sample1.mPdfW;
                const float pdf1EM  = EMPdfW(sample1.mWig);

                // Strategy 2: Sample the environment map alone
                LightSample sample2;
                SampleEnvMap(aRng, aSurfFrame, sample2);
                const float pdf2EM  = sample2.mPdfW;
                const float pdf2Cos = CosHemispherePdfW(aSurfFrame.Normal(), sample2.mWig);

                // Combine the two samples via MIS (balanced heuristics)
                const float part1 =
                      sample1.mSample.Luminance()
                    / (pdf1Cos + pdf1EM);
                const float part2 =
                    sample2.mSample.Luminance()
                    / (pdf2Cos + pdf2EM);
                sum += part1 + part2;
            }

            return sum / count;
        }
        else
        {
            // A constant environment illumination.
            // Assuming constant BRDF, we can compute the integral analytically.
            // \int{f_r * L_i * \cos{theta_i} d\omega} = f_r * L_i * \int{\cos{theta_i} d\omega} = f_r * L_i * \pi
            return mConstantRadiance.Luminance() * PI_F;
        }
    }

    // Sample the hemisphere in the normal direction in a cosine-weighted fashion
    void SampleCosHemisphere(
        Rng             &aRng,
        const Frame     &aSurfFrame,
        LightSample     &oSample) const
    {
        Vec3f wil = SampleCosHemisphereW(aRng.GetVec2f(), &oSample.mPdfW);
        oSample.mLightProbability = 1.0f;

        oSample.mWig = aSurfFrame.ToWorld(wil);
        oSample.mDist = std::numeric_limits<float>::max();

        const SpectrumF radiance = mEnvMap->Lookup(oSample.mWig, false);
        const float cosThetaIn   = wil.z;
        oSample.mSample = radiance * cosThetaIn;
    }

    // Sample the environment map with the pdf proportional to luminance of the map
    void SampleEnvMap(
        Rng             &aRng,
        const Frame     &aSurfFrame,
        LightSample     &oSample) const
    {
        PG3_ASSERT(mEnvMap != NULL);

        SpectrumF radiance;

        // Sample the environment map with the pdf proportional to luminance of the map
        oSample.mWig                = mEnvMap->Sample(aRng.GetVec2f(), oSample.mPdfW, &radiance);
        oSample.mDist               = std::numeric_limits<float>::max();
        oSample.mLightProbability   = 1.0f;

        const float cosThetaIn = Dot(oSample.mWig, aSurfFrame.Normal());
        if (cosThetaIn <= 0.0f)
            // The sample is below the surface - no light contribution
            oSample.mSample.MakeZero();
        else
            oSample.mSample = radiance * cosThetaIn;
    }

    float EMPdfW(const Vec3f &aDirection) const
    {
        PG3_ASSERT(mEnvMap != NULL);
        PG3_ASSERT(!aDirection.IsZero());

        return mEnvMap->PdfW(aDirection);
    }

public:

    SpectrumF       mConstantRadiance;
    EnvironmentMap *mEnvMap;
};
