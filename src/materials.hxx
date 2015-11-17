#pragma once 

#include "hardsettings.hxx"
#include "math.hxx"
#include "spectrum.hxx"
#include "rng.hxx"
#include "types.hxx"

#define MAT_BLOCKER_EPSILON    1e-5

// Various material IoRs and absorbances at roughly 590 nm
#define MAT_AIR_IOR             1.000277f
#define MAT_GLASS_CORNING_IOR   1.510000f
#define MAT_COPPER_IOR          0.468000f
#define MAT_COPPER_ABSORBANCE   2.810000f
#define MAT_SILVER_IOR          0.121000f
#define MAT_SILVER_ABSORBANCE   3.660000f
#define MAT_GOLD_IOR            0.236000f
#define MAT_GOLD_ABSORBANCE     2.960089f

class BRDFSample
{
public:
    // BRDF attenuation * cosine theta_in
    SpectrumF   mSample;

    // Angular PDF of the sample.
    // In finite BRDF cases, it contains the PDF of all finite components altogether.
    // In infitite cases, it equals INFINITY_F.
    float       mPdfW;

    // Probability of picking the additive BRDF component which generated this sample.
    // The component can be more infinite pdf (dirac impulse) BRDFs, e.g. Fresnel, 
    // and/or one total finite BRDF. Finite BRDFs cannot be sampled separatelly due to MIS; 
    // Inifinite components' contributions are computed outside MIS mechanism.
    float       mCompProbability;

    // Chosen incoming direction
    Vec3f       mWil;
};

class AbstractMaterial
{
public:
    virtual SpectrumF EvalBrdf(
        const Vec3f& aWil,
        const Vec3f& aWol
        ) const = 0;

    // Generates a random BRDF sample.
    virtual void SampleBrdf(
        Rng         &aRng,
        const Vec3f &aWol,
        BRDFSample  &oBrdfSample
        ) const = 0;

    virtual void GetFiniteCompProbabilities(
              float &oPdfW,
              float &oCompProbability,
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const = 0;

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(
        const Vec3f &aWol
        ) const = 0;

    virtual bool IsReflectanceZero() const = 0;
};

class PhongMaterial : public AbstractMaterial
{
public:
    PhongMaterial()
    {
        mDiffuseReflectance.SetGreyAttenuation(0.0f);
        mPhongReflectance.SetGreyAttenuation(0.0f);
        mPhongExponent = 1.f;
    }

