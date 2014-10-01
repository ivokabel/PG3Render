#pragma once

#include <vector>
#include <cmath>
#include "math.hxx"
#include "spectrum.hxx"

class AbstractLight
{
public:

    virtual Spectrum SampleIllumination(const Vec3f& aSurfPt, const Frame& aFrame, Vec3f& oWig, float& oLightDist) const = 0;
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
        mP0 = aP0;
        mE1 = aP1 - aP0;
        mE2 = aP2 - aP0;

        Vec3f normal = Cross(mE1, mE2);
        float len    = normal.Length();
        mInvArea     = 2.f / len;
        mFrame.SetFromZ(normal);
    }

    virtual void SetPower(const Spectrum& aPower)
    {
        // Radiance = Flux/(Pi*Area)  [W * sr^-1 * m^2]
        // = 25/(Pi*0.25) = 31.831
        // = 25/(Pi*6.5538048016) = 1.214

        mRadiance = aPower * (mInvArea / PI_F);
    }

    virtual Spectrum SampleIllumination(const Vec3f& aSurfPt, const Frame& aFrame, Vec3f& oWig, float& oLightDist) const
    {
        // TODO
        aSurfPt;
        aFrame;
        oWig;
        oLightDist;

        return Spectrum().Zero();
    }

public:
    Vec3f       mP0, mE1, mE2;
    Frame       mFrame;
    Spectrum    mRadiance;    // Spectral radiance
    float       mInvArea;
};

//////////////////////////////////////////////////////////////////////////
class PointLight : public AbstractLight
{
public:

    PointLight(const Vec3f& aPosition)
    {
        mPosition = aPosition;
    }

    virtual void SetPower(const Spectrum& aPower)
    {
        mIntensity = aPower / (4 * PI_F);
    }

    virtual Spectrum SampleIllumination(
        const Vec3f& aSurfPt, 
        const Frame& aFrame, 
        Vec3f& oWig, 
        float& oLightDist) const
    {
        oWig           = mPosition - aSurfPt;
        float distSqr  = oWig.LenSqr();
        oLightDist     = sqrt(distSqr);
        
        oWig /= oLightDist;

        float cosTheta = Dot(aFrame.mZ, oWig);

        if (cosTheta <= 0)
            return Spectrum().Zero();

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
        mBackgroundLight.SetSRGBLight(135 / 255.f, 206 / 255.f, 250 / 255.f);
    }

    virtual Spectrum SampleIllumination(const Vec3f& aSurfPt, const Frame& aFrame, Vec3f& oWig, float& oLightDist) const
    {
        // TODO
        aSurfPt;
        aFrame;
        oWig;
        oLightDist;

        return Spectrum().Zero();
    }

public:

    Spectrum mBackgroundLight;
};
