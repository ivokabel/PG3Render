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
class LightSample
{
public:
    // containts (outgoing radiance * geometric component) / PDF or its equivalent (e.g. for point lights)
    SpectrumF   mSample;

    Vec3f       mWig;
    float       mDist;
};

//////////////////////////////////////////////////////////////////////////
class AbstractLight
{
public:

    // Used in MC estimator of the planar version of the rendering equation. For a randomly sampled 
    // point on the light source surface it computes: (outgoing radiance * geometric component) / PDF
    virtual void SampleIllumination(
        const Vec3f &aSurfPt, 
        const Frame &aFrame, 
        Rng         &aRng,
        LightSample &oSample
        ) const = 0;

    // Returns amount of outgoing radiance from the point in the direction
    virtual SpectrumF GetEmmision(
        const Vec3f& aPt,
        const Vec3f& aWol) const = 0;

    // Returns an estimate of light contribution of this light-source to the given point.
    // Used for picking one of all available light sources when doing light-source sampling.
    virtual float EstimateContribution(
        const Vec3f &aSurfPt,
        const Frame &aFrame,
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
        const Vec3f& aPt,
        const Vec3f& aWol) const
    {
        aPt; // unused parameter

        // We don't check the point since we expect it to be within the light surface

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
        const Vec3f &aSurfPt, 
        const Frame &aFrame, 
        Rng         &aRng,
        LightSample &oSample
        ) const
    {
        // Sample the whole triangle surface
        const Vec2f baryCoords = SampleUniformTriangle(aRng.GetVec2f());
        const Vec3f P1 = mP0 + mE1;
        const Vec3f P2 = mP0 + mE2;
        const Vec3f samplePoint =
              baryCoords.x * P1
            + baryCoords.y * P2
            + (1.0f - baryCoords.x - baryCoords.y) * mP0;

#ifdef PG3_DEBUG_ASSERT_ENABLED
        // Weak sanity check: the point must be within the min-max boundaries of the triangle vertices
        const float minX = std::min(std::min(mP0.x, P1.x), P2.x);
        const float maxX = std::max(std::max(mP0.x, P1.x), P2.x);
        const float minY = std::min(std::min(mP0.y, P1.y), P2.y);
        const float maxY = std::max(std::max(mP0.y, P1.y), P2.y);
        PG3_DEBUG_ASSERT(samplePoint.x >= minX);
        PG3_DEBUG_ASSERT(samplePoint.x <= maxX);
        PG3_DEBUG_ASSERT(samplePoint.y >= minY);
        PG3_DEBUG_ASSERT(samplePoint.y <= maxY);
#endif

        ComputeSample(aSurfPt, samplePoint, aFrame, oSample);
    }

    virtual float EstimateContribution(
        const Vec3f &aSurfPt,
        const Frame &aFrame,
        Rng         &aRng
        ) const
    {
        aRng; // unused param

        // Doesn't work: 
        // Estimate the contribution using a "sample" in the centre of gravity of the triangle
        // -> This cuts off the light if the center of gravity is below the surface while some part of triangle can still be visible!
        //const Vec3f centerOfGravity =
        //    // (mP0 + P1 + P2) / 3.0f
        //    // (mP0 + mP0 + mE1 + mP0 + mE2) / 3.0f
        //    // mP0 + (mE1 + mE2) / 3.0f
        //    mP0 + (mE1 + mE2) / 3.0f; // TODO: Pre-compute

        // Combine the estimate from all vertices of the triangle
        const Vec3f P1 = mP0 + mE1;
        const Vec3f P2 = mP0 + mE2;
        LightSample sample0, sample1, sample2;
        ComputeSample(aSurfPt, mP0, aFrame, sample0);
        ComputeSample(aSurfPt, P1,  aFrame, sample1);
        ComputeSample(aSurfPt, P2,  aFrame, sample2);
        return
            (   sample0.mSample.Luminance()
              + sample1.mSample.Luminance()
              + sample2.mSample.Luminance())
            / 3.f;
    }

private:
    void ComputeSample(
        const Vec3f &aSurfPt,
        const Vec3f &aSamplePt,
        const Frame &aSurfFrame,
        LightSample &oSample
        ) const
    {
        // Direction, distance
        oSample.mWig         = aSamplePt - aSurfPt;
        const float distSqr  = oSample.mWig.LenSqr();
        oSample.mDist        = sqrt(distSqr);
        oSample.mWig        /= oSample.mDist;

        // Prepare geometric component: (out cosine * in cosine) / distance^2
        const float cosThetaOut = -Dot(mFrame.mZ, oSample.mWig); // for two-sided light use absf()
        const float cosThetaIn  =  Dot(aSurfFrame.mZ, oSample.mWig);

        // Compute "radiance * geometric component / PDF"
        if ((cosThetaIn <= 0) || (cosThetaOut <= 0))
            oSample.mSample.MakeZero();
        else
        {
            const float geomComp = (cosThetaIn * cosThetaOut) / distSqr;
            const float invPDF   = mArea;
            oSample.mSample = mRadiance * geomComp * invPDF;
        }
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
        const Vec3f& aPt,
        const Vec3f& aWol) const
    {
        aPt; aWol; // unused parameter

        return SpectrumF().MakeZero();
    };

    virtual void SampleIllumination(
        const Vec3f &aSurfPt,
        const Frame &aFrame,
        Rng         &aRng,
        LightSample &oSample
        ) const
    {
        aRng; // unused param

        SampleIllumination(aSurfPt, aFrame, oSample);
    }

