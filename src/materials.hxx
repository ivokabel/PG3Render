#pragma once 

#include "hard_config.hxx"

#include "physics.hxx"
#include "microfacet.hxx"
#include "sampling.hxx"
#include "math.hxx"
#include "spectrum.hxx"
#include "rng.hxx"
#include "types.hxx"

#include <memory>

#define MAT_BLOCKER_EPSILON    1e-5

// Various material IoRs and absorbances at roughly 590 nm
namespace SpectralData
{
    // Dielectrics
    const float kAirIor             = 1.000277f;
    const float kGlassCorningIor    = 1.510000f;

    // Conductors
    const float kCopperIor          = 0.468000f;
    const float kCopperAbsorb       = 2.810000f;
    const float kSilverIor          = 0.121000f;
    const float kSilverAbsorb       = 3.660000f;
    const float kGoldIor            = 0.236000f;
    const float kGoldAbsorb         = 2.960089f;
}

enum MaterialProperties
{
    kBsdfNone                       = 0x00000000,

    // Sides of surface which should be sampled by lights:

    kBsdfFrontSideLightSampling     = 0x00000001,
    // Only needed if the back surface is not occluded by the surrounding geometry
    // (e.g. single polygons or other non-watertight geometry)
    kBsdfBackSideLightSampling      = 0x00000002,
};

// Structure that holds data for evaluation and/or sampling of materials.
// In the case involving sampling many of the members have slightly different meaning.
class MaterialRecord
{
public:

    MaterialRecord(const Vec3f &aWil, const Vec3f &aWol) :
        wol(aWol),
        wil(aWil),
        mFlags(kNone),
        mOptDataMaskRequested(kOptNone),
        mOptDataMaskProvided(kOptNone)
    {}

    MaterialRecord(const Vec3f &aWol) :
        MaterialRecord(Vec3f(0.0f), aWol)
    {}

    bool IsBlocker() const
    {
        const float attenuationAndCos = attenuation.Max() * ThetaInCosAbs();
        return attenuationAndCos <= MAT_BLOCKER_EPSILON;
    }

    float ThetaInCos() const
    {
        return wil.z;
    }

    float ThetaInCosAbs() const
    {
        return std::abs(ThetaInCos());
    }

    // Not a Dirac component
    bool IsFiniteComp() const
    {
        return pdfW != Math::InfinityF();
    }

public:

    // Outgoing direction
    Vec3f       wol;

    // Incoming direction (either input or output parameter)
    Vec3f       wil;

    // BSDF value for the case of finite BSDF or attenuation for dirac BSDF.
    //
    // For sampling usage scenario it relates to the chosen BSDF component only,
    // otherwise it relates to the total finite BSDF.
    //
    // TODO: Rename to transmittance??
    SpectrumF   attenuation;

    // In finite BSDF cases it contains the angular PDF of all finite components summed up.
    // In infinite cases it equals Math::InfinityF().
    //
    // For sampling usage scenario it relates to the chosen BSDF component only,
    // otherwise it relates to the total finite BSDF.
    float       pdfW;

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
    float       compProb;

public:

    // General flags
    enum Flags
    {
        kNone           = 0x0000,
        kReflectionOnly = 0x0001, // Forbids refractions
    };

private:

    Flags mFlags;

public:

    void SetFlag(Flags aFlag)
    {
        mFlags = static_cast<Flags>(mFlags | aFlag);
    }

    bool IsFlagSet(Flags aFlag) const
    {
        return static_cast<Flags>(mFlags & aFlag) == aFlag;
    }

public:

    // Optional data flags
    enum OptDataType
    {
        kOptNone            = 0x0000,

        // Compute sampling PDF and component probability.
        // For sampling routines this flag does not have to be set - probabilities are always computed.
        kOptSamplingProbs   = 0x0001,

        kOptEta             = 0x0002,
        kOptHalfwayVec      = 0x0004,
        kOptReflectance     = 0x0008,
    };

private:

    OptDataType mOptDataMaskRequested;
    OptDataType mOptDataMaskProvided;

public:

    void RequestOptData(OptDataType aTypeMask)
    {
        mOptDataMaskRequested = static_cast<OptDataType>(mOptDataMaskRequested | aTypeMask);
    }

    bool AreOptDataRequested(OptDataType aTypeMask) const
    {
        return static_cast<OptDataType>(mOptDataMaskRequested & aTypeMask) == aTypeMask;
    }

    void SetAreOptDataProvided(OptDataType aTypeMask)
    {
        mOptDataMaskProvided = static_cast<OptDataType>(mOptDataMaskProvided | aTypeMask);

        // Clear the request to avoid unnecessary re-evaluation
        mOptDataMaskRequested = static_cast<OptDataType>(mOptDataMaskRequested & ~aTypeMask);
    }

    bool AreOptDataProvided(OptDataType aTypeMask) const
    {
        return static_cast<OptDataType>(mOptDataMaskProvided & aTypeMask) == aTypeMask;
    }

public:

    // Optional data (tied with mOptDataMaskRequested and mOptDataMaskProvided)

    // Optional eta (relative index of refraction).
    // Valid only if AreOptDataProvided(kOptEta) is true.
    float optEta;

    // Optional halfway vector (microfacet normal) for the given in-out directions.
    // Valid only if AreOptDataProvided(kOptHalfwayVec) is true.
    Vec3f optHalfwayVec;

    // Optional material reflectance
    // Valid only if AreOptDataProvided(kOptReflectance) is true.
    SpectrumF optReflectance;
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

    // Evaluates the BSDF and computes additional data requested through the MaterialRecord
    virtual void EvalBsdf(MaterialRecord  &oMatRecord) const = 0;

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord) const = 0;

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(const Vec3f &aWol) const = 0;

    virtual bool IsReflectanceZero() const = 0;

    virtual bool GetOptData(MaterialRecord &oMatRecord) const
    {
        oMatRecord; // unused param
        return false;
    }

protected:
    MaterialProperties  mProperties;
};

class LambertMaterial : public AbstractMaterial
{
public:
    LambertMaterial() :
        AbstractMaterial(kBsdfFrontSideLightSampling)
    {
        mReflectance.SetGreyAttenuation(0.0f);
    }

