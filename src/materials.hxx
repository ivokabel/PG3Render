#pragma once 

#include "math.hxx"
#include "spectrum.hxx"
#include "rng.hxx"
#include "types.hxx"

#define MATERIAL_BLOCKER_EPSILON    1e-5

class BRDFSample
{
public:
    SpectrumF   mSample;            // BRDF attenuation * cosine theta_in

    // Angular PDF of the sample. In finite BRDF cases, it contains the whole PDF of all finite
    // components and is already pre-multiplied by probability of this case.
    float       mPdfW;

    // Probability of picking the additive BRDF component which generated this sample.
    // The component can be an infinite pdf (dirac impulse) BRDF, e.g. Fresnel, 
    //or total finite BRDF. Finite BRDFs cannot be sampled separatelly due to MIS; 
    // Inifinite components' contributions are computed outside MIS mechanism.
    float       mCompProbability;

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
        if (wil.z <= 0.f && wol.z <= 0.f)
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
        const float diffuseReflectanceEst = GetDiffuseReflectance();
        const float cosThetaOut = std::max(aWol.z, 0.f);
        const float glossyReflectanceEst =
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).
        const float totalReflectance = (diffuseReflectanceEst + glossyReflectanceEst);

        if (totalReflectance < MATERIAL_BLOCKER_EPSILON)
        {
            // Diffuse fallback for blocker materials
            oBrdfSample.mSample.MakeZero();
            oBrdfSample.mWil = SampleCosHemisphereW(aRng.GetVec2f(), &oBrdfSample.mPdfW);
            oBrdfSample.mCompProbability = 1.f;
            return;
        }

        PG3_ASSERT_FLOAT_IN_RANGE(
            diffuseReflectanceEst / totalReflectance + glossyReflectanceEst / totalReflectance,
            0.0f, 1.01f);

        // Choose a component sampling strategy based on diffuse and specular reflectance
        const float randomVal = aRng.GetFloat() * totalReflectance;
        if (randomVal < diffuseReflectanceEst)
        {
            // Diffuse, cosine-weighted sampling
            oBrdfSample.mWil = SampleCosHemisphereW(aRng.GetVec2f());
            PG3_ASSERT_VAL_NONNEGATIVE(oBrdfSample.mWil.z);
        }
        else
        {
            // Glossy component sampling

            // Sample phong lobe in the canonical coordinate system (lobe around normal)
            const Vec3f canonicalSample = 
                SamplePowerCosHemisphereW(aRng.GetVec2f(), mPhongExponent/*, &oBrdfSample.mPdfW*/);

            // Rotate sample to mirror-reflection
            const Vec3f wrl = ReflectLocal(aWol);
            Frame lobeFrame;
            lobeFrame.SetFromZ(wrl);
            oBrdfSample.mWil = lobeFrame.ToWorld(canonicalSample);
        }

        oBrdfSample.mCompProbability = 1.f;

        // Get whole PDF value
        oBrdfSample.mPdfW = 
            GetPdfW(aWol, oBrdfSample.mWil, diffuseReflectanceEst, glossyReflectanceEst);

        const float thetaCosIn = oBrdfSample.mWil.z;
        if (thetaCosIn > 0.0f)
            // Above surface: Evaluate the whole BRDF
            oBrdfSample.mSample = EvalBrdf(oBrdfSample.mWil, aWol) * thetaCosIn;
        else
            // Below surface: The sample is valid, it just has zero contribution
            oBrdfSample.mSample.MakeZero();
    }

    float GetPdfW(
        const Vec3f &aWol,
        const Vec3f &aWil,
        const float diffuseReflectanceEst,
        const float glossyReflectanceEst
        ) const
    {
        const float totalReflectance = (diffuseReflectanceEst + glossyReflectanceEst);

        PG3_ASSERT_FLOAT_IN_RANGE(diffuseReflectanceEst, 0.0f, 1.001f);
        PG3_ASSERT_FLOAT_IN_RANGE(glossyReflectanceEst,  0.0f, 1.001f);
        // TODO: Uncomment when there are only energy conserving materials in the scene
        //PG3_ASSERT_FLOAT_IN_RANGE(totalReflectance,      0.0f, 1.001f);

        if (totalReflectance < MATERIAL_BLOCKER_EPSILON)
            // Diffuse fallback for blocker materials
            return CosHemispherePdfW(aWil);

        // Rotate the outgoing direction back to canonical lobe coordinates (lobe around normal)
        const Vec3f wrl = ReflectLocal(aWol);
        Frame lobeFrame;
        lobeFrame.SetFromZ(wrl);
        const Vec3f wiCanonical = lobeFrame.ToLocal(aWil);

        // Sum up both components' PDFs
        const float diffuseProbability = diffuseReflectanceEst / totalReflectance;
        const float glossyProbability  = glossyReflectanceEst  / totalReflectance;
        
        PG3_ASSERT_FLOAT_IN_RANGE(diffuseProbability + glossyProbability, 0.0f, 1.001f);
        return
              diffuseProbability * CosHemispherePdfW(aWil)
            + glossyProbability  * PowerCosHemispherePdfW(wiCanonical, mPhongExponent);
    }

    float GetPdfW(
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const
    {
        // Compute scalar reflectances. Replicated in SampleBrdf()!
        const float diffuseReflectanceEst = GetDiffuseReflectance();
        const float cosThetaOut = std::max(aWol.z, 0.f);
        const float glossyReflectanceEst =
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).

        return GetPdfW(aWol, aWil, diffuseReflectanceEst, glossyReflectanceEst);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    float GetRRContinuationProb(
        const Vec3f &aWol
        ) const
    {
        // For conversion to scalar form we combine two strategies: maximum component value 
        // with weighted "luminance". The "luminance" strategy minimizes noise in colour 
        // channels which human eye is most sensitive to; however, it doesn't work well for paths
        // which mainly contribute with less important channels (e.g. blue in sRGB). 
        // In such cases, the paths can have very small probability of survival even if they 
        // transfers the less important channels with no attenuation which leads to blue or,
        // less often, red fireflies. Maximum channel strategy removes those fireflies completely,
        // but tends to prefer less important channels too much and doesn't cut paths 
        // with blocking combinations of attenuations like (1,0,0)*(0,1,0).
        // It seems that a combination of both works pretty well.
        const float blendCoeff = 0.15f;
        const float diffuseReflectanceEst =
              blendCoeff         * mDiffuseReflectance.Luminance()
            + (1.f - blendCoeff) * mDiffuseReflectance.Max();
        const float cosThetaOut = std::max(aWol.z, 0.f);
        const float glossyReflectanceEst =
              (   blendCoeff         * mPhongReflectance.Luminance()
                + (1.f - blendCoeff) * mPhongReflectance.Max())
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).
                                           // Replicated in SampleBrdf() and GetPdfW()!
        const float totalReflectance = (diffuseReflectanceEst + glossyReflectanceEst);

        return totalReflectance;
    }

    SpectrumF   mDiffuseReflectance;
    SpectrumF   mPhongReflectance;
    float       mPhongExponent;
};
