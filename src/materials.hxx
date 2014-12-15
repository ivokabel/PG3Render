#pragma once 

#include "math.hxx"
#include "spectrum.hxx"
#include "rng.hxx"
#include "types.hxx"

class BRDFSample
{
public:
    SpectrumF   mSample;    // (BRDF component attenuation * cosine) / (PDF * component probability)
    float       mThetaInCos;
};

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
        mDiffuseReflectance = aDiffuse ? aDiffuseReflectance : SpectrumF().MakeZero();
        mPhongReflectance   = aGlossy  ? aGlossyReflectance  : SpectrumF().MakeZero();
        mPhongExponent      = aPhongExponent;

        if (aGlossy)
            // to make it energy conserving (phong reflectance is already scaled down by the caller)
            mDiffuseReflectance /= 2;
    }

    SpectrumF EvalBrdf(const Vec3f& wil, const Vec3f& wol) const
    {
        if (wil.z <= 0 && wol.z <= 0)
            return SpectrumF().MakeZero();

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

    // TODO
    // Used in the two-step MC estimator of the angular version of the rendering equation. 
    // It first randomly chooses a BRDF component and then it samples a random direction 
    // for this component.
    void SampleBrdf(
        Rng         &aRng,
        const Frame &aFrame,
        const Vec3f &oWog,
        BRDFSample  &brdfSample,
        Vec3f       &oWig
        ) const
    {
        // unused params
        oWog;

        // debug
        brdfSample.mSample.MakeZero();

        // Compute scalar diff and spec reflectances (later: pre-compute?)

        // Choose a component based on diffuse and specular reflectance

        // If diffuse

            // cosine-weighted sampling: local direction and pdf
            float pdf;
            Vec3f wil = SampleCosHemisphereW(aRng.GetVec2f(), &pdf);
            oWig = aFrame.ToWorld(wil);

            brdfSample.mThetaInCos = wil.z;
            PG3_DEBUG_ASSERT_VAL_NONNEGATIVE(brdfSample.mThetaInCos);

            // Compute the resulting sample:
            //    BRDF (constant) * cos
            //  / (pdf * probability of diffuse component (diffuse luminance))
            brdfSample.mSample =
                  ((mDiffuseReflectance / PI_F)/*TODO: Pre-compute*/ * brdfSample.mThetaInCos)
                / (pdf * 1.0f/*debug*/);

        // else if specular

            // sample phong lobe: local direction and pdf

            // evaluate phong lobe

            // compute the resulting sample: 
                // BRDF
                // /
                // (pdf * probability of specular component (specular luminance))

        // convert incoming direction to world coords
    }

    SpectrumF   mDiffuseReflectance;
    SpectrumF   mPhongReflectance;
    float       mPhongExponent;
};