    LambertMaterial(const SpectrumF &aDiffuseReflectance) :
        AbstractMaterial(kBsdfFrontSideLightSampling)
    {
        mReflectance = aDiffuseReflectance;
    }

    SpectrumF EvalBsdf(
        const Vec3f& aWil,
        const Vec3f& aWol) const
    {
        if (aWil.z <= 0.f || aWol.z <= 0.f)
            return SpectrumF().MakeZero();

        return mReflectance / Math::kPiF; // TODO: Pre-compute?
    }

    virtual void EvalBsdf(MaterialRecord  &oMatRecord) const override
    {
        oMatRecord.attenuation = EvalBsdf(oMatRecord.wil, oMatRecord.wol);

        GetOptData(oMatRecord);
    }

    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord) const override
    {
        oMatRecord.wil         = Sampling::SampleCosHemisphereW(aRng.GetVec2f(), &oMatRecord.pdfW);
        oMatRecord.attenuation = EvalBsdf(oMatRecord.wil, oMatRecord.wol);
        oMatRecord.compProb    = 1.f;
    }

    float GetPdfW(const Vec3f &aWil) const
    {
        return Sampling::CosHemispherePdfW(aWil);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(const Vec3f &aWol) const override
    {
        aWol; // unused param

        // For conversion from spectral to scalar form we combine two strategies:
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
        static const float blendCoeff = 0.15f;
        const float probability =
              blendCoeff         * mReflectance.Luminance()
            + (1.f - blendCoeff) * mReflectance.Max();

        return Math::Clamp(probability, 0.f, 1.f);
    }

    virtual bool IsReflectanceZero() const override
    {
        return mReflectance.IsZero();
    }

    virtual bool GetOptData(MaterialRecord &oMatRecord) const override
    {
        if (oMatRecord.AreOptDataRequested(MaterialRecord::kOptSamplingProbs))
        {
            oMatRecord.pdfW     = GetPdfW(oMatRecord.wil);
            oMatRecord.compProb = 1.f;
            oMatRecord.SetAreOptDataProvided(MaterialRecord::kOptSamplingProbs);
        }

        if (oMatRecord.AreOptDataRequested(MaterialRecord::kOptReflectance))
        {
            oMatRecord.optReflectance = mReflectance;
            oMatRecord.SetAreOptDataProvided(MaterialRecord::kOptReflectance);
        }

        return true;
    }

protected:

    SpectrumF mReflectance;
};


class PhongMaterial : public AbstractMaterial
{
public:
    PhongMaterial() :
        AbstractMaterial(kBsdfFrontSideLightSampling)
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
        AbstractMaterial(kBsdfFrontSideLightSampling)
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
        return mDiffuseReflectance / Math::kPiF; // TODO: Pre-compute?
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
        const float constComponent = (mPhongExponent + 2.0f) / (2.0f * Math::kPiF); // TODO: Pre-compute?
        const Vec3f wrl = Geom::ReflectLocal(aWil);
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

    SpectrumF EvalBsdf(
        const Vec3f& aWil,
        const Vec3f& aWol) const
    {
        if (aWil.z <= 0.f || aWol.z <= 0.f)
            return SpectrumF().MakeZero();

        const SpectrumF diffuseComponent = EvalDiffuseComponent();
        const SpectrumF glossyComponent  = EvalGlossyComponent(aWil, aWol);

        return diffuseComponent + glossyComponent;
    }

    virtual void EvalBsdf(MaterialRecord &oMatRecord) const override
    {
        oMatRecord.attenuation = PhongMaterial::EvalBsdf(oMatRecord.wil, oMatRecord.wol);

        if (oMatRecord.AreOptDataRequested(MaterialRecord::kOptSamplingProbs))
        {
            GetWholeFiniteCompProbabilities(
                oMatRecord.pdfW,
                oMatRecord.compProb,
                oMatRecord.wol,
                oMatRecord.wil);
            oMatRecord.SetAreOptDataProvided(MaterialRecord::kOptSamplingProbs);
        }
    }