    PhongMaterial(
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

    SpectrumF EvalGlossyComponent(
        const Vec3f& aWil,
        const Vec3f& aWol
        ) const
    {
        const float constComponent = (mPhongExponent + 2.0f) / (2.0f * PI_F); // TODO: Pre-compute?
        const Vec3f wrl = ReflectLocal(aWil);
        // We need to restrict to positive cos values only, otherwise we get unwanted behaviour 
        // in the retroreflection zone.
        const float thetaRCos = std::max(Dot(wrl, aWol), 0.f);
        const float poweredCos = powf(thetaRCos, mPhongExponent);

        return mPhongReflectance * (constComponent * poweredCos);
    }

    float GetGlossyReflectance() const
    {
        return mPhongReflectance.Luminance(); // TODO: Pre-compute?
    }

    virtual SpectrumF EvalBrdf(
        const Vec3f& aWil,
        const Vec3f& aWol
        ) const override
    {
        if (aWil.z <= 0.f || aWol.z <= 0.f)
            return SpectrumF().MakeZero();

        const SpectrumF diffuseComponent = EvalDiffuseComponent();
        const SpectrumF glossyComponent  = EvalGlossyComponent(aWil, aWol);

        return diffuseComponent + glossyComponent;
    }

    // Generates a random BRDF sample.
    // It first randomly chooses a BRDF component and then it samples a random direction 
    // for this component.
    virtual void SampleBrdf(
        Rng         &aRng,
        const Vec3f &aWol,
        BRDFSample  &oBrdfSample
        ) const override
    {
        // Compute scalar reflectances. Replicated in GetFiniteCompProbabilities()!
        const float diffuseReflectanceEst = GetDiffuseReflectance();
        const float cosThetaOut = std::max(aWol.z, 0.f);
        const float glossyReflectanceEst =
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).
        const float totalReflectance = (diffuseReflectanceEst + glossyReflectanceEst);

        if (totalReflectance < MAT_BLOCKER_EPSILON)
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
            oBrdfSample.mSample = PhongMaterial::EvalBrdf(oBrdfSample.mWil, aWol) * thetaCosIn;
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

        if (totalReflectance < MAT_BLOCKER_EPSILON)
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

    virtual void GetFiniteCompProbabilities(
              float &oPdfW,
              float &oCompProbability,
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const override
    {
        // Compute scalar reflectances. Replicated in SampleBrdf()!
        const float diffuseReflectanceEst = GetDiffuseReflectance();
        const float cosThetaOut = std::max(aWol.z, 0.f);
        const float glossyReflectanceEst =
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).

        oPdfW            = GetPdfW(aWol, aWil, diffuseReflectanceEst, glossyReflectanceEst);
        oCompProbability = 1.0f;
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(
        const Vec3f &aWol
        ) const override
    {
        // For conversion to scalar form we combine two strategies:
        //      maximum component value and weighted "luminance".
        // The "luminance" strategy minimizes noise in colour 
        // channels which human eye is most sensitive to; however, it doesn't work well for paths
        // which mainly contribute with less important channels (e.g. blue in sRGB). 
        // In such cases, the paths can have very small probability of survival even if they 
        // transfer the less important channels with no attenuation which leads to blue or,
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
              (blendCoeff         * mPhongReflectance.Luminance()
            +  (1.f - blendCoeff) * mPhongReflectance.Max())
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).
                                           // Replicated in SampleBrdf() and GetFiniteCompProbabilities()!
        const float totalReflectance = (diffuseReflectanceEst + glossyReflectanceEst);

        return totalReflectance;
    }

    virtual bool IsReflectanceZero() const override
    {
        return mDiffuseReflectance.IsZero() && mPhongReflectance.IsZero();
    }

    SpectrumF   mDiffuseReflectance;
    SpectrumF   mPhongReflectance;
    float       mPhongExponent;
};

class AbstractSmoothMaterial : public AbstractMaterial
{
public:
    virtual SpectrumF EvalBrdf(
        const Vec3f& aWil,
        const Vec3f& aWol
        ) const override
    {
        aWil; aWol; // unreferenced params
            
        // There is zero probability of hitting the only one or two valid combinations of 
        // incoming and outgoing directions which transfer light
        SpectrumF result;
        result.SetGreyAttenuation(0.0f);
        return result;
    }

    virtual void GetFiniteCompProbabilities(
              float &oPdfW,
              float &oCompProbability,
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const override
    {
        aWil; aWol; // unreferenced params

        // There is zero probability of hitting the only valid combination of 
        // incoming and outgoing directions that transfers light
        oPdfW            = 0.0f;
        oCompProbability = 0.0f;
    }
};

class SmoothConductorMaterial : public AbstractSmoothMaterial
{
public:
    SmoothConductorMaterial(
        float aInnerIor, float aOuterIor,   // n
        float aAbsorbance)                  // k
    {
        if (!IsTiny(aOuterIor))
            mEta = aInnerIor / aOuterIor;
        else
            mEta = 1.0f;
        mAbsorbance = aAbsorbance;
    }

    // Generates a random BRDF sample.
    virtual void SampleBrdf(
        Rng         &aRng,
        const Vec3f &aWol,
        BRDFSample  &oBrdfSample
        ) const override
    {
        aRng; // unreferenced params

        oBrdfSample.mWil = ReflectLocal(aWol);
        oBrdfSample.mPdfW = INFINITY_F;
        oBrdfSample.mCompProbability = 1.f;

        // TODO: This may be cached (GetRRContinuationProb and SampleBrdf compute the same Fresnel 
        //       value), but it doesn't seem to be the bottleneck now. Postponing.

        const float reflectance = FresnelConductor(oBrdfSample.mWil.z, mEta, mAbsorbance);
        oBrdfSample.mSample.SetGreyAttenuation(reflectance);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(
        const Vec3f &aWol
        ) const override
    {
        // We can use local z of outgoing direction, because it's equal to incoming direction z

        // TODO: This may be cached (GetRRContinuationProb and SampleBrdf compute the same Fresnel 
        //       value), but it doesn't seem to be the bottleneck now. Postponing.

        const float reflectance = FresnelConductor(aWol.z, mEta, mAbsorbance);
        return reflectance;
    }

    virtual bool IsReflectanceZero() const override
    {
        return false; // TODO
    }

protected:
    float mEta;         // inner IOR / outer IOR
    float mAbsorbance;  // k, the imaginary part of the complex index of refraction
};

class SmoothDielectricMaterial : public AbstractSmoothMaterial
{
public:
    SmoothDielectricMaterial(float aInnerIor, float aOuterIor)
    {
        if (!IsTiny(aOuterIor))
            mEta = aInnerIor / aOuterIor;
        else
            mEta = 1.0f;
    }

