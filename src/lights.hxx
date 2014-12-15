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
class AbstractLight
{
public:

    // Used in MC estimator of the planar version of the rendering equation. For a randomly sampled 
    // point on the light source surface it computes: (outgoing radiance * geometric component) / PDF
    virtual SpectrumF SampleIllumination(
        const Vec3f &aSurfPt, 
        const Frame &aFrame, 
        Rng         &aRng,
        Vec3f       &oWig, 
        float       &oLightDist
        ) const = 0;

    // Returns amount of outgoing radiance from the point in the direction
    virtual SpectrumF GetEmmision(
        const Vec3f& aPt,
        const Vec3f& aWol) const = 0;
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

    virtual SpectrumF SampleIllumination(
        const Vec3f &aSurfPt,
        const Frame &aFrame,
        Rng         &aRng,
        Vec3f       &oWig,
        float       &oLightDist
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

        // Direction, distance
        oWig                 = samplePoint - aSurfPt;
        const float distSqr  = oWig.LenSqr();
        oLightDist           = sqrt(distSqr);
        oWig                /= oLightDist;

        // Prepare geometric component: (out cosine * in cosine) / distance^2
        const float cosThetaOut = -Dot(mFrame.mZ, oWig); // for two-sided light use absf()
        const float cosThetaIn  =  Dot(aFrame.mZ, oWig);

        // Compute "radiance * geometric component / PDF"
        if ((cosThetaIn <= 0) || (cosThetaOut <= 0))
            return SpectrumF().MakeZero();
        else
        {
            const float geomComponent = (cosThetaIn * cosThetaOut) / distSqr;
            const float invPDF        = mArea;
            const SpectrumF result =
                  mRadiance
                * geomComponent
                * invPDF;
            return result;
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

    virtual SpectrumF SampleIllumination(
        const Vec3f &aSurfPt,
        const Frame &aFrame,
        Rng         &aRng,
        Vec3f       &oWig,
        float       &oLightDist
        ) const
    {
        aRng; // unused parameter

        oWig                = mPosition - aSurfPt;
        const float distSqr = oWig.LenSqr();
        oLightDist          = sqrt(distSqr);
        
        oWig /= oLightDist;

        float cosTheta = Dot(aFrame.mZ, oWig);

        if (cosTheta <= 0)
            return SpectrumF().MakeZero();
        else
            return mIntensity * cosTheta / distSqr;
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
    virtual SpectrumF SampleIllumination(
        const Vec3f &aSurfPt, 
        const Frame &aFrame, 
        Rng         &aRng,
        Vec3f       &oWig,
        float       &oLightDist
        ) const
    {
        aSurfPt; // unused parameter

        if (mEnvMap != NULL)
        {
            #ifndef ENVMAP_USE_IMPORTANCE_SAMPLING

                // Debug: Sample the hemisphere in the normal direction in a cosine-weighted fashion
                float pdf;
                Vec3f wil = SampleCosHemisphereW(aRng.GetVec2f(), &pdf);
                oWig = aFrame.ToWorld(wil);
                const SpectrumF radiance = mEnvMap->Lookup(oWig, false);
                const float cosThetaIn = wil.z;
                const SpectrumF result =
                        radiance
                    * cosThetaIn
                    / pdf;
                return result;

            #else

                float       pdf;
                SpectrumF   radiance;

                // Sample the environment map with the pdf proportional to luminance of the map
                oWig        = mEnvMap->Sample(aRng.GetVec2f(), pdf, &radiance);
                oLightDist  = std::numeric_limits<float>::max();

                const float cosThetaIn = Dot(oWig, aFrame.Normal());
                if (cosThetaIn <= 0.0f)
                {
                    // The sample is below the surface - no light contribution
                    return SpectrumF().MakeZero();
                }
                else
                {
                    const SpectrumF result = radiance * cosThetaIn / pdf;
                    return result;
                }

            #endif
        }
        else
        {
            // Sample the hemisphere in the normal direction in a cosine-weighted fashion
            float pdf;
            Vec3f wil = SampleCosHemisphereW(aRng.GetVec2f(), &pdf);

            oWig = aFrame.ToWorld(wil);
            oLightDist = std::numeric_limits<float>::max();

            const float cosThetaIn = wil.z;
            const SpectrumF result =
                mConstantRadiance
                * cosThetaIn
                / pdf;
            return result;
        }
    }

public:

    SpectrumF       mConstantRadiance;
    EnvironmentMap *mEnvMap;
};