    // Generates a random BSDF sample.
    // It first randomly chooses a BSDF component and then it samples a random direction 
    // for this component.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord) const override
    {
        // Compute scalar reflectances. Replicated in GetWholeFiniteCompProbabilities()!
        const float diffuseReflectanceEst = GetDiffuseReflectance();
        const float cosThetaOut = std::max(oMatRecord.wol.z, 0.f);
        const float glossyReflectanceEst =
              GetGlossyReflectance()
            * (0.5f + 0.5f * cosThetaOut); // Attenuate to make it half the full reflectance at grazing angles.
                                           // Cheap, but relatively good approximation of actual glossy reflectance
                                           // (part of the glossy lobe can be under the surface).
        const float totalReflectance = (diffuseReflectanceEst + glossyReflectanceEst);

        if (totalReflectance < MAT_BLOCKER_EPSILON)
        {
            // Diffuse fallback for blocker materials
            oMatRecord.attenuation.MakeZero();
            oMatRecord.wil      = Sampling::SampleCosHemisphereW(aRng.GetVec2f(), &oMatRecord.pdfW);
            oMatRecord.compProb = 1.f;
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
            oMatRecord.wil = Sampling::SampleCosHemisphereW(aRng.GetVec2f());
        }
        else
        {
            // Glossy component sampling

            // Sample phong lobe in the canonical coordinate system (lobe around normal)
            const Vec3f canonicalSample =
                Sampling::SamplePowerCosHemisphereW(aRng.GetVec2f(), mPhongExponent/*, &oMatRecord.pdfW*/);

            // Rotate sample to mirror-reflection
            const Vec3f wrl = Geom::ReflectLocal(oMatRecord.wol);
            Frame lobeFrame;
            lobeFrame.SetFromZ(wrl);
            oMatRecord.wil = lobeFrame.ToWorld(canonicalSample);
        }

        oMatRecord.compProb = 1.f;

        // Get whole PDF value
        oMatRecord.pdfW =
            GetPdfW(oMatRecord.wol, oMatRecord.wil, diffuseReflectanceEst, glossyReflectanceEst);

        const float thetaInCos = oMatRecord.ThetaInCos();
        if (thetaInCos > 0.0f)
            // Above surface: Evaluate the whole BSDF
            oMatRecord.attenuation = PhongMaterial::EvalBsdf(oMatRecord.wil, oMatRecord.wol);
        else
            // Below surface: The sample is valid, it just has zero contribution
            oMatRecord.attenuation.MakeZero();
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
            return Sampling::CosHemispherePdfW(aWil);

        // Rotate the outgoing direction back to canonical lobe coordinates (lobe around normal)
        const Vec3f wrl = Geom::ReflectLocal(aWol);
        Frame lobeFrame;
        lobeFrame.SetFromZ(wrl);
        const Vec3f wiCanonical = lobeFrame.ToLocal(aWil);

        // Sum up both components' PDFs
        const float diffuseProbability = diffuseReflectanceEst / totalReflectance;
        const float glossyProbability  = glossyReflectanceEst  / totalReflectance;

        PG3_ASSERT_FLOAT_IN_RANGE(diffuseProbability + glossyProbability, 0.0f, 1.001f);
        return
              diffuseProbability * Sampling::CosHemispherePdfW(aWil)
            + glossyProbability  * Sampling::PowerCosHemispherePdfW(wiCanonical, mPhongExponent);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(const Vec3f &aWol) const override
    {
        // For conversion from spectral to scalar form we combine two strategies:
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
        static const float blendCoeff = 0.15f;
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

        return Math::Clamp(totalReflectance, 0.f, 1.f);
    }

    virtual bool IsReflectanceZero() const override
    {
        return mDiffuseReflectance.IsZero() && mPhongReflectance.IsZero();
    }

protected:

    void GetWholeFiniteCompProbabilities(
              float &oWholeFinCompPdfW,
              float &oWholeFinCompProbability,
        const Vec3f &aWol,
        const Vec3f &aWil) const
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

    SpectrumF   mDiffuseReflectance;
    SpectrumF   mPhongReflectance;
    float       mPhongExponent;
};


class AbstractSmoothMaterial : public AbstractMaterial
{
protected:
    AbstractSmoothMaterial() :
        AbstractMaterial(kBsdfNone) // Dirac materials don't work with light sampling
    {}

public:
    SpectrumF EvalBsdf(
        const Vec3f& aWil,
        const Vec3f& aWol) const
    {
        aWil; aWol; // unreferenced params
            
        // There is zero probability of hitting the only one or two valid combinations of 
        // incoming and outgoing directions which transfer light
        SpectrumF result;
        result.SetGreyAttenuation(0.0f);
        return result;
    }

    virtual void EvalBsdf(MaterialRecord  &oMatRecord) const override
    {
        oMatRecord.attenuation = EvalBsdf(oMatRecord.wil, oMatRecord.wol);

        if (oMatRecord.AreOptDataRequested(MaterialRecord::kOptSamplingProbs))
        {
            GetWholeFiniteCompProbabilities(
                oMatRecord.pdfW,
                oMatRecord.compProb,
                oMatRecord.wol,
                oMatRecord.wil);
            oMatRecord.SetAreOptDataProvided(MaterialRecord::kOptSamplingProbs);
        }
    }

protected:

