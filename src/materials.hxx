#pragma once 

#include "math.hxx"
#include "spectrum.hxx"

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
        const Spectrum& aDiffuseReflectance,
        const Spectrum& aGlossyReflectance,
        float aPhongExponent,
        uint aDiffuse,
        uint aGlossy)
    {
        //Reset();

        mDiffuseReflectance = aDiffuse ? aDiffuseReflectance : Spectrum().Zero();
        mPhongReflectance =   aGlossy ?  aGlossyReflectance  : Spectrum().Zero();
        mPhongExponent = aPhongExponent;

        if (aGlossy)
            // TODO: Is this enough? Shouldn't we do that also to the phong lobe reflectance?
            mDiffuseReflectance /= 2; // to make it energy conserving
    }

    Spectrum evalBrdf(const Vec3f& wil, const Vec3f& wol) const
    {
        if (wil.z <= 0 && wol.z <= 0)
            return Spectrum().Zero();

        Spectrum diffuseComponent(mDiffuseReflectance / PI_F); // TODO: Pre-compute

        // Spectrum glossyComponent  = 

        return diffuseComponent /* + glossyComponent */;
    }

    Spectrum    mDiffuseReflectance;
    Spectrum    mPhongReflectance;
    float       mPhongExponent;
};