    virtual float EstimateContribution(
        const Vec3f &aSurfPt,
        const Frame &aFrame,
        Rng         &aRng
        ) const
    {
        aRng; // unused param

        LightSample sample;
        SampleIllumination(aSurfPt, aFrame, sample);
        return sample.mSample.Luminance();
    }

private:
    void SampleIllumination(
        const Vec3f &aSurfPt,
        const Frame &aFrame,
        LightSample &oSample
        ) const
    {
        oSample.mWig = mPosition - aSurfPt;
        const float distSqr = oSample.mWig.LenSqr();
        oSample.mDist = sqrt(distSqr);

        oSample.mWig /= oSample.mDist;

        float cosTheta = Dot(aFrame.mZ, oSample.mWig);

        if (cosTheta <= 0)
            oSample.mSample.MakeZero();
        else
            oSample.mSample = mIntensity * cosTheta / distSqr;
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
    PG3_PROFILING_NOINLINE
    SpectrumF GetEmmision(const Vec3f& aWig, bool bDoBilinFiltering = false) const
    {
        aWig; // unused parameter

        if (mEnvMap != NULL)
            return mEnvMap->Lookup(aWig, bDoBilinFiltering);
        else
            return mConstantRadiance;
    };

    // Returns amount of outgoing radiance in the direction.
    // The point parameter is unused - it is an heritage of the abstract light interface
    virtual SpectrumF GetEmmision(
        const Vec3f& aPt, 
        const Vec3f& aWol) const
    {
        aPt; // unused parameter

        return GetEmmision(-aWol);
    };

    PG3_PROFILING_NOINLINE
    virtual void SampleIllumination(
        const Vec3f &aSurfPt,
        const Frame &aFrame,
        Rng         &aRng,
        LightSample &oSample
        ) const
    {
        aSurfPt; // unused parameter

        if (mEnvMap != NULL)
        {
            #ifndef ENVMAP_USE_IMPORTANCE_SAMPLING

                SampleCosHemisphere(aRng, aFrame, oSample, NULL);

            #else

                float pdf;
                SampleEnvMap(aRng, aFrame, oSample, pdf);

            #endif
        }
        else
        {
            // Sample the hemisphere in the normal direction in a cosine-weighted fashion
            float pdf;
            Vec3f wil = SampleCosHemisphereW(aRng.GetVec2f(), &pdf);

            oSample.mWig    = aFrame.ToWorld(wil);
            oSample.mDist   = std::numeric_limits<float>::max();

            const float cosThetaIn = wil.z;
            oSample.mSample = mConstantRadiance * cosThetaIn / pdf;
        }
    }

    virtual float EstimateContribution(
        const Vec3f &aSurfPt,
        const Frame &aFrame,
        Rng         &aRng
        ) const
    {
        aSurfPt; // unused param

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
                float pdf1Cos;
                SampleCosHemisphere(aRng, aFrame, sample1, &pdf1Cos, false);
                const float pdf1EM = EMPdfW(sample1.mWig);

                // Strategy 2: Sample the environment map alone
                LightSample sample2;
                float pdf2EM;
                SampleEnvMap(aRng, aFrame, sample2, pdf2EM, false);
                const float pdf2Cos = CosHemispherePdfW(aFrame.Normal(), sample2.mWig);

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
        Rng         &aRng,
        const Frame &aFrame,
        LightSample &oSample,
        float       *oPdf,
        bool         aDivideByPdf = true) const
    {
        Vec3f wil = SampleCosHemisphereW(aRng.GetVec2f(), oPdf);

        oSample.mWig = aFrame.ToWorld(wil);
        oSample.mDist = std::numeric_limits<float>::max();

        const SpectrumF radiance = mEnvMap->Lookup(oSample.mWig, false);
        if (aDivideByPdf)
            oSample.mSample = radiance * PI_F /*instead of: cosThetaIn / oPdf = cos / (cos / Pi) = Pi*/;
        else
        {
            const float cosThetaIn = wil.z;
            oSample.mSample = radiance * cosThetaIn;
        }
    }

    // Sample the environment map with the pdf proportional to luminance of the map
    void SampleEnvMap(
        Rng         &aRng,
        const Frame &aFrame,
        LightSample &oSample,
        float       &oPdf,
        bool         aDivideByPdf = true) const
    {
        PG3_ASSERT(mEnvMap != NULL);

        SpectrumF radiance;

        // Sample the environment map with the pdf proportional to luminance of the map
        oSample.mWig = mEnvMap->Sample(aRng.GetVec2f(), oPdf, &radiance);
        oSample.mDist = std::numeric_limits<float>::max();

        const float cosThetaIn = Dot(oSample.mWig, aFrame.Normal());
        if (cosThetaIn <= 0.0f)
            // The sample is below the surface - no light contribution
            oSample.mSample.MakeZero();
        else if (aDivideByPdf)
            oSample.mSample = radiance * cosThetaIn / oPdf;
        else
            oSample.mSample = radiance * cosThetaIn;
    }

    float EMPdfW(const Vec3f &aDirection) const
    {
        PG3_ASSERT(mEnvMap != NULL);

        return mEnvMap->PdfW(aDirection);
    }

public:

    SpectrumF       mConstantRadiance;
    EnvironmentMap *mEnvMap;
};