    void GetWholeFiniteCompProbabilities(
              float &oWholeFinCompPdfW,
              float &oWholeFinCompProbability,
        const Vec3f &aWol,
        const Vec3f &aWil) const
    {
        aWil; aWol; // unreferenced params

        // There is no finite (non-dirac) component
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
        if (!Math::IsTiny(aOuterIor))
            mEta = aInnerIor / aOuterIor;
        else
            mEta = 1.0f;
        mAbsorbance = aAbsorbance;
    }

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord) const override
    {
        aRng; // unreferenced params

        oMatRecord.wil         = Geom::ReflectLocal(oMatRecord.wol);
        oMatRecord.pdfW        = Math::InfinityF();
        oMatRecord.compProb    = 1.f;

        // TODO: This may be cached (GetRRContinuationProb and SampleBsdf compute the same Fresnel 
        //       value), but it doesn't seem to be the bottleneck now. Postponing.

        const float reflectance = Physics::FresnelConductor(oMatRecord.ThetaInCos(), mEta, mAbsorbance);
        oMatRecord.attenuation.SetGreyAttenuation(reflectance);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(const Vec3f &aWol) const override
    {
        // We can use local z of outgoing direction, because it's equal to incoming direction z

        // TODO: This may be cached (GetRRContinuationProb and SampleBsdf compute the same Fresnel 
        //       value), but it doesn't seem to be the bottleneck now. Postponing.

        const float reflectance = Physics::FresnelConductor(aWol.z, mEta, mAbsorbance);
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
        if (!Math::IsTiny(aOuterIor))
            mEta = aInnerIor / aOuterIor;
        else
            mEta = 1.0f;

        if (!Math::IsTiny(aInnerIor))
            mEtaInv = aOuterIor / aInnerIor;
        else
            mEtaInv = 1.0f;
    }

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord) const override
    {
        const float fresnelRefl = Physics::FresnelDielectric(oMatRecord.wol.z, mEta);

        float attenuation;

        // Randomly choose between reflection or refraction
        const float rnd = aRng.GetFloat();
        if (rnd <= fresnelRefl)
        {
            // Reflect
            // This branch also handles TIR cases
            oMatRecord.wil = Geom::ReflectLocal(oMatRecord.wol);
            attenuation    = fresnelRefl;

            oMatRecord.compProb = fresnelRefl;
        }
        else
        {
            // Refract
            // TODO: local version of refract?
            // TODO: Re-use cosTrans from fresnel in refraction to save one sqrt?
            bool isDirInAboveSurface;
            Geom::Refract(oMatRecord.wil, isDirInAboveSurface, oMatRecord.wol, Vec3f(0.f, 0.f, 1.f), mEta);
            attenuation = 1.0f - fresnelRefl;

            // Radiance (de)compression
            attenuation *= isDirInAboveSurface ? Math::Sqr(mEta) : Math::Sqr(mEtaInv);

            oMatRecord.compProb = 1.0f - fresnelRefl;
        }

        oMatRecord.attenuation.SetGreyAttenuation(attenuation);
        oMatRecord.pdfW = Math::InfinityF();
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(const Vec3f &aWol) const override
    {
        aWol; // unused parameter

        return 1.0f; // reflectance is always 1 for dielectrics
    }

    virtual bool IsReflectanceZero() const override
    {
        return false;
    }

protected:
    float mEta;     // inner IOR / outer IOR
    float mEtaInv;  // outer IOR / inner IOR
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
        AbstractMaterial(kBsdfFrontSideLightSampling)
    {
        if (!Math::IsTiny(aOuterIor))
            mEta = aInnerIor / aOuterIor;
        else
            mEta = 1.0f;
        mAbsorbance = aAbsorbance;

        mRoughnessAlpha = Math::Clamp(aRoughnessAlpha, 0.001f, 1.0f);
    }

    virtual void EvalBsdf(MaterialRecord  &oMatRecord) const override
    {
        EvalContext ctx;
        InitEvalContext(ctx, oMatRecord.wil, oMatRecord.wol);

        oMatRecord.attenuation = EvalBsdf(ctx);

        GetOptData(oMatRecord, ctx);
    }

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord) const override
    {
        EvalContext ctx;

        ctx.wol = oMatRecord.wol;

        ctx.microfacetDir =
            Microfacet::SampleGgxVisibleNormals(ctx.wol, mRoughnessAlpha, aRng.GetVec2f());
        ctx.distrVal =
            Microfacet::DistributionGgx(ctx.microfacetDir, mRoughnessAlpha);

        bool isOutDirAboveMicrofacet;
        Geom::Reflect(ctx.wil, isOutDirAboveMicrofacet, ctx.wol, ctx.microfacetDir);
        oMatRecord.wil = ctx.wil;

        const float cosThetaOM = Dot(ctx.microfacetDir, ctx.wol);
        ctx.fresnelReflectance = Physics::FresnelConductor(cosThetaOM, mEta, mAbsorbance);

        GetWholeFiniteCompProbabilities(oMatRecord.pdfW, oMatRecord.compProb, ctx);

        if (!isOutDirAboveMicrofacet)
            // Outgoing dir is below microsurface: this happens occasionally because of numerical problems in the sampling routine.
            oMatRecord.attenuation.MakeZero();
        else if (ctx.wil.z < 0.0f)
            // Incoming dir is below relative surface: the sample is valid, it just has zero contribution
            oMatRecord.attenuation.MakeZero();
        else
            oMatRecord.attenuation = EvalBsdf(ctx);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(const Vec3f &aWol) const override
    {
        aWol; // unreferenced param

        return 1.0f; // TODO: GetReflectanceEst(aCtx);
    }

    virtual bool GetOptData(MaterialRecord &oMatRecord) const override
    {
        // TODO: Initializing the context just to obtain optional data might be inefficient!
        EvalContext ctx;
        InitEvalContext(ctx, oMatRecord.wil, oMatRecord.wol);

        GetOptData(oMatRecord, ctx);

        return true;
    }

    virtual bool IsReflectanceZero() const override
    {
        return false; // there should always be non-zero reflectance
    }

protected:

    class EvalContext
    {
    public:
        Vec3f wol;
        Vec3f wil;

        Vec3f microfacetDir;
        float fresnelReflectance;
        float distrVal;
    };

    void InitEvalContext(
        EvalContext &oCtx,
        const Vec3f &aWil,
        const Vec3f &aWol
        ) const
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aWil);
        PG3_ASSERT_VEC3F_NORMALIZED(aWol);

        oCtx.wol = aWol;
        oCtx.wil = aWil;

        oCtx.microfacetDir = Microfacet::HalfwayVectorReflectionLocal(oCtx.wil, oCtx.wol);

        oCtx.distrVal = Microfacet::DistributionGgx(oCtx.microfacetDir, mRoughnessAlpha);

