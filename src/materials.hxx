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

enum MaterialProperties
{
    kBSDFNone                       = 0x00000000,

    // What sides of surface should lights sample

    kBSDFFrontSideLightSampling     = 0x00000001,
    // Only needed if the back surface is not occluded by the surrounding geometry
    // (e.g. single polygons or other non-watertight geometry)
    kBSDFBackSideLightSampling      = 0x00000002,
};

// Structure that holds data for sampling & evaluation or just evaluation of materials.
// In the case involving sampling many of the members have slightly different meaning.
class MaterialRecord
{
public:

    MaterialRecord(const Vec3f &aWol) :
        mWol(aWol), mWil(0.0f) {}

    MaterialRecord(const Vec3f &aWil, const Vec3f &aWol) :
        mWol(aWol), mWil(aWil) {}

    bool IsBlocker() const
    {
        const float attenuationAndCos = mAttenuation.Max() * ThetaInCosAbs();
        return attenuationAndCos <= MAT_BLOCKER_EPSILON;
    }

    float ThetaInCos() const
    {
        return mWil.z;
    }

    float ThetaInCosAbs() const
    {
        return std::abs(ThetaInCos());
    }

public:

    // Outgoing direction
    Vec3f       mWol;

    // Incoming direction (either input or output parameter)
    Vec3f       mWil;

    // BSDF value for the case of finite BSDF or attenuation for dirac BSDF.
    //
    // For sampling usage scenario it relates to the chosen BSDF component only,
    // otherwise it relates to the total finite BSDF.
    SpectrumF   mAttenuation;

    // In finite BSDF cases it contains the angular PDF of all finite components summed up.
    // In infinite cases it equals INFINITY_F.
    //
    // For sampling usage scenario it relates to the chosen BSDF component only,
    // otherwise it relates to the total finite BSDF.
    float       mPdfW;

    // Probability of picking the additive BSDF component for given outgoing direction.
    // The components can be more infinite pdf (dirac) BSDFs (e.g. Fresnel) 
    // and/or one total finite BSDF.
    //
    // Finite sub-components are treated as one total finite component, because finite BSDFs 
    // cannot be sampled separatelly due to MIS. Infinite components' contributions are computed 
    // outside MIS mechanism.
    //
    // For sampling usage scenario it relates to the chosen BSDF component only,
    // otherwise it relates to the total finite BSDF.
    float       mCompProbability;
};

class AbstractMaterial
{
protected:
    AbstractMaterial(MaterialProperties aProperties) :
        mProperties(aProperties)
    {}

public:

    MaterialProperties GetProperties() const
    {
        return mProperties;
    }

    // Just evaluates the BSDF
    virtual SpectrumF EvalBsdf(
        const Vec3f& aWil,  // Incoming radiance direction
        const Vec3f& aWol   // Outgoing radiance direction
        ) const = 0;

    // Evaluates the BSDF and also computes the probabilities needed for MIS computations
    virtual void EvalBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const = 0;

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const = 0;

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(
        const Vec3f &aWol
        ) const = 0;

    virtual bool IsReflectanceZero() const = 0;

protected:
    MaterialProperties  mProperties;
};

