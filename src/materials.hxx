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

    SpectrumF EvalDiffuseComponent() const
    {
        return SpectrumF(mDiffuseReflectance / PI_F); // TODO: Pre-compute
    }

    SpectrumF EvalGlossyComponent(const Vec3f& wil, const Vec3f& wol) const
    {
        const float constComponent = (mPhongExponent + 2.0f) / (2.0f * PI_F); // TODO: Pre-compute
        const Vec3f wrl = ReflectLocal(wil);
        // We need to restrict to positive cos values only, otherwise we get unwanted behaviour 
        // in the retroreflection zone.
        const float thetaRCos = std::max(Dot(wrl, wol), 0.f);
        const float poweredCos = powf(thetaRCos, mPhongExponent);

        return SpectrumF(mPhongReflectance * (constComponent * poweredCos));
    }

    SpectrumF EvalBrdf(const Vec3f& wil, const Vec3f& wol) const
    {
        if (wil.z <= 0 && wol.z <= 0)
            return SpectrumF().MakeZero();

        const SpectrumF diffuseComponent = EvalDiffuseComponent();
        const SpectrumF glossyComponent  = EvalGlossyComponent(wil, wol);

        return diffuseComponent + glossyComponent;
    }

    // TODO
    // Used in the two-step MC estimator of the angular version of the rendering equation. 
    // It first randomly chooses a BRDF component and then it samples a random direction 
    // for this component.
    void SampleBrdf(
        Rng         &aRng,
        const Vec3f &aWol,
        BRDFSample  &brdfSample,
        Vec3f       &oWil
        ) const
    {
        // debug
        brdfSample.mSample.MakeZero();

        // Compute scalar diff and spec reflectances (later: pre-compute?)

        // Choose a component based on diffuse and specular reflectance
        const float componentProbability = 1.0f; // debug

        // If diffuse

            //// cosine-weighted sampling: local direction and pdf
            //float pdf;
            //oWil = SampleCosHemisphereW(aRng.GetVec2f(), &pdf);
            //brdfSample.mThetaInCos = oWil.z;
            //PG3_DEBUG_ASSERT_VAL_NONNEGATIVE(brdfSample.mThetaInCos);

            //// Compute the resulting sample:
            ////    BRDF (constant) * cos
            ////  / (pdf * probability of diffuse component (diffuse luminance))
            //brdfSample.mSample =
            //      (EvalDiffuseComponent() * brdfSample.mThetaInCos)
            //    / (pdf * componentProbability);

        // else if specular

            // Sample phong lobe in canonical coordinate system (around normal)
            float pdf;
            const Vec3f canonicalSample = SamplePowerCosHemisphereW(aRng.GetVec2f(), mPhongExponent, &pdf);

            // Rotate sample to mirror-reflection
            Frame lobeFrame;
            const Vec3f wrl = ReflectLocal(aWol);
            lobeFrame.SetFromZ(wrl);
            oWil = lobeFrame.ToWorld(canonicalSample);
            brdfSample.mThetaInCos = std::max(oWil.z, 0.f);

            if (brdfSample.mThetaInCos > 0.0f)
            {
                // Evaluate phong lobe
                const SpectrumF glossyComponent = EvalGlossyComponent(oWil, aWol);

                // Compute the resulting sample: 
                // BRDF / (pdf * probability of specular component (specular luminance))
                brdfSample.mSample =
                      (glossyComponent * brdfSample.mThetaInCos)
                    / (pdf * componentProbability);
            }
            else
                brdfSample.mSample.MakeZero();
    }

    SpectrumF   mDiffuseReflectance;
    SpectrumF   mPhongReflectance;
    float       mPhongExponent;
};