        const float cosThetaOM = Dot(oCtx.microfacetDir, oCtx.wol);
        oCtx.fresnelReflectance = Physics::FresnelConductor(cosThetaOM, mEta, mAbsorbance);
    }

    SpectrumF EvalBsdf(EvalContext &aCtx) const
    {
        if (   aCtx.wil.z <= 0.f
            || aCtx.wol.z <= 0.f
            || Math::IsTiny(4.0f * aCtx.wil.z * aCtx.wol.z)) // BSDF denominator
        {
            return SpectrumF().MakeZero();
        }

        // Geometrical factor: Shadowing (incoming direction) * Masking (outgoing direction)
        const float shadowing = Microfacet::SmithMaskingFunctionGgx(aCtx.wil, aCtx.microfacetDir, mRoughnessAlpha);
        const float masking   = Microfacet::SmithMaskingFunctionGgx(aCtx.wol, aCtx.microfacetDir, mRoughnessAlpha);
        const float geometricalFactor = shadowing * masking;

        PG3_ASSERT_FLOAT_IN_RANGE(geometricalFactor, 0.0f, 1.0f);

        // The whole BSDF
        const float cosThetaI = aCtx.wil.z;
        const float cosThetaO = aCtx.wol.z;
        const float bsdfVal =
                (aCtx.fresnelReflectance * geometricalFactor * aCtx.distrVal)
              / (4.0f * cosThetaI * cosThetaO);

        PG3_ASSERT_FLOAT_NONNEGATIVE(bsdfVal);

        // TODO: Color (from fresnel?)
        SpectrumF result;
        result.SetGreyAttenuation(bsdfVal);
        return result;
    }

    void GetWholeFiniteCompProbabilities(
              float         &oWholeFinCompPdfW,
              float         &oWholeFinCompProbability,
        const EvalContext   &aCtx) const
    {
        oWholeFinCompProbability = 1.0f;

        if (aCtx.wol.z < 0.f)
        {
            oWholeFinCompPdfW = 0.0f;
            return;
        }

        const float normalPdf =
            Microfacet::GgxSamplingPdfVisibleNormals(
                aCtx.wol, aCtx.microfacetDir, aCtx.distrVal, mRoughnessAlpha);
        const float reflectionJacobian =
            Microfacet::ReflectionJacobian(aCtx.wol, aCtx.microfacetDir);
        oWholeFinCompPdfW = normalPdf * reflectionJacobian;
    }

    void GetOptData(
        MaterialRecord      &oMatRecord,
        const EvalContext   &aCtx) const
    {
        if (oMatRecord.AreOptDataRequested(MaterialRecord::kOptSamplingProbs))
        {
            GetWholeFiniteCompProbabilities(oMatRecord.pdfW, oMatRecord.compProb, aCtx);
            oMatRecord.SetAreOptDataProvided(MaterialRecord::kOptSamplingProbs);
        }

        if (oMatRecord.AreOptDataRequested(MaterialRecord::kOptReflectance))
        {
            oMatRecord.optReflectance = GetReflectanceEst(aCtx);
            oMatRecord.SetAreOptDataProvided(MaterialRecord::kOptReflectance);
        }
    }

    SpectrumF GetReflectanceEst(const EvalContext &aCtx) const
    {
        // We estimate the whole BRDF reflectance with the current microfacet Fresnel reflectance
        return aCtx.fresnelReflectance; // gets converted to spectrum...
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
                  kBsdfFrontSideLightSampling
                | (aAllowBackSideLightSampling ? kBsdfBackSideLightSampling : 0)))
    {
        if (!Math::IsTiny(aOuterIor))
        {
            mEta    = aInnerIor / aOuterIor;
            mEtaInv = aOuterIor / aInnerIor;
        }
        else
        {
            mEta    = 1.0f;
            mEtaInv = 1.0f;
        }

        mRoughnessAlpha = Math::Clamp(aRoughnessAlpha, 0.001f, 1.0f);
    }

    virtual void EvalBsdf(MaterialRecord  &oMatRecord) const override
    {
        EvalContext ctx;
        InitEvalContext(ctx, oMatRecord.wil, oMatRecord.wol);

        EvalBsdf(oMatRecord, ctx);

        GetOptData(oMatRecord, ctx);
    }

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord) const override
    {
        EvalContext ctx;

        // WARNING: Partially replicated in InitEvalContext()!

        // Make sure that the underlying code always deals with outgoing direction which is above the surface
        ctx.isOutDirFromBelow   = (oMatRecord.wol.z < 0.f);
        ctx.wolSwitched         = oMatRecord.wol * (ctx.isOutDirFromBelow ? -1.0f : 1.0f);
        ctx.etaSwitched         = (ctx.isOutDirFromBelow ? mEtaInv : mEta);
        ctx.etaInvSwitched      = (ctx.isOutDirFromBelow ? mEta : mEtaInv);

        ctx.microfacetDirSwitched =
            Microfacet::SampleGgxVisibleNormals(ctx.wolSwitched, mRoughnessAlpha, aRng.GetVec2f());
        ctx.distrVal =
            Microfacet::DistributionGgx(ctx.microfacetDirSwitched, mRoughnessAlpha);

        float thetaInCosAbs;

        const bool reflectionOnly = oMatRecord.IsFlagSet(MaterialRecord::kReflectionOnly);

        // Randomly choose between reflection or refraction
        const float cosThetaOM = Dot(ctx.microfacetDirSwitched, ctx.wolSwitched);
        ctx.fresnelReflectance = Physics::FresnelDielectric(cosThetaOM, ctx.etaSwitched);
        bool isOutDirAboveMicrofacet;
        if (reflectionOnly || (aRng.GetFloat() <= ctx.fresnelReflectance))
        {
            // This branch also handles TIR cases
            Geom::Reflect(ctx.wilSwitched, isOutDirAboveMicrofacet, ctx.wolSwitched, ctx.microfacetDirSwitched);
            thetaInCosAbs = ctx.wilSwitched.z;
            ctx.isReflection = true;
        }
        else
        {
            // TODO: Re-use cosTrans from fresnel in refraction to save one sqrt?
            Geom::Refract(ctx.wilSwitched, isOutDirAboveMicrofacet, ctx.wolSwitched, ctx.microfacetDirSwitched, ctx.etaSwitched);
            thetaInCosAbs = -ctx.wilSwitched.z;
            ctx.isReflection = false;
        }

        // Switch up-down back if necessary
        oMatRecord.wil = ctx.wilSwitched * (ctx.isOutDirFromBelow ? -1.0f : 1.0f);

        GetWholeFiniteCompProbabilities(oMatRecord.pdfW, oMatRecord.compProb, ctx, oMatRecord);

        if (!isOutDirAboveMicrofacet)
            // Outgoing dir is below microsurface: this happens occasionally because of numerical problems in the sampling routine.
            oMatRecord.attenuation.MakeZero();
        else if (thetaInCosAbs < 0.0f)
            // Incoming dir is below relative surface: the sample is valid, it just has zero contribution
            oMatRecord.attenuation.MakeZero();
        else
            EvalBsdf(oMatRecord, ctx);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(const Vec3f &aWol) const override
    {
        aWol; // unreferenced param

        return 1.0f;
    }

    virtual bool IsReflectanceZero() const override
    {
        return false; // there always is non-zero reflectance
    }

    virtual bool GetOptData(MaterialRecord &oMatRecord) const override
    {
        // TODO: Initializing the context just to obtain optional data might be inefficient!
        EvalContext ctx;
        InitEvalContext(ctx, oMatRecord.wil, oMatRecord.wol);

        GetOptData(oMatRecord, ctx);

        return true;
    }

protected:

    class EvalContext
    {
    public:
        Vec3f wolSwitched;
        Vec3f wilSwitched;

        bool isOutDirFromBelow;
        bool isReflection;
        float etaSwitched;
        float etaInvSwitched;

        Vec3f microfacetDirSwitched;
        float fresnelReflectance;
        float distrVal;
    };

    void InitEvalContext(
        EvalContext &oCtx,
        const Vec3f &aWil,
        const Vec3f &aWol
        ) const
    {
        // Warning: Partially replicated in SampleBsdf()!

        PG3_ASSERT_VEC3F_NORMALIZED(aWil);
        PG3_ASSERT_VEC3F_NORMALIZED(aWol);

        // Make sure that the underlying code always deals with outgoing direction which is above the surface
        oCtx.isOutDirFromBelow  = (aWol.z < 0.f);
        oCtx.isReflection       = (aWil.z > 0.f == aWol.z > 0.f); // on the same side of the surface
        oCtx.wolSwitched        = aWol * (oCtx.isOutDirFromBelow ? -1.0f : 1.0f);
        oCtx.wilSwitched        = aWil * (oCtx.isOutDirFromBelow ? -1.0f : 1.0f);
        oCtx.etaSwitched        = (oCtx.isOutDirFromBelow ? mEtaInv : mEta);
        oCtx.etaInvSwitched     = (oCtx.isOutDirFromBelow ? mEta : mEtaInv);

        if (oCtx.isReflection)
            oCtx.microfacetDirSwitched =
                Microfacet::HalfwayVectorReflectionLocal(
                    oCtx.wilSwitched, oCtx.wolSwitched);
        else
            // Since the incident direction is below geometrical surface, we use inverse eta
            oCtx.microfacetDirSwitched =
                Microfacet::HalfwayVectorRefractionLocal(
                    oCtx.wilSwitched, oCtx.wolSwitched, oCtx.etaInvSwitched);

        oCtx.distrVal =
            Microfacet::DistributionGgx(oCtx.microfacetDirSwitched, mRoughnessAlpha);
        const float cosThetaOM  = Dot(oCtx.microfacetDirSwitched, oCtx.wolSwitched);
        oCtx.fresnelReflectance = Physics::FresnelDielectric(cosThetaOM, oCtx.etaSwitched);
    }

    void EvalBsdf(
        MaterialRecord  &oMatRecord,
        EvalContext     &aCtx) const
    {
        const float cosThetaIAbs = std::abs(aCtx.wilSwitched.z);
        const float cosThetaOAbs = std::abs(aCtx.wolSwitched.z);

        const bool reflectionOnly = oMatRecord.IsFlagSet(MaterialRecord::kReflectionOnly);

        if (   (!aCtx.isReflection && Math::IsTiny(cosThetaIAbs * cosThetaOAbs))
            || ( aCtx.isReflection && Math::IsTiny(4.0f * cosThetaIAbs * cosThetaOAbs))
            || (reflectionOnly && !aCtx.isReflection))
        {
            oMatRecord.attenuation.MakeZero();
            return;
        }

        // Geometrical factor: Shadowing (incoming direction) * Masking (outgoing direction)
        const float shadowing =
            Microfacet::SmithMaskingFunctionGgx(
                aCtx.wilSwitched, aCtx.microfacetDirSwitched, mRoughnessAlpha);
        const float masking =
            Microfacet::SmithMaskingFunctionGgx(
                aCtx.wolSwitched, aCtx.microfacetDirSwitched, mRoughnessAlpha);
        const float geometricalFactor = shadowing * masking;

        PG3_ASSERT_FLOAT_IN_RANGE(geometricalFactor, 0.0f, 1.0f);

        // The whole BSDF
        float bsdfVal;
        const float fresnel = aCtx.fresnelReflectance;
        if (aCtx.isReflection)
        {
            bsdfVal =
                  (fresnel * geometricalFactor * aCtx.distrVal)
                / (4.0f * cosThetaIAbs * cosThetaOAbs);
        }
        else
        {
            const float cosThetaMI = Dot(aCtx.microfacetDirSwitched, aCtx.wilSwitched);
            const float cosThetaMO = Dot(aCtx.microfacetDirSwitched, aCtx.wolSwitched);
            bsdfVal =
                  (   (std::abs(cosThetaMI) * std::abs(cosThetaMO))
                    / (cosThetaIAbs * cosThetaOAbs))
                * (   (Math::Sqr(aCtx.etaInvSwitched) * (1.0f - fresnel) * geometricalFactor * aCtx.distrVal)
                    / (Math::Sqr(cosThetaMI + aCtx.etaInvSwitched * cosThetaMO)));

            // TODO: What if (cosThetaMI + etaInvSwitched * cosThetaMO) is close to zero??

            bsdfVal *= Math::Sqr(aCtx.etaSwitched); // radiance (solid angle) (de)compression
        }

        PG3_ASSERT_FLOAT_NONNEGATIVE(bsdfVal);

        // TODO: Color (from fresnel?)
        oMatRecord.attenuation.SetGreyAttenuation(bsdfVal);
    }

    void GetWholeFiniteCompProbabilities(
              float             &oWholeFinCompPdfW,
              float             &oWholeFinCompProbability,
        const EvalContext       &aCtx,
        const MaterialRecord    &aMatRecord) const
    {
        oWholeFinCompProbability = 1.0f;

        const bool reflectionOnly = aMatRecord.IsFlagSet(MaterialRecord::kReflectionOnly);

        if (   (aCtx.wolSwitched.z < 0.f)
            || (reflectionOnly && !aCtx.isReflection))
        {
            oWholeFinCompPdfW = 0.0f;
            return;
        }

        const float visNormalsPdf =
            Microfacet::GgxSamplingPdfVisibleNormals(
                aCtx.wolSwitched, aCtx.microfacetDirSwitched, aCtx.distrVal, mRoughnessAlpha);

        const float transfJacobian = [&]()
        {
            if (aCtx.isReflection)
                return Microfacet::ReflectionJacobian(
                    aCtx.wilSwitched, aCtx.microfacetDirSwitched);
            else
                return Microfacet::RefractionJacobian(
                    aCtx.wolSwitched, aCtx.wilSwitched, aCtx.microfacetDirSwitched, aCtx.etaInvSwitched);
        }();

        const float compProbability =
            reflectionOnly      ? 1.f :
            aCtx.isReflection   ? aCtx.fresnelReflectance :
                                  (1.f - aCtx.fresnelReflectance);

        oWholeFinCompPdfW = visNormalsPdf * transfJacobian * compProbability;

        PG3_ASSERT_FLOAT_NONNEGATIVE(oWholeFinCompPdfW);
    }

    void GetOptData(
        MaterialRecord      &oMatRecord,
        const EvalContext   &aCtx) const
    {
        if (oMatRecord.AreOptDataRequested(MaterialRecord::kOptSamplingProbs))
        {
            GetWholeFiniteCompProbabilities(oMatRecord.pdfW, oMatRecord.compProb, aCtx, oMatRecord);
            oMatRecord.SetAreOptDataProvided(MaterialRecord::kOptSamplingProbs);
        }

        if (oMatRecord.AreOptDataRequested(MaterialRecord::kOptEta))
        {
            oMatRecord.optEta = mEta;
            oMatRecord.SetAreOptDataProvided(MaterialRecord::kOptEta);
        }

        if (oMatRecord.AreOptDataRequested(MaterialRecord::kOptHalfwayVec))
        {
            oMatRecord.optHalfwayVec =
                  aCtx.microfacetDirSwitched
                * (aCtx.isOutDirFromBelow ? -1.0f : 1.0f);
            oMatRecord.SetAreOptDataProvided(MaterialRecord::kOptHalfwayVec);
        }
    }

protected:

    float           mEta;               // inner IOR / outer IOR
    float           mEtaInv;            // outer IOR / inner IOR
    float           mRoughnessAlpha;    // GGX isotropic roughness
};


