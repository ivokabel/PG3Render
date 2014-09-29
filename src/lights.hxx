#pragma once

#include <vector>
#include <cmath>
#include "math.hxx"
#include "spectrum.hxx"

class AbstractLight
{
public:

    virtual Spectrum sampleIllumination(const Vec3f& aSurfPt, const Frame& aFrame, Vec3f& oWig, float& oLightDist) const = 0;
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
        p0 = aP0;
        e1 = aP1 - aP0;
        e2 = aP2 - aP0;

        Vec3f normal = Cross(e1, e2);
        float len    = normal.Length();
        mInvArea     = 2.f / len;
        mFrame.SetFromZ(normal);
    }

    virtual void SetPower(const Spectrum& aPower)
    {
        // Radiance = Flux/(Pi*Area)  [W * sr^-1 * m^2]
        // = 25/(Pi*0.25) = 31.831
        // = 25/(Pi*6.5538048016) = 1.214

        mRadiance = aPower * mInvArea / PI_F;
    }

    virtual Spectrum sampleIllumination(const Vec3f& aSurfPt, const Frame& aFrame, Vec3f& oWig, float& oLightDist) const
    {
        // TODO
        aSurfPt;
        aFrame;
        oWig;
        oLightDist;

    	return Spectrum(0);
    }

public:
    Vec3f p0, e1, e2;
    Frame mFrame;
    Spectrum mRadiance;    // Integral radiance? [W.m^-2.sr^-1]
    float mInvArea;
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

	virtual Spectrum sampleIllumination(
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

		if(cosTheta <= 0)
			return Spectrum(0);

		return mIntensity * cosTheta / distSqr;
	}

public:

    Vec3f mPosition;
    Spectrum mIntensity;   // Integral radiant intensity? [W.sr^-1]
};


//////////////////////////////////////////////////////////////////////////
class BackgroundLight : public AbstractLight
{
public:
    BackgroundLight()
    {
        mBackgroundColor = Spectrum(135, 206, 250) / Spectrum(255.f);
    }

    virtual Spectrum sampleIllumination(const Vec3f& aSurfPt, const Frame& aFrame, Vec3f& oWig, float& oLightDist) const
    {
        // TODO
        aSurfPt;
        aFrame;
        oWig;
        oLightDist;

        return Spectrum(0);
    }

public:

    Spectrum mBackgroundColor;
};