    // Generates a random BRDF sample.
    virtual void SampleBrdf(
        Rng         &aRng,
        const Vec3f &aWol,
        BRDFSample  &oBrdfSample
        ) const override
    {
        // TODO: Re-use cosTrans from fresnel in refraction to save one sqrt
        const float fresnelRefl = FresnelDielectric(aWol.z, mEta);

        float attenuation;
        if (fresnelRefl >= 1.0f)
        {
            // Ideal mirror reflection (most probably caused by TIR) - always reflect
            oBrdfSample.mWil = ReflectLocal(aWol);
            attenuation      = 1.0f;
        }
        else
        {
            // Randomly choose between reflection or refraction
            const float rnd = aRng.GetFloat();
            if (rnd <= fresnelRefl)
            {
                // Reflect
                oBrdfSample.mWil = ReflectLocal(aWol);
                attenuation      = fresnelRefl;
            }
            else
            {
                // Refract
                // TODO: local version of refract?
                Refract(oBrdfSample.mWil, aWol, Vec3f(0.f, 0.f, 1.f), mEta);
                attenuation = 1.0f - fresnelRefl;
            }
        }

        oBrdfSample.mCompProbability = attenuation;
        oBrdfSample.mPdfW            = INFINITY_F;
        oBrdfSample.mSample.SetGreyAttenuation(attenuation);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(
        const Vec3f &aWol
        ) const override
    {
        aWol; // unused parameter

        return 1.0f; // reflectance is always 1 for dielectrics
    }

    virtual bool IsReflectanceZero() const override
    {
        return false;
    }

protected:
    float mEta;         // inner IOR / outer IOR
};

class MicrofacetGGXConductorMaterial : public AbstractMaterial
{
public:
    MicrofacetGGXConductorMaterial(
        float aRoughnessAlpha,
        float aInnerIor /* n */,
        float aOuterIor,
        float aAbsorbance /* k */)
    {
        if (!IsTiny(aOuterIor))
            mEta = aInnerIor / aOuterIor;
        else
            mEta = 1.0f;
        mAbsorbance = aAbsorbance;

        mRoughnessAlpha = Clamp(aRoughnessAlpha, 0.001f, 1.0f);
    }

    virtual SpectrumF EvalBrdf(
        const Vec3f& aWil,
        const Vec3f& aWol
        ) const override
    {
        if (   aWil.z <= 0.f
            || aWol.z <= 0.f
            || IsTiny(4.0f * aWil.z * aWol.z)) // BRDF denominator
        {
            return SpectrumF().MakeZero();
        }

        // Halfway vector (microfacet normal)
        const Vec3f halfwayVec = HalfwayVector(aWil, aWol);

        // Fresnel coefficient on the microsurface
        const float cosThetaOM = Dot(halfwayVec, aWol);
        const float fresnelReflectance = FresnelConductor(cosThetaOM, mEta, mAbsorbance);

        // Microfacets distribution
        float distrVal = MicrofacetDistributionGgx(halfwayVec, mRoughnessAlpha);

        // Geometrical factor: Shadowing (incoming direction) * Masking (outgoing direction)
        const float shadowing = MicrofacetMaskingFunctionGgx(aWil, halfwayVec, mRoughnessAlpha);
        const float masking   = MicrofacetMaskingFunctionGgx(aWol, halfwayVec, mRoughnessAlpha);
        const float geometricalFactor = shadowing * masking;

        PG3_ASSERT_FLOAT_IN_RANGE(geometricalFactor, 0.0f, 1.0f);

        // The whole BRDF
        const float cosThetaI = aWil.z;
        const float cosThetaO = aWol.z;
        const float brdfVal =
                (fresnelReflectance * geometricalFactor * distrVal)
              / (4.0f * cosThetaI * cosThetaO);

        PG3_ASSERT_FLOAT_NONNEGATIVE(brdfVal);

        // TODO: Color (from fresnel?)
        SpectrumF result;
        result.SetGreyAttenuation(brdfVal);
        return result;
    }