class WeidlichWilkie2LayeredMaterial : public AbstractMaterial
{
public:

    WeidlichWilkie2LayeredMaterial(
        AbstractMaterial    *aOuterLayerMaterial,
        AbstractMaterial    *aInnerLayerMaterial,
        SpectrumF            aMediumAttenuationCoeff, // 0 means no attenuation
        float                aMediumThickness)
        :
        AbstractMaterial(MaterialProperties(kBsdfFrontSideLightSampling)), // TODO
        mOuterLayerMaterial(aOuterLayerMaterial),
        mInnerLayerMaterial(aInnerLayerMaterial),
        mMediumAttCoeff(aMediumAttenuationCoeff),
        mMediumThickness(aMediumThickness)
    {
        PG3_ASSERT(mOuterLayerMaterial.get() != nullptr);
        PG3_ASSERT(mInnerLayerMaterial.get() != nullptr);
        PG3_ASSERT_VEC3F_VALID(aMediumAttenuationCoeff);
    }

    virtual void EvalBsdf(MaterialRecord &oMatRecord) const override
    {
        const Vec3f wil = oMatRecord.wil;
        const Vec3f wol = oMatRecord.wol;
            
        oMatRecord.compProb = 1.f;

        // No transmission below horizon (both input and output)
        if (oMatRecord.wil.z <= 0.f || oMatRecord.wol.z <= 0.f)
        {
            oMatRecord.attenuation.MakeZero();
            oMatRecord.pdfW = 0.f;
            // This also handles TIR refraction scenarios - we expect the rest of the system
            // to work properly even if we return incorrect (zero) PDF
            return;
        }

        const bool computeProbs = oMatRecord.AreOptDataRequested(MaterialRecord::kOptSamplingProbs);

        // Outer layer reflection
        MaterialRecord outerMatRecRefl(wil, wol);
        outerMatRecRefl.RequestOptData(MaterialRecord::kOptEta);
        //outerMatRecRefl.RequestOptData(MaterialRecord::kOptHalfwayVec);
        outerMatRecRefl.SetFlag(MaterialRecord::kReflectionOnly);
        if (computeProbs)
            outerMatRecRefl.RequestOptData(MaterialRecord::kOptSamplingProbs);
        mOuterLayerMaterial->EvalBsdf(outerMatRecRefl);

        PG3_ASSERT(outerMatRecRefl.AreOptDataProvided(MaterialRecord::kOptEta));
        //PG3_ASSERT(outerMatRecRefl.AreOptDataProvided(MaterialRecord::kOptHalfwayVec));

        const float outerEta = outerMatRecRefl.optEta;
        const SpectrumF outerMatAttenuation = outerMatRecRefl.attenuation;

        // Refracted directions
        Vec3f wilRefract, wolRefract;
        Geom::Refract(wilRefract, wil, Vec3f(0.f, 0.f, 1.f), outerEta);
        Geom::Refract(wolRefract, wol, Vec3f(0.f, 0.f, 1.f), outerEta);
        //Geom::Refract(wilRefract, wil, outerMatRecRefl.optHalfwayVec, outerEta);
        //Geom::Refract(wolRefract, wol, outerMatRecRefl.optHalfwayVec, outerEta);
        // TODO: Are refracted directions always valid??

        const float wilFresnelRefl  = Physics::FresnelDielectric(wil.z, outerEta);
        const float wolFresnelRefl  = Physics::FresnelDielectric(wol.z, outerEta);
        const float wilFresnelTrans = 1.f - wilFresnelRefl;
        const float wolFresnelTrans = 1.f - wolFresnelRefl;

        // Medium attenuation
        const float clampedCosO      = std::max(wol.z, 0.0001f);
        const float clampedCosI      = std::max(wil.z, 0.0001f);
        const float mediumPathLength = mMediumThickness * (1.f / clampedCosO + 1.f / clampedCosI);
        const SpectrumF mediumTrans  = Physics::BeerLambert(mMediumAttCoeff, mediumPathLength);

        // Evaluate inner layer
        MaterialRecord innerMatRec(-wilRefract, -wolRefract);
        if (computeProbs)
            innerMatRec.RequestOptData(MaterialRecord::kOptSamplingProbs);
        innerMatRec.RequestOptData(MaterialRecord::kOptReflectance);
        mInnerLayerMaterial->EvalBsdf(innerMatRec);

        PG3_ASSERT(innerMatRec.AreOptDataProvided(MaterialRecord::kOptReflectance));

        const SpectrumF innerMatAttenuation =
              innerMatRec.attenuation
            * wilFresnelTrans * wolFresnelTrans // refraction transmissions
            * Math::Sqr(1.f / outerEta)         // incident solid angle (de)compression
            * mediumTrans;

        //oMatRecord.attenuation = outerMatAttenuation; // debug
        //oMatRecord.attenuation = innerMatAttenuation; // debug
        oMatRecord.attenuation = outerMatAttenuation + innerMatAttenuation;

        // Sampling PDF
        if (computeProbs)
        {
            const float innerReflectance = innerMatRec.optReflectance.Luminance();

            // Medium attenuation estimate
            // We estimate the incoming path length using the outgoing one
            const float mediumPathLengthEst = mMediumThickness * (1.f / clampedCosO * 2.f);
            const SpectrumF mediumTransEst  = Physics::BeerLambert(mMediumAttCoeff, mediumPathLengthEst);

            const float outerCompContrEst = wolFresnelRefl;
            const float innerCompContrEst = innerReflectance * wolFresnelTrans * mediumTransEst.Luminance();
            const float totalContrEst     = outerCompContrEst + innerCompContrEst;

            PG3_ASSERT_FLOAT_LARGER_THAN(totalContrEst, 0.001f);

            const float outerPdfWeight = outerCompContrEst / totalContrEst;
            const float innerPdfWeight = innerCompContrEst / totalContrEst;

            const auto outerPdf = outerMatRecRefl.pdfW;
            const auto innerPdf =
                  innerMatRec.pdfW
                * Math::Sqr(1.f / outerEta) * (wil.z / -wilRefract.z); // solid angle (de)compression

            //oMatRecord.pdfW = outerPdf; // debug
            //oMatRecord.pdfW  = innerPdf; // debug
            oMatRecord.pdfW = outerPdf * outerPdfWeight + innerPdf * innerPdfWeight;

            oMatRecord.SetAreOptDataProvided(MaterialRecord::kOptSamplingProbs);
        }
    }

