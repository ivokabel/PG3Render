#pragma once

#include <vector>
#include <cmath>
#include <cassert>
#include "math.hxx"
#include "spectrum.hxx"
#include "rng.hxx"

class AbstractLight
{
public:

    // Used in MC estimator of the planar version of the rendering equation. For a randomly sampled 
    // point on the light source surface it computes: (outgoing radiance * geometric component) / PDF
    virtual Spectrum SampleIllumination(
        const Vec3f& aSurfPt, 
        const Frame& aFrame, 
        Vec3f& oWig, 
        float& oLightDist,
        Rng &rng) const = 0;

    // Returns amount of outgoing radiance from the point in the direction
    virtual Spectrum GetEmmision(
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
        mRadiance.Zero();

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
    virtual Spectrum GetEmmision(
        const Vec3f& aPt,
        const Vec3f& aWol) const
    {
        aPt; aWol; // unused parameters

        // We don't check the point since we expect it to be within the light surface

        if (aWol.z <= 0.)
            return Spectrum().Zero();
        else
            return mRadiance;
    };

    virtual void SetPower(const Spectrum& aPower)
    {
        // Radiance = Flux/(Pi*Area)  [W * sr^-1 * m^2]

        mRadiance = aPower * (mInvArea / PI_F);
    }

    virtual Spectrum SampleIllumination(
        const Vec3f& aSurfPt, 
        const Frame& aFrame, 
        Vec3f& oWig, 
        float& oLightDist, 
        Rng &rng) const
    {
        // Sample the whole triangle surface
        const Vec2f baryCoords = SampleUniformTriangle(rng.GetVec2f());
        const Vec3f P1 = mP0 + mE1;
        const Vec3f P2 = mP0 + mE2;
        const Vec3f samplePoint =
              baryCoords.x * P1
            + baryCoords.y * P2
            + (1.0f - baryCoords.x - baryCoords.y) * mP0;

#ifdef _DEBUG
        // Weak sanity check: the point must be within the min-max boundaries of the triangle vertices
        const float minX = std::min(std::min(mP0.x, P1.x), P2.x);
        const float maxX = std::max(std::max(mP0.x, P1.x), P2.x);
        const float minY = std::min(std::min(mP0.y, P1.y), P2.y);
        const float maxY = std::max(std::max(mP0.y, P1.y), P2.y);
        assert(samplePoint.x >= minX);
        assert(samplePoint.x <= maxX);
        assert(samplePoint.y >= minY);
        assert(samplePoint.y <= maxY);
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
            return Spectrum().Zero();
        else
        {
            const float geomComponent = (cosThetaIn * cosThetaOut) / distSqr;
            const float invPDF        = mArea;
            const Spectrum result =
                  mRadiance
                * geomComponent
                * invPDF;
            return result;
        }
    }

public:
    Vec3f       mP0, mE1, mE2;
    Frame       mFrame;
    Spectrum    mRadiance;    // Spectral radiance
    float       mArea;
    float       mInvArea;
};

//////////////////////////////////////////////////////////////////////////
class PointLight : public AbstractLight
{
public:

    PointLight(const Vec3f& aPosition)
    {
        mIntensity.Zero();
        mPosition = aPosition;
    }

    virtual void SetPower(const Spectrum& aPower)
    {
        mIntensity = aPower / (4 * PI_F);
    }

    // Returns amount of outgoing radiance in the direction.
    // The point parameter is unused - it is a heritage of the abstract light interface
    virtual Spectrum GetEmmision(
        const Vec3f& aPt,
        const Vec3f& aWol) const
    {
        aPt; aWol; // unused parameter

        return Spectrum().Zero();
    };

    virtual Spectrum SampleIllumination(
        const Vec3f& aSurfPt, 
        const Frame& aFrame, 
        Vec3f& oWig, 
        float& oLightDist,
        Rng &rng) const
    {
        rng; // unused parameter

        oWig                = mPosition - aSurfPt;
        const float distSqr = oWig.LenSqr();
        oLightDist          = sqrt(distSqr);
        
        oWig /= oLightDist;

        float cosTheta = Dot(aFrame.mZ, oWig);

        if (cosTheta <= 0)
            return Spectrum().Zero();
        else
            return mIntensity * cosTheta / distSqr;
    }

public:

    Vec3f       mPosition;
    Spectrum    mIntensity;   // Spectral radiant intensity
};


//////////////////////////////////////////////////////////////////////////
class BackgroundLight : public AbstractLight
{
public:
    BackgroundLight()
    {
        mRadiance.Zero();
    }

    virtual void SetConstantRadiance(const Spectrum& aRadiance)
    {
        mRadiance = aRadiance;
    }

    // Returns amount of incoming radiance from the direction.
    Spectrum GetEmmision(const Vec3f& aWig) const
    {
        aWig; // unused parameter

        return mRadiance;
    };

    // Returns amount of outgoing radiance in the direction.
    // The point parameter is unused - it is a heritage of the abstract light interface
    virtual Spectrum GetEmmision(
        const Vec3f& aPt, 
        const Vec3f& aWol) const
    {
        aPt; aWol; // unused parameters

        return GetEmmision(-aWol);
    };

    virtual Spectrum SampleIllumination(
        const Vec3f& aSurfPt, 
        const Frame& aFrame, 
        Vec3f& oWig, 
        float& oLightDist,
        Rng &rng) const
    {
        aSurfPt; // unused parameter

        // Sample the hemisphere in the normal direction in a cosine-weighted fashion
        float pdf;
        Vec3f wil = SampleCosHemisphereW(rng.GetVec2f(), &pdf);

        oWig        = aFrame.ToWorld(wil);
        oLightDist  = std::numeric_limits<float>::max();

        const float cosThetaIn = wil.z;
        const Spectrum result =
              mRadiance
            * cosThetaIn
            / pdf;
        return result;
    }

public:

    Spectrum mRadiance;
};
