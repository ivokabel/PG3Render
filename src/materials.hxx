#pragma once 

#include "math.hxx"
#include "spectrum.hxx"
#include "rng.hxx"
#include "types.hxx"

#define MATERIAL_BLOCKER_EPSILON    1e-5

class BRDFSample
{
public:
    SpectrumF   mSample;            // BRDF component attenuation * cosine theta_in

    float       mPdfW;              // Angular PDF of the sample
    float       mCompProbability;   // Probability of picking the additive BRDF component which generated this sample
    Vec3f       mWil;               // Chosen incoming direction
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
        uint32_t         aDiffuse,
        uint32_t         aGlossy)
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

    // Generates a radnom BRDF sample.
    // It first randomly chooses a BRDF component and then it samples a random direction 
    // for this component.
    void SampleBrdf(
        Rng         &aRng,
        const Vec3f &aWol,
        BRDFSample  &oBrdfSample
        ) const
    {
        // Compute scalar reflectances. Replicated in GetPdfW()!
        const float diffuseReflectance = GetDiffuseReflectance();
        const float cosThetaOut = std::max(aWol.z, 0.f);
        const float glossyReflectance = 
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).
        const float totalReflectance = (diffuseReflectance + glossyReflectance);

        if (totalReflectance < MATERIAL_BLOCKER_EPSILON)
        {
            // Diffuse fallback for blocker materials
            oBrdfSample.mSample.MakeZero();
            oBrdfSample.mWil = SampleCosHemisphereW(aRng.GetVec2f(), &oBrdfSample.mPdfW);
            oBrdfSample.mCompProbability = 1.f;
            return;
        }

        // Choose a component based on diffuse and specular reflectance
        const float randomVal = aRng.GetFloat() * totalReflectance;
        if (randomVal < diffuseReflectance)
        {
            // Diffuse component

            oBrdfSample.mCompProbability = diffuseReflectance / totalReflectance;

            // Cosine-weighted sampling
            oBrdfSample.mWil = SampleCosHemisphereW(aRng.GetVec2f(), &oBrdfSample.mPdfW);
            PG3_ASSERT_VAL_NONNEGATIVE(oBrdfSample.mWil.z);

            const float thetaCosIn = oBrdfSample.mWil.z;
            oBrdfSample.mSample = EvalDiffuseComponent() * thetaCosIn;
        }
        else
        {
            // Specular component

            oBrdfSample.mCompProbability = 1.f - diffuseReflectance / totalReflectance;

            // Sample phong lobe in the canonical coordinate system (lobe around normal)
            const Vec3f canonicalSample = 
                SamplePowerCosHemisphereW(aRng.GetVec2f(), mPhongExponent, &oBrdfSample.mPdfW);

            // Rotate sample to mirror-reflection
            const Vec3f wrl = ReflectLocal(aWol);
            Frame lobeFrame;
            lobeFrame.SetFromZ(wrl);
            oBrdfSample.mWil = lobeFrame.ToWorld(canonicalSample);

            const float thetaCosIn = oBrdfSample.mWil.z;
            if (thetaCosIn > 0.0f)
                // Above surface: Evaluate the Phong lobe
                oBrdfSample.mSample = EvalGlossyComponent(oBrdfSample.mWil, aWol) * thetaCosIn;
            else
                // Below surface: The sample is valid, it just has zero contribution
                oBrdfSample.mSample.MakeZero();
        }
    }

    float GetPdfW(
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const
    {
        // Compute scalar reflectances. Replicated in SampleBrdf()!
        const float diffuseReflectance = GetDiffuseReflectance();
        const float cosThetaOut = std::max(aWol.z, 0.f);
        const float glossyReflectance =
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).
        const float totalReflectance = (diffuseReflectance + glossyReflectance);

        if (totalReflectance < MATERIAL_BLOCKER_EPSILON)
            // Diffuse fallback for blocker materials
            return CosHemispherePdfW(aWil);

        // Rotate the outgoing direction back to canonical lobe coordinates (lobe around normal)
        const Vec3f wrl = ReflectLocal(aWol);
        Frame lobeFrame;
        lobeFrame.SetFromZ(wrl);
        const Vec3f wiCanonical = lobeFrame.ToLocal(aWil);

        // Sum up both components' PDFs
        const float diffuseProbability = diffuseReflectance / totalReflectance;
        const float glossyProbability  = 1.f - diffuseProbability;
        return
              diffuseProbability * CosHemispherePdfW(aWil)
            + glossyProbability  * PowerCosHemispherePdfW(wiCanonical, mPhongExponent);
    }

    float GetReflectanceEstimate(
        const Vec3f &aWol
        ) const
    {
        // Compute scalar reflectances. Replicated in SampleBrdf() and GetPdfW()!
        const float diffuseReflectance = GetDiffuseReflectance();
        const float cosThetaOut = std::max(aWol.z, 0.f);
        const float glossyReflectance =
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).
        const float totalReflectance = (diffuseReflectance + glossyReflectance);

        return totalReflectance;
    }

    SpectrumF   mDiffuseReflectance;
    SpectrumF   mPhongReflectance;
    float       mPhongExponent;
};
