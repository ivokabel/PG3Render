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
        return mDiffuseReflectance / PI_F; // TODO: Pre-compute?
    }

    float GetDiffuseReflectance() const
    {
        return mDiffuseReflectance.Luminance(); // TODO: Pre-compute?
    }

    SpectrumF EvalGlossyComponent(const Vec3f& wil, const Vec3f& wol) const
    {
        const float constComponent = (mPhongExponent + 2.0f) / (2.0f * PI_F); // TODO: Pre-compute?
        const Vec3f wrl = ReflectLocal(wil);
        // We need to restrict to positive cos values only, otherwise we get unwanted behaviour 
        // in the retroreflection zone.
        const float thetaRCos = std::max(Dot(wrl, wol), 0.f);
        const float poweredCos = powf(thetaRCos, mPhongExponent);

        return mPhongReflectance * (constComponent * poweredCos);
    }

    float GetGlossyReflectance() const
    {
        return mPhongReflectance.Luminance(); // TODO: Pre-compute?
    }

    SpectrumF EvalBrdf(const Vec3f& wil, const Vec3f& wol) const
    {
        if (wil.z <= 0 && wol.z <= 0)
            return SpectrumF().MakeZero();

        const SpectrumF diffuseComponent = EvalDiffuseComponent();
        const SpectrumF glossyComponent  = EvalGlossyComponent(wil, wol);

        return diffuseComponent + glossyComponent;
    }

    // It first randomly chooses a BRDF component and then it samples a random direction 
    // for this component.
    void SampleBrdf(
        Rng         &aRng,
        const Vec3f &aWol,
        BRDFSample  &brdfSample,
        Vec3f       &oWil
        ) const
    {
        // Compute scalar reflectances
        const float diffuseReflectance = GetDiffuseReflectance();
        const float cosThetaOut = std::max(aWol.z, 0.f);
        const float glossyReflectance = 
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).
        const float totalReflectance = (diffuseReflectance + glossyReflectance);

        if (totalReflectance < 0.0001) // TODO
        {
            brdfSample.mSample.MakeZero();
            return;
        }

        // Choose a component based on diffuse and specular reflectance
        const float randomVal = aRng.GetFloat() * totalReflectance;
        if (randomVal < diffuseReflectance)
        {
            // Diffuse component

            const float componentProbability = diffuseReflectance / totalReflectance;

            // Cosine-weighted sampling
            float pdf;
            oWil = SampleCosHemisphereW(aRng.GetVec2f(), &pdf);
            brdfSample.mThetaInCos = oWil.z;
            PG3_DEBUG_ASSERT_VAL_NONNEGATIVE(brdfSample.mThetaInCos);

            // Compute the two-step MC estimator sample
            brdfSample.mSample =
                  (EvalDiffuseComponent() * brdfSample.mThetaInCos)
                / (pdf * componentProbability);
        }
        else
        {
            // Specular component

            const float componentProbability = 1.f - diffuseReflectance / totalReflectance;

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
                // Above surface: Evaluate the Phong lobe and compute the two-step MC estimator sample
                brdfSample.mSample =
                      (EvalGlossyComponent(oWil, aWol) * brdfSample.mThetaInCos)
                    / (pdf * componentProbability);
            }
            else
                // Below surface: The sample is valid, it just has zero contribution
                brdfSample.mSample.MakeZero();
        }
    }

    SpectrumF   mDiffuseReflectance;
    SpectrumF   mPhongReflectance;
    float       mPhongExponent;
};