    // Generates a random BRDF sample.
    virtual void SampleBrdf(
        Rng         &aRng,
        const Vec3f &aWol,
        BRDFSample  &oBrdfSample
        ) const override
    {
        #if defined MATERIAL_GGX_SAMPLING_COS

        oBrdfSample.mWil =
            SampleCosHemisphereW(aRng.GetVec2f(), &oBrdfSample.mPdfW);

        const float thetaCosIn = oBrdfSample.mWil.z;
        if (thetaCosIn > 0.0f)
            // Above surface: Evaluate the whole BRDF
            oBrdfSample.mSample =
                MicrofacetGGXConductorMaterial::EvalBrdf(oBrdfSample.mWil, aWol) * thetaCosIn;
        else
            // Below surface: The sample is valid, it just has zero contribution
            oBrdfSample.mSample.MakeZero();

        #elif defined MATERIAL_GGX_SAMPLING_ALL_NORMALS

        // Distribution sampling
        bool isAboveMicrofacet =
            SampleGgxAllNormals(aWol, mRoughnessAlpha, aRng.GetVec2f(), oBrdfSample.mWil);

        // TODO: Re-use already evaluated data? half-way vector, distribution value?
        MicrofacetGGXConductorMaterial::GetFiniteCompProbabilities(
            oBrdfSample.mPdfW, oBrdfSample.mCompProbability,
            aWol, oBrdfSample.mWil);

        const float thetaCosIn = oBrdfSample.mWil.z;
        if ((thetaCosIn > 0.0f) && isAboveMicrofacet)
            // Above surface: Evaluate the whole BRDF
            oBrdfSample.mSample =
                MicrofacetGGXConductorMaterial::EvalBrdf(oBrdfSample.mWil, aWol) * thetaCosIn;
        else
            // Below surface: The sample is valid, it just has zero contribution
            oBrdfSample.mSample.MakeZero();

        #elif defined MATERIAL_GGX_SAMPLING_VISIBLE_NORMALS

        bool isAboveMicrofacet =
            SampleGgxVisibleNormals(aWol, mRoughnessAlpha, aRng.GetVec2f(), oBrdfSample.mWil);

        // TODO: Re-use already evaluated data? half-way vector, distribution value?
        MicrofacetGGXConductorMaterial::GetFiniteCompProbabilities(
            oBrdfSample.mPdfW, oBrdfSample.mCompProbability,
            aWol, oBrdfSample.mWil);

        const float thetaCosIn = oBrdfSample.mWil.z;
        if (!isAboveMicrofacet)
            // Outgoing dir is below microsurface: this happens occasionally because of numerical problems.
            oBrdfSample.mSample.MakeZero();
        else if (thetaCosIn < 0.0f)
            // Incoming dir is below surface: the sample is valid, it just has zero contribution
            oBrdfSample.mSample.MakeZero();
        else
            oBrdfSample.mSample =
                MicrofacetGGXConductorMaterial::EvalBrdf(oBrdfSample.mWil, aWol) * thetaCosIn;

        #else
        #error Undefined GGX sampling method!
        #endif
    }

    virtual void GetFiniteCompProbabilities(
              float &oPdfW,
              float &oCompProbability,
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const override
    {
        oCompProbability = 1.0f;

        if (aWol.z < 0.f)
        {
            oPdfW = 0.0f;
            return;
        }

        #if defined MATERIAL_GGX_SAMPLING_COS

        aWol; // unreferenced params

        if (aWil.z < 0.f)
            return 0.0f;

        oPdfW = CosHemispherePdfW(aWil);

        #elif defined MATERIAL_GGX_SAMPLING_ALL_NORMALS

        oPdfW = GgxSamplingPdfAllNormals(aWol, aWil, mRoughnessAlpha);

        #elif defined MATERIAL_GGX_SAMPLING_VISIBLE_NORMALS

        oPdfW = GgxSamplingPdfVisibleNormals(aWol, aWil, mRoughnessAlpha);

        #else
        #error Undefined GGX sampling method!
        #endif
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(
        const Vec3f &aWol
        ) const override
    {
        aWol; // unreferenced params

        return 1.0f; // debug
    }

    virtual bool IsReflectanceZero() const override
    {
        return false; // debug
    }

protected:
    float           mEta;               // inner IOR / outer IOR
    float           mAbsorbance;        // k, the imaginary part of the complex index of refraction
    float           mRoughnessAlpha;    // GGX isotropic roughness
};

