#pragma once 

#include "math.hxx"
#include "spectrum.hxx"
#include "types.hxx"

class Material
{
public:
    Material()
    {
        Reset();
    }

    void Reset()
    {
        mDiffuseReflectance.SetGreyAttenuation(0.0f);
        mPhongReflectance.SetGreyAttenuation(0.0f);
        mPhongExponent = 1.f;
    }

    void Set(
        const SpectrumF &aDiffuseReflectance,
        const SpectrumF &aGlossyReflectance,
        float            aPhongExponent,
        uint             aDiffuse,
        uint             aGlossy)
    {
        mDiffuseReflectance = aDiffuse ? aDiffuseReflectance : SpectrumF().Zero();
        mPhongReflectance   = aGlossy  ? aGlossyReflectance  : SpectrumF().Zero();
        mPhongExponent      = aPhongExponent;

        if (aGlossy)
            // to make it energy conserving (phong reflectance is already scaled down by the caller)
            mDiffuseReflectance /= 2;
    }

    SpectrumF EvalBrdf(const Vec3f& wil, const Vec3f& wol) const
    {
        if (wil.z <= 0 && wol.z <= 0)
            return SpectrumF().Zero();

        // Diffuse component
        const SpectrumF diffuseComponent(mDiffuseReflectance / PI_F); // TODO: Pre-compute

        // Glossy component
        const float constComponent = (mPhongExponent + 2.0f) / (2.0f * PI_F); // TODO: Pre-compute
        const Vec3f wrl = ReflectLocal(wil);
        // We need to restrict to positive cos values only, otherwise we get unwanted behaviour 
        // in the retroreflection zone.
        const float thetaRCos = std::max(Dot(wrl, wol), 0.f); 
        const float poweredCos = powf(thetaRCos, mPhongExponent);
        const SpectrumF glossyComponent(mPhongReflectance * (constComponent * poweredCos));

        return diffuseComponent + glossyComponent;
    }

    SpectrumF   mDiffuseReflectance;
    SpectrumF   mPhongReflectance;
    float       mPhongExponent;
};
