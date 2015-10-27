#pragma once 

#include "hardsettings.hxx"
#include "math.hxx"
#include "spectrum.hxx"
#include "rng.hxx"
#include "types.hxx"

#define MAT_BLOCKER_EPSILON    1e-5

// Various material IoRs and absorbances at roughly 590 nm
#define MAT_AIR_IOR            1.000277f
#define MAT_COPPER_IOR         0.468000f
#define MAT_COPPER_ABSORBANCE  2.810000f
#define MAT_SILVER_IOR         0.121000f
#define MAT_SILVER_ABSORBANCE  3.660000f
#define MAT_GOLD_IOR           0.236000f
#define MAT_GOLD_ABSORBANCE    2.960089f

class BRDFSample
{
public:
    // BRDF attenuation * cosine theta_in
    SpectrumF   mSample;

    // Angular PDF of the sample. In finite BRDF cases, it contains the whole PDF of all finite
    // components and is already pre-multiplied by probability of this case.
    float       mPdfW;

    // Probability of picking the additive BRDF component which generated this sample.
    // The component can be an infinite pdf (dirac impulse) BRDF, e.g. Fresnel, 
    // or total finite BRDF. Finite BRDFs cannot be sampled separatelly due to MIS; 
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

    virtual float GetPdfW(
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
        // Compute scalar reflectances. Replicated in GetPdfW()!
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

    virtual float GetPdfW(
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

        return GetPdfW(aWol, aWil, diffuseReflectanceEst, glossyReflectanceEst);
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
                                           // Replicated in SampleBrdf() and GetPdfW()!
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

class SmoothConductorMaterial : public AbstractMaterial
{
public:
    SmoothConductorMaterial(float aInnerIor /* n */, float aOuterIor, float aAbsorbance /* k */)
    {
        if (!IsTiny(aOuterIor))
            mEta = aInnerIor / aOuterIor;
        else
            mEta = 1.0f;
        mAbsorbance = aAbsorbance;
    }

    virtual SpectrumF EvalBrdf(
        const Vec3f& aWil,
        const Vec3f& aWol
        ) const override
    {
        aWil; aWol; // unreferenced params
            
        // There is zero probability of hitting the only valid combination of 
        // incoming and outgoing directions that transfers light
        SpectrumF result;
        result.SetGreyAttenuation(0.0f);
        return result;
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

    virtual float GetPdfW(
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const override
    {
        // unreferenced params
        aWil;
        aWol;

        // There is zero probability of hitting the only valid combination of 
        // incoming and outgoing directions that transfers light
        return 0.0f;
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
        aWol; // unreferenced params

#if defined MATERIAL_GGX_SAMPLING_COS

        oBrdfSample.mWil =
            SampleCosHemisphereW(aRng.GetVec2f(), &oBrdfSample.mPdfW); // debug: cosine-weighted sampling

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
            SampleGgxNormals(aWol, mRoughnessAlpha, aRng.GetVec2f(), oBrdfSample.mWil);

        // TODO: Re-use already evaluated data? half-way vector, distribution value?
        oBrdfSample.mPdfW = MicrofacetGGXConductorMaterial::GetPdfW(aWol, oBrdfSample.mWil);

        const float thetaCosIn = oBrdfSample.mWil.z;
        if ((thetaCosIn > 0.0f) && isAboveMicrofacet)
            // Above surface: Evaluate the whole BRDF
            oBrdfSample.mSample =
                MicrofacetGGXConductorMaterial::EvalBrdf(oBrdfSample.mWil, aWol) * thetaCosIn;
        else
            // Below surface: The sample is valid, it just has zero contribution
            oBrdfSample.mSample.MakeZero();

#elif defined MATERIAL_GGX_SAMPLING_VISIBLE_NORMALS

        //SampleGgxVisibleNormals(..., const Vec2f &aSample, ...)
        const Vec2f aSample = aRng.GetVec2f();
        Vec3f microfacetDir;
        {
            // Stretch Wol to canonical, unit roughness space
            Vec3f wolStretch =
                Normalize(Vec3f(aWol.x * mRoughnessAlpha, aWol.y * mRoughnessAlpha, aWol.z));

            float thetaWolStretch = 0.0f;
            float phiWolStretch   = 0.0f;
            if (wolStretch.z < 0.999f)
            {
                thetaWolStretch = std::acos(wolStretch.z);
                phiWolStretch   = std::atan2(wolStretch.y, wolStretch.x);
            }
            const float sinPhi = std::sin(phiWolStretch);
            const float cosPhi = std::cos(phiWolStretch);

            // Sample GGX P^{22}_{\omega_o}(slope, 1, 1) for given incident direction theta
            Vec2f slopeStretch;
            //Sample11(Vec2f &aSlope, const float aThetaI, const Vec2f &aSample);
            {
                // params...
                Vec2f &aSlope = slopeStretch;
                float aThetaI = thetaWolStretch;


                if (aThetaI < 0.0001f)
                {
                    // Normal incidence - simple hemispherical sampling?
                    const float sampleXClamped  = std::min(aSample.x, 0.9999f);
                    const float radius          = SafeSqrt(sampleXClamped / (1 - sampleXClamped));
                    const float phi             = 2 * PI_F * aSample.y;
                    const float sinPhi          = std::sin(phi);
                    const float cosPhi          = std::cos(phi);
                    aSlope = Vec2f(radius * cosPhi, radius * sinPhi);

                    PG3_ASSERT_VEC2F_VALID(aSlope);
                }
                else
                {
                    const float tanThetaI    = std::tan(aThetaI);
                    const float tanThetaIInv = 1.0f / tanThetaI;
                    const float G1 =
                        2.0f / (1.0f + SafeSqrt(1.0f + 1.0f / (tanThetaIInv * tanThetaIInv)));
                    //const float G1xxx =
                    //    2.0f / (1.0f + SafeSqrt(1.0f + tanThetaI * tanThetaI));

                    // Sample x dimension (marginalized PDF - can be sampled directly via CDF^-1)
                    float A = 2.0f * aSample.x / G1 - 1.0f; // TODO: dividing by G1, which can be zero?!?
                    if (std::abs(A) == 1.0f)
                        A -= SignNum(A) * 1e-4f; // avoid division by zero later
                    const float B = tanThetaI;
                    const float tmpFract = 1.0f / (A * A - 1.0f);
                    const float D = SafeSqrt(B * B * tmpFract * tmpFract - (A * A - B * B) * tmpFract);
                    const float slopeX1 = B * tmpFract - D;
                    const float slopeX2 = B * tmpFract + D;
                    aSlope.x = (A < 0.0f || slopeX2 > (1.0f / tanThetaI)) ? slopeX1 : slopeX2;

                    PG3_ASSERT_FLOAT_VALID(aSlope.x);

                    // Sample y dimension
                    // Using conditional PDF; however, CDF is not directly invertible, so we use rational fit of CDF^-1.
                    // We sample just one half-space - PDF is symmetrical in y dimension.
                    // We use improved fit from Mitsuba renderer rather than the original fit from the paper.
                    float ySign;
                    float yHalfSample;
                    if (aSample.y > 0.5f) // pick one positive/negative interval
                    {
                        ySign       = 1.0f;
                        yHalfSample = 2.0f * (aSample.y - 0.5f);
                    }
                    else
                    {
                        ySign       = -1.0f;
                        yHalfSample = 2.0f * (0.5f - aSample.y);
                    }
                    const float z =
                            (yHalfSample * (yHalfSample * (yHalfSample *
                             -(float)0.365728915865723 + (float)0.790235037209296) - 
                             (float)0.424965825137544) + (float)0.000152998850436920)
                        /
                            (yHalfSample * (yHalfSample * (yHalfSample * (yHalfSample *
                             (float)0.169507819808272 - (float)0.397203533833404) - 
                             (float)0.232500544458471) + (float)1) - (float)0.539825872510702);
                    aSlope.y = ySign * z * std::sqrt(1.0f + aSlope.x * aSlope.x);

                    PG3_ASSERT_FLOAT_VALID(aSlope.y);
                }
            }

            PG3_ASSERT_VEC2F_VALID(slopeStretch);

            // Rotate
            slopeStretch = Vec2f(
                slopeStretch.x * cosPhi - slopeStretch.y * sinPhi,
                slopeStretch.x * sinPhi + slopeStretch.y * cosPhi);

            // Unstretch back to non-unit roughness
            Vec2f slope(
                slopeStretch.x * mRoughnessAlpha,
                slopeStretch.y * mRoughnessAlpha);

            PG3_ASSERT_VEC2F_VALID(slope);

            // Compute normal
            const float slopeLengthInv = 1.0f / Vec3f(slope.x, slope.y, 1.0f).Length();
            microfacetDir = Vec3f(
                -slope.x * slopeLengthInv,
                -slope.y * slopeLengthInv,
                slopeLengthInv);

            PG3_ASSERT_FLOAT_VALID(slopeLengthInv);
            PG3_ASSERT_VEC3F_VALID(microfacetDir);
        }

        // Reflect from the microfacet
        Reflect(aWol, microfacetDir, oBrdfSample.mWil);

        // TODO: Assert below microsurface surface
        const float thetaMOCos = Dot(aWol, microfacetDir);

        static uint32_t validCount = 0;
        static uint32_t belowSurfaceCount = 0;
        static uint32_t belowMFacetCount = 0;

        const float thetaCosIn = oBrdfSample.mWil.z;
        if (thetaMOCos <= 0.0f)
        {
            oBrdfSample.mSample.MakeZero(); // Outgoing dir is below microsurface!
            belowMFacetCount++;
        }
        else if (thetaCosIn <= 0.0f)
        {
            // Incoming dir is below surface: The sample is valid, it just has zero contribution
            oBrdfSample.mSample.MakeZero();
            belowSurfaceCount++;
        }
        else
        {
            // Above surface: Evaluate the whole BRDF
            oBrdfSample.mSample =
                MicrofacetGGXConductorMaterial::EvalBrdf(oBrdfSample.mWil, aWol) * thetaCosIn;
            validCount++;
        }

        // TODO: Re-use already evaluated data? half-way vector, distribution value?
        oBrdfSample.mPdfW = MicrofacetGGXConductorMaterial::GetPdfW(aWol, oBrdfSample.mWil);

#else
#error Undefined GGX sampling method!
#endif

        oBrdfSample.mCompProbability = 1.f;
    }

    virtual float GetPdfW(
        const Vec3f &aWol,
        const Vec3f &aWil
        ) const override
    {
        if (aWol.z < 0.f)
            return 0.0f;

#if defined MATERIAL_GGX_SAMPLING_COS

        if (aWil.z < 0.f)
            return 0.0f;

        aWol; // unreferenced params
        return CosHemispherePdfW(aWil); // debug: cosine-weighted sampling

#elif defined MATERIAL_GGX_SAMPLING_ALL_NORMALS

        if (aWil.z < 0.f)
            return 0.0f;

        return GgxMicrofacetSamplingPdf(aWol, aWil, mRoughnessAlpha);

#elif defined MATERIAL_GGX_SAMPLING_VISIBLE_NORMALS

        // Visible normals PDF
        {
            // TODO: Reuse computations when(if) combined with sampling routine??

            const Vec3f halfwayVec  = HalfwayVector(aWil, aWol);
            if (halfwayVec.z <= 0.f)
                return 0.0f;

            const float distrVal    = MicrofacetDistributionGgx(halfwayVec, mRoughnessAlpha);
            const float masking     = MicrofacetMaskingFunctionGgx(aWol, halfwayVec, mRoughnessAlpha);
            const float cosThetaOM  = Dot(halfwayVec, aWol);
            const float cosThetaO   = std::max(aWol.z, 0.00001f);

            const float microfacetPdf = (masking * std::abs(cosThetaOM) * distrVal) / cosThetaO;

            PG3_ASSERT_FLOAT_NONNEGATIVE(microfacetPdf);

            // Jacobian of the reflection transform
            const float cosThetaOMClamped = std::max(cosThetaOM, 0.00001f);
            const float transfJacobian = 1.0f / (4.0f * cosThetaOMClamped);

            PG3_ASSERT_FLOAT_NONNEGATIVE(transfJacobian);

            const float pdf = microfacetPdf * transfJacobian;

            PG3_ASSERT_FLOAT_NONNEGATIVE(pdf);

            return pdf;
        }


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