    // Generates a random BSDF sample.
    virtual void SampleBsdf(
        Rng             &aRng,
        MaterialRecord  &oMatRecord) const override
    {
        // Component contribution estimation
        // TODO: Precompute to contex!

        MaterialRecord outerMatRecord(oMatRecord.wil, oMatRecord.wol);
        outerMatRecord.RequestOptData(MaterialRecord::kOptEta);
        //outerMatRecord.RequestOptData(MaterialRecord::kOptHalfwayVec);
        mOuterLayerMaterial->GetOptData(outerMatRecord);

        PG3_ASSERT(outerMatRecord.AreOptDataProvided(MaterialRecord::kOptEta));
        //PG3_ASSERT(outerMatRecord.AreOptDataProvided(MaterialRecord::kOptHalfwayVec));

        MaterialRecord innerMatRecord(oMatRecord.wil, oMatRecord.wol);
        innerMatRecord.RequestOptData(MaterialRecord::kOptReflectance);
        mInnerLayerMaterial->GetOptData(innerMatRecord);

        PG3_ASSERT(innerMatRecord.AreOptDataProvided(MaterialRecord::kOptReflectance));

        const float outerEta            = outerMatRecord.optEta;
        const float wolFresnelRefl      = Physics::FresnelDielectric(oMatRecord.wol.z, outerEta);
        const float wolFresnelTrans     = 1.f - wolFresnelRefl;
        const float innerReflectance    = innerMatRecord.optReflectance.Luminance();

        // Medium attenuation estimate
        // We estimate the incoming path length using the outgoing one
        const float clampedCosO         = std::max(oMatRecord.wol.z, 0.0001f);
        const float mediumPathLengthEst = mMediumThickness * (1.f / clampedCosO * 2.f);
        const SpectrumF mediumTransEst  = Physics::BeerLambert(mMediumAttCoeff, mediumPathLengthEst);

        const float outerCompContrEst = wolFresnelRefl;
        const float innerCompContrEst = innerReflectance * wolFresnelTrans * mediumTransEst.Luminance();
        const float totalContrEst     = outerCompContrEst + innerCompContrEst;

        PG3_ASSERT_FLOAT_LARGER_THAN(totalContrEst, 0.001f);

        // Pick and sample one component
        const float randomVal = aRng.GetFloat() * totalContrEst;
        if (randomVal < outerCompContrEst)
        {
            // Outer component
            outerMatRecord.SetFlag(MaterialRecord::kReflectionOnly);
            mOuterLayerMaterial->SampleBsdf(aRng, outerMatRecord);

            PG3_ASSERT(oMatRecord.wil.z >= -0.001f);

            oMatRecord.wil = outerMatRecord.wil;
        }
        else
        {
            // Inner component

            // Compute refracted outgoing direction
            Vec3f wolRefract;
            Geom::Refract(wolRefract, oMatRecord.wol, Vec3f(0.f, 0.f, 1.f), outerEta);
            //Geom::Refract(wolRefract, aWol, oMatRecord.optHalfwayVec, outerEta);
            // TODO: Is refracted direction always valid??

            // Sample inner BRDF
            MaterialRecord innerMatRecord(-wolRefract);
            innerMatRecord.RequestOptData(MaterialRecord::kOptSamplingProbs); // debug
            mInnerLayerMaterial->SampleBsdf(aRng, innerMatRecord);

            // Refract through the upper layer
            // This can yield directions under surface due to TIR!
            Geom::Refract(oMatRecord.wil, -innerMatRecord.wil, Vec3f(0.f, 0.f, 1.f), outerEta);
        }

        // Evaluate BRDF & PDF
        oMatRecord.RequestOptData(MaterialRecord::kOptSamplingProbs);
        EvalBsdf(oMatRecord);
    }

    // Computes the probability of surviving for Russian roulette in path tracer
    // based on the material reflectance.
    virtual float GetRRContinuationProb(const Vec3f &aWol) const override
    {
        aWol; // unreferenced param

        PG3_ERROR_NOT_IMPLEMENTED("");

        return 1.0f;
    }

    virtual bool IsReflectanceZero() const override
    {
        PG3_ERROR_NOT_IMPLEMENTED("");

        return false; // debug
    }

protected:

    std::unique_ptr<AbstractMaterial>   mOuterLayerMaterial;
    std::unique_ptr<AbstractMaterial>   mInnerLayerMaterial;
    SpectrumF                           mMediumAttCoeff;
    float                               mMediumThickness;
};