class PhongMaterial : public AbstractMaterial
{
public:
    PhongMaterial() :
        AbstractMaterial(kBSDFFrontSideLightSampling)
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
        uint32_t         aGlossy) :
        AbstractMaterial(kBSDFFrontSideLightSampling)
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

    virtual SpectrumF EvalBsdf(
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

    virtual void EvalBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const override
    {
        aRng; //unused parameter

        oMatRecord.mAttenuation = PhongMaterial::EvalBsdf(oMatRecord.mWil, oMatRecord.mWol);

        PhongMaterial::GetWholeFiniteCompProbabilities(
            oMatRecord.mPdfW,
            oMatRecord.mCompProbability,
            oMatRecord.mWol,
            oMatRecord.mWil);
    }

    // Generates a random BSDF sample.
    // It first randomly chooses a BSDF component and then it samples a random direction 
    // for this component.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const override
    {
        // Compute scalar reflectances. Replicated in GetWholeFiniteCompProbabilities()!
        const float diffuseReflectanceEst = GetDiffuseReflectance();
        const float cosThetaOut = std::max(oMatRecord.mWol.z, 0.f);
        const float glossyReflectanceEst =
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).
        const float totalReflectance = (diffuseReflectanceEst + glossyReflectanceEst);

        if (totalReflectance < MAT_BLOCKER_EPSILON)
        {
            // Diffuse fallback for blocker materials
            oMatRecord.mAttenuation.MakeZero();
            oMatRecord.mWil = SampleCosHemisphereW(aRng.GetVec2f(), &oMatRecord.mPdfW);
            oMatRecord.mCompProbability = 1.f;
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
            oMatRecord.mWil = SampleCosHemisphereW(aRng.GetVec2f());
        }
        else
        {
            // Glossy component sampling

            // Sample phong lobe in the canonical coordinate system (lobe around normal)
            const Vec3f canonicalSample =
                SamplePowerCosHemisphereW(aRng.GetVec2f(), mPhongExponent/*, &oMatRecord.mPdfW*/);

            // Rotate sample to mirror-reflection
            const Vec3f wrl = ReflectLocal(oMatRecord.mWol);
            Frame lobeFrame;
            lobeFrame.SetFromZ(wrl);
            oMatRecord.mWil = lobeFrame.ToWorld(canonicalSample);
        }

        oMatRecord.mCompProbability = 1.f;

        // Get whole PDF value
        oMatRecord.mPdfW =
            GetPdfW(oMatRecord.mWol, oMatRecord.mWil, diffuseReflectanceEst, glossyReflectanceEst);

        const float thetaInCos = oMatRecord.ThetaInCos();
        if (thetaInCos > 0.0f)
            // Above surface: Evaluate the whole BSDF
            oMatRecord.mAttenuation = PhongMaterial::EvalBsdf(oMatRecord.mWil, oMatRecord.mWol);
        else
            // Below surface: The sample is valid, it just has zero contribution
            oMatRecord.mAttenuation.MakeZero();
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

    void GetWholeFiniteCompProbabilities(
              float &oWholeFinCompPdfW,
              float &oWholeFinCompProbability,
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const
    {
        // Compute scalar reflectances. Replicated in SampleBsdf()!
        const float diffuseReflectanceEst = GetDiffuseReflectance();
        const float cosThetaOut = std::max(aWol.z, 0.f);
        const float glossyReflectanceEst =
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).

        oWholeFinCompPdfW        = GetPdfW(aWol, aWil, diffuseReflectanceEst, glossyReflectanceEst);
        oWholeFinCompProbability = 1.0f;
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
                                           // Replicated in SampleBsdf() and GetWholeFiniteCompProbabilities()!
        const float totalReflectance = (diffuseReflectanceEst + glossyReflectanceEst);

        return Clamp(totalReflectance, 0.f, 1.f);
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
protected:
    AbstractSmoothMaterial() :
        AbstractMaterial(kBSDFNone) // Dirac materials don't work with light sampling
    {}

public:
    virtual SpectrumF EvalBsdf(
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

    virtual void EvalBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const override
    {
        aRng; //unused parameter

        oMatRecord.mAttenuation =
            AbstractSmoothMaterial::EvalBsdf(oMatRecord.mWil, oMatRecord.mWol);

        AbstractSmoothMaterial::GetWholeFiniteCompProbabilities(
            oMatRecord.mPdfW,
            oMatRecord.mCompProbability,
            oMatRecord.mWol,
            oMatRecord.mWil);
    }

    void GetWholeFiniteCompProbabilities(
              float &oWholeFinCompPdfW,
              float &oWholeFinCompProbability,
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const
    {
        aWil; aWol; // unreferenced params

        // There is no finite component
        oWholeFinCompPdfW        = 0.0f;
        oWholeFinCompProbability = 0.0f;
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

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const override
    {
        aRng; // unreferenced params

        oMatRecord.mWil             = ReflectLocal(oMatRecord.mWol);
        oMatRecord.mPdfW            = INFINITY_F;
        oMatRecord.mCompProbability = 1.f;

        // TODO: This may be cached (GetRRContinuationProb and SampleBsdf compute the same Fresnel 
        //       value), but it doesn't seem to be the bottleneck now. Postponing.

        const float reflectance = FresnelConductor(oMatRecord.ThetaInCos(), mEta, mAbsorbance);
        oMatRecord.mAttenuation.SetGreyAttenuation(reflectance);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(
        const Vec3f &aWol
        ) const override
    {
        // We can use local z of outgoing direction, because it's equal to incoming direction z

        // TODO: This may be cached (GetRRContinuationProb and SampleBsdf compute the same Fresnel 
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

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const override
    {
        const float fresnelRefl = FresnelDielectric(oMatRecord.mWol.z, mEta);

        float attenuation;

        // Randomly choose between reflection or refraction
        const float rnd = aRng.GetFloat();
        if (rnd <= fresnelRefl)
        {
            // Reflect
            // This branch also handles TIR cases
            oMatRecord.mWil = ReflectLocal(oMatRecord.mWol);
            attenuation     = fresnelRefl;
        }
        else
        {
            // Refract
            // TODO: local version of refract?
            // TODO: Re-use cosTrans from fresnel in refraction to save one sqrt?
            bool isAboveMicrofacet;
            Refract(oMatRecord.mWil, isAboveMicrofacet, oMatRecord.mWol, Vec3f(0.f, 0.f, 1.f), mEta);
            attenuation = 1.0f - fresnelRefl;

            // TODO: Radiance (de)compression?
        }

        oMatRecord.mCompProbability = attenuation;
        oMatRecord.mPdfW            = INFINITY_F;
        oMatRecord.mAttenuation.SetGreyAttenuation(attenuation);
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
        :
        AbstractMaterial(kBSDFFrontSideLightSampling)
    {
        if (!IsTiny(aOuterIor))
            mEta = aInnerIor / aOuterIor;
        else
            mEta = 1.0f;
        mAbsorbance = aAbsorbance;

        mRoughnessAlpha = Clamp(aRoughnessAlpha, 0.001f, 1.0f);
    }

    virtual SpectrumF EvalBsdf(
        const Vec3f& aWil,
        const Vec3f& aWol
        ) const override
    {
        if (   aWil.z <= 0.f
            || aWol.z <= 0.f
            || IsTiny(4.0f * aWil.z * aWol.z)) // BSDF denominator
        {
            return SpectrumF().MakeZero();
        }

        // Halfway vector (microfacet normal)
        const Vec3f halfwayVec = HalfwayVectorReflectionLocal(aWil, aWol);

        // Fresnel coefficient on the microsurface
        const float cosThetaOM = Dot(halfwayVec, aWol);
        const float fresnelReflectance = FresnelConductor(cosThetaOM, mEta, mAbsorbance);

        // Microfacets distribution
        float distrVal = MicrofacetDistributionGgx(halfwayVec, mRoughnessAlpha);

        // Geometrical factor: Shadowing (incoming direction) * Masking (outgoing direction)
        const float shadowing = MicrofacetSmithMaskingFunctionGgx(aWil, halfwayVec, mRoughnessAlpha);
        const float masking   = MicrofacetSmithMaskingFunctionGgx(aWol, halfwayVec, mRoughnessAlpha);
        const float geometricalFactor = shadowing * masking;

        PG3_ASSERT_FLOAT_IN_RANGE(geometricalFactor, 0.0f, 1.0f);

        // The whole BSDF
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

    virtual void EvalBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const override
    {
        aRng; //unused parameter

        oMatRecord.mAttenuation =
            MicrofacetGGXConductorMaterial::EvalBsdf(oMatRecord.mWil, oMatRecord.mWol);

        MicrofacetGGXConductorMaterial::GetWholeFiniteCompProbabilities(
            oMatRecord.mPdfW,
            oMatRecord.mCompProbability,
            oMatRecord.mWol,
            oMatRecord.mWil);
    }

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const override
    {
        Vec3f microfacetDir =
            SampleGgxVisibleNormals(oMatRecord.mWol, mRoughnessAlpha, aRng.GetVec2f());

        bool isOutDirAboveMicrofacet;
        Reflect(oMatRecord.mWil, isOutDirAboveMicrofacet, oMatRecord.mWol, microfacetDir);

        // TODO: Re-use already evaluated data? half-way vector, distribution value?
        MicrofacetGGXConductorMaterial::GetWholeFiniteCompProbabilities(
            oMatRecord.mPdfW, oMatRecord.mCompProbability,
            oMatRecord.mWol, oMatRecord.mWil);

        const float thetaInCos = oMatRecord.ThetaInCos();
        if (!isOutDirAboveMicrofacet)
            // Outgoing dir is below microsurface: this happens occasionally because of numerical problems in the sampling routine.
            oMatRecord.mAttenuation.MakeZero();
        else if (thetaInCos < 0.0f)
            // Incoming dir is below surface: the sample is valid, it just has zero contribution
            oMatRecord.mAttenuation.MakeZero();
        else
            oMatRecord.mAttenuation =
                MicrofacetGGXConductorMaterial::EvalBsdf(oMatRecord.mWil, oMatRecord.mWol);
    }

    void GetWholeFiniteCompProbabilities(
              float &oWholeFinCompPdfW,
              float &oWholeFinCompProbability,
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const
    {
        oWholeFinCompProbability = 1.0f;

        if (aWol.z < 0.f)
        {
            oWholeFinCompPdfW = 0.0f;
            return;
        }

        const Vec3f halfwayVec = HalfwayVectorReflectionLocal(aWil, aWol);
        const float distrVal = MicrofacetDistributionGgx(halfwayVec, mRoughnessAlpha);
        const float normalPdf = GgxSamplingPdfVisibleNormals(aWol, halfwayVec, distrVal, mRoughnessAlpha);
        const float reflectionJacobian = MicrofacetReflectionJacobian(aWol, halfwayVec);
        oWholeFinCompPdfW = normalPdf * reflectionJacobian;
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(
        const Vec3f &aWol
        ) const override
    {
        aWol; // unreferenced param

        return 1.0f; // TODO
    }

    virtual bool IsReflectanceZero() const override
    {
        return false; // there should always be non-zero reflectance
    }

protected:
    float           mEta;               // inner IOR / outer IOR
    float           mAbsorbance;        // k, the imaginary part of the complex index of refraction
    float           mRoughnessAlpha;    // GGX isotropic roughness
};

class MicrofacetGGXDielectricMaterial : public AbstractMaterial
{
public:
    MicrofacetGGXDielectricMaterial(
        float aRoughnessAlpha,
        float aInnerIor /* n */,
        float aOuterIor,
        bool aAllowBackSideLightSampling = false// Only needed if the back surface is not occluded 
                                                // by the surrounding geometry (e.g. single polygons 
                                                // or other non-watertight geometry)
        ) :
        AbstractMaterial(
            MaterialProperties(
                  kBSDFFrontSideLightSampling
                | (aAllowBackSideLightSampling ? kBSDFBackSideLightSampling : 0)))
    {
        if (!IsTiny(aOuterIor))
        {
            mEta    = aInnerIor / aOuterIor;
            mEtaInv = aOuterIor / aInnerIor;
        }
        else
        {
            mEta    = 1.0f;
            mEtaInv = 1.0f;
        }

        mRoughnessAlpha = Clamp(aRoughnessAlpha, 0.001f, 1.0f);
    }

protected:

    class EvalContext
    {
    public:
        Vec3f wolSwitched;
        Vec3f wilSwitched;

        bool isOutDirFromBelow;
        bool isOutDirAboveMicrofacet;
        bool isReflection;
        float etaSwitched;
        float etaInvSwitched;

        Vec3f microfacetDirSwitched;
        float fresnelReflectance;
        float distrVal;
    };

    void InitEvalContextFromInOut(
        EvalContext &oCtx,
        const Vec3f &aWil,
        const Vec3f &aWol
        ) const
    {
        // Warning: Partially replicated in SampleBsdf()!

        // Make sure that the underlying code always deals with outgoing direction which is above the surface
        // TODO: Use just mirror symmetry instead of center symmetry?
        oCtx.isOutDirFromBelow  = (aWol.z < 0.f);
        oCtx.isReflection       = (aWil.z > 0.f == aWol.z > 0.f); // on the same side of the surface
        oCtx.wolSwitched        = aWol * (oCtx.isOutDirFromBelow ? -1.0f : 1.0f);
        oCtx.wilSwitched        = aWil * (oCtx.isOutDirFromBelow ? -1.0f : 1.0f);
        oCtx.etaSwitched        = (oCtx.isOutDirFromBelow ? mEtaInv : mEta);
        oCtx.etaInvSwitched     = (oCtx.isOutDirFromBelow ? mEta : mEtaInv);

        if (oCtx.isReflection)
            oCtx.microfacetDirSwitched = HalfwayVectorReflectionLocal(
                oCtx.wilSwitched, oCtx.wolSwitched);
        else
            // Since the incident direction is below geometrical surface, we use inverse eta
            oCtx.microfacetDirSwitched = HalfwayVectorRefractionLocal(
                oCtx.wilSwitched, oCtx.wolSwitched, oCtx.etaInvSwitched);

        oCtx.distrVal = MicrofacetDistributionGgx(oCtx.microfacetDirSwitched, mRoughnessAlpha);
        const float cosThetaOM = Dot(oCtx.microfacetDirSwitched, oCtx.wolSwitched);
        oCtx.fresnelReflectance = FresnelDielectric(cosThetaOM, oCtx.etaSwitched);
    }

    SpectrumF EvalBsdf(EvalContext &aCtx) const
    {
        const float cosThetaIAbs = std::abs(aCtx.wilSwitched.z);
        const float cosThetaOAbs = std::abs(aCtx.wolSwitched.z);

        // Check BSDF denominator
        if (aCtx.isReflection && IsTiny(4.0f * cosThetaIAbs * cosThetaOAbs))
            return SpectrumF().MakeZero();
        else if (!aCtx.isReflection && IsTiny(cosThetaIAbs * cosThetaOAbs))
            return SpectrumF().MakeZero();

        // Geometrical factor: Shadowing (incoming direction) * Masking (outgoing direction)
        const float shadowing = MicrofacetSmithMaskingFunctionGgx(aCtx.wilSwitched, aCtx.microfacetDirSwitched, mRoughnessAlpha);
        const float masking   = MicrofacetSmithMaskingFunctionGgx(aCtx.wolSwitched, aCtx.microfacetDirSwitched, mRoughnessAlpha);
        const float geometricalFactor = shadowing * masking;

        PG3_ASSERT_FLOAT_IN_RANGE(geometricalFactor, 0.0f, 1.0f);

        // The whole BSDF
        float brdfVal;
        const float fresnelRefl = aCtx.fresnelReflectance;
        if (aCtx.isReflection)
            brdfVal =
                  (fresnelRefl * geometricalFactor * aCtx.distrVal)
                / (4.0f * cosThetaIAbs * cosThetaOAbs);
        else
        {
            const float cosThetaMI = Dot(aCtx.microfacetDirSwitched, aCtx.wilSwitched);
            const float cosThetaMO = Dot(aCtx.microfacetDirSwitched, aCtx.wolSwitched);
            brdfVal =
                  (   (std::abs(cosThetaMI) * std::abs(cosThetaMO))
                    / (cosThetaIAbs * cosThetaOAbs))
                * (   (Sqr(aCtx.etaInvSwitched) * (1.0f - fresnelRefl) * geometricalFactor * aCtx.distrVal)
                    / (Sqr(cosThetaMI + aCtx.etaInvSwitched * cosThetaMO)));
                       // TODO: What if (cosThetaMI + etaInvSwitched * cosThetaMO) is close to zero??

            brdfVal *= Sqr(aCtx.etaSwitched); // radiance (solid angle) compression
        }

        PG3_ASSERT_FLOAT_NONNEGATIVE(brdfVal);

        // TODO: Color (from fresnel?)
        SpectrumF result;
        result.SetGreyAttenuation(brdfVal);
        return result;
    }

public:

    virtual SpectrumF EvalBsdf(
        const Vec3f& aWil,
        const Vec3f& aWol
        ) const override
    {
        EvalContext ctx;
        InitEvalContextFromInOut(ctx, aWil, aWol);

        SpectrumF bsdfVal = EvalBsdf(ctx);

        return bsdfVal;
    }

    virtual void EvalBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const override
    {
        aRng; //unused parameter

        EvalContext ctx;
        InitEvalContextFromInOut(ctx, oMatRecord.mWil, oMatRecord.mWol);

        oMatRecord.mAttenuation = EvalBsdf(ctx);
        GetWholeFiniteCompProbabilities(oMatRecord.mPdfW, oMatRecord.mCompProbability, ctx);
    }

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord
        ) const override
    {
        EvalContext ctx;

        // Warning: Partially replicated in InitEvalContextFromInOut()!

        // Make sure that the underlying code always deals with outgoing direction which is above the surface
        // TODO: Use just mirror symmetry instead of center symmetry?
        ctx.isOutDirFromBelow   = (oMatRecord.mWol.z < 0.f);
        ctx.wolSwitched         = oMatRecord.mWol * (ctx.isOutDirFromBelow ? -1.0f : 1.0f);
        ctx.etaSwitched         = (ctx.isOutDirFromBelow ? mEtaInv : mEta);
        ctx.etaInvSwitched      = (ctx.isOutDirFromBelow ? mEta : mEtaInv);

        ctx.microfacetDirSwitched =
            SampleGgxVisibleNormals(ctx.wolSwitched, mRoughnessAlpha, aRng.GetVec2f());
        ctx.distrVal =
            MicrofacetDistributionGgx(ctx.microfacetDirSwitched, mRoughnessAlpha);

        float thetaInCosAbs;

        // Randomly choose between reflection or refraction
        const float cosThetaOM = Dot(ctx.microfacetDirSwitched, ctx.wolSwitched);
        ctx.fresnelReflectance  = FresnelDielectric(cosThetaOM, ctx.etaSwitched);
        const float rnd = aRng.GetFloat();
        if (rnd <= ctx.fresnelReflectance)
        {
            // This branch also handles TIR cases
            Reflect(ctx.wilSwitched, ctx.isOutDirAboveMicrofacet, ctx.wolSwitched, ctx.microfacetDirSwitched);
            thetaInCosAbs = ctx.wilSwitched.z;
            ctx.isReflection = true;
        }
        else
        {
            // TODO: Re-use cosTrans from fresnel in refraction to save one sqrt?
            Refract(ctx.wilSwitched, ctx.isOutDirAboveMicrofacet, ctx.wolSwitched, ctx.microfacetDirSwitched, ctx.etaSwitched);
            thetaInCosAbs = -ctx.wilSwitched.z;
            ctx.isReflection = false;
        }

        // Switch up-down back if necessary
        oMatRecord.mWil = ctx.wilSwitched * (ctx.isOutDirFromBelow ? -1.0f : 1.0f);

        GetWholeFiniteCompProbabilities(oMatRecord.mPdfW, oMatRecord.mCompProbability, ctx);

        if (!ctx.isOutDirAboveMicrofacet)
            // Outgoing dir is below microsurface: this happens occasionally because of numerical problems in the sampling routine.
            oMatRecord.mAttenuation.MakeZero();
        else if (thetaInCosAbs < 0.0f)
            // Incoming dir is below relative surface: the sample is valid, it just has zero contribution
            oMatRecord.mAttenuation.MakeZero();
        else
            oMatRecord.mAttenuation = EvalBsdf(ctx);
    }

    void GetWholeFiniteCompProbabilities(
              float         &oWholeFinCompPdfW,
              float         &oWholeFinCompProbability,
        const EvalContext   &aCtx
        ) const
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aWil);
        PG3_ASSERT_VEC3F_NORMALIZED(aWol);

        oWholeFinCompProbability = 1.0f;

        if (aCtx.wolSwitched.z < 0.f)
        {
            oWholeFinCompPdfW = 0.0f;
            return;
        }

        const float visNormalsPdf =
            GgxSamplingPdfVisibleNormals(aCtx.wolSwitched, aCtx.microfacetDirSwitched,
                                         aCtx.distrVal, mRoughnessAlpha);

        float transfJacobian;
        if (aCtx.isReflection)
            transfJacobian = MicrofacetReflectionJacobian(
                aCtx.wilSwitched, aCtx.microfacetDirSwitched);
        else
            transfJacobian = MicrofacetRefractionJacobian(
                aCtx.wolSwitched, aCtx.wilSwitched, aCtx.microfacetDirSwitched, aCtx.etaInvSwitched);

        const float compProbability =
            aCtx.isReflection ? aCtx.fresnelReflectance : (1.0f - aCtx.fresnelReflectance);

        oWholeFinCompPdfW = visNormalsPdf * transfJacobian * compProbability;

        PG3_ASSERT_FLOAT_NONNEGATIVE(oWholeFinCompPdfW);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(
        const Vec3f &aWol
        ) const override
    {
        aWol; // unreferenced param

        return 1.0f;
    }

    virtual bool IsReflectanceZero() const override
    {
        return false; // there always is non-zero reflectance
    }

protected:
    float           mEta;               // inner IOR / outer IOR
    float           mEtaInv;            // outer IOR / inner IOR
    float           mRoughnessAlpha;    // GGX isotropic roughness
};

