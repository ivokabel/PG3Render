#pragma once

#include "spectrum.hxx"
#include "renderer.hxx"
#include "rng.hxx"
#include "types.hxx"
#include "hardconfig.hxx"

#include <vector>
#include <cmath>
#include <omp.h>
#include <cassert>

class PathTracerBase : public AbstractRenderer
{
protected:
    class LightSamplingContext
    {
    public:
        LightSamplingContext(size_t aLightCount) :
            mLightContribEstsCache(aLightCount, 0.f),
            mValid(false)
        {};

        std::vector<float>      mLightContribEstsCache;
        bool                    mValid;
    };

public:

    PathTracerBase(
        const Config    &aConfig,
        int32_t          aSeed = 1234
    ) :
        AbstractRenderer(aConfig), mRng(aSeed),
        mMinPathLength(aConfig.mMinPathLength),
        mMaxPathLength(aConfig.mMaxPathLength),
        mIndirectIllumClipping(aConfig.mIndirectIllumClipping),
        mSplittingBudget(aConfig.mSplittingBudget),

        // debug, temporary
        mDbgSplittingLevel(aConfig.mDbgSplittingLevel),
        mDbgSplittingLightToBrdfSmplRatio(aConfig.mDbgSplittingLightToBrdfSmplRatio)
    {}

    virtual void EstimateIncomingRadiance(
        const Algorithm      aAlgorithm,
        const Ray           &aRay,
              SpectrumF     &oRadiance
        ) = 0;

    virtual void RunIteration(
        const Algorithm     aAlgorithm,
        uint32_t            aIteration) override
    {
        aIteration; // unused parameter

        const uint32_t resX = uint32_t(mConfig.mScene->mCamera.mResolution.x);
        const uint32_t resY = uint32_t(mConfig.mScene->mCamera.mResolution.y);

        for (uint32_t pixID = 0; pixID < resX * resY; pixID++)
        {
            //////////////////////////////////////////////////////////////////////////
            // Generate ray

            const uint32_t x = pixID % resX;
            const uint32_t y = pixID / resX;

            const Vec2f randomOffset = mRng.GetVec2f();
            const Vec2f basedCoords = Vec2f(float(x), float(y));
            const Vec2f basedCoordsBound = Vec2f(float(x), float(y));
            const Vec2f sample = basedCoords + randomOffset;

            Ray ray = mConfig.mScene->mCamera.GenerateRay(sample);

            //////////////////////////////////////////////////////////////////////////
            // Estimate radiance

            SpectrumF radianceEstimate;
            EstimateIncomingRadiance(aAlgorithm, ray, radianceEstimate);
            mFramebuffer.AddRadiance(x, y, radianceEstimate);
        }

        mIterations++;
    };

    void GetDirectRadianceFromDirection(
        const Vec3f                 &aSurfPt,
        const Frame                 &aSurfFrame,
        const AbstractMaterial      &aSurfMaterial,
        const Vec3f                 &aWil,
              LightSamplingContext  &aContext,
              SpectrumF             &oLight,
              float                 *oPdfW = NULL,
              float                 *oLightProbability = NULL
        )
    {
        int32_t lightId = -1;

        const Vec3f wig = aSurfFrame.ToWorld(aWil);
        const float rayMin = Geom::EpsRayCos(aWil.z);
        const Ray bsdfRay(aSurfPt, wig, rayMin);
        RayIntersection bsdfIsect(1e36f);
        if (mConfig.mScene->Intersect(bsdfRay, bsdfIsect))
        {
            if (bsdfIsect.lightID >= 0)
            {
                // We hit light source geometry, get outgoing radiance
                const Vec3f lightPt = bsdfRay.org + bsdfRay.dir * bsdfIsect.dist;
                const AbstractLight *light = mConfig.mScene->GetLightPtr(bsdfIsect.lightID);
                Frame frame;
                frame.SetFromZ(bsdfIsect.normal);
                const Vec3f ligthWol = frame.ToLocal(-bsdfRay.dir);
                oLight = light->GetEmmision(lightPt, ligthWol, aSurfPt, oPdfW, &aSurfFrame);

                lightId = bsdfIsect.lightID;
            }
            else
            {
                // We hit a geometry which is not a light source, 
                // no direct light contribution for this sample
                oLight.MakeZero();
            }
        }
        else
        {
            // No geometry intersection, get radiance from background
            const BackgroundLight *backgroundLight = mConfig.mScene->GetBackground();
            if (backgroundLight != NULL)
            {
                oLight = backgroundLight->GetEmmision(wig, false, oPdfW, &aSurfFrame);
                lightId = mConfig.mScene->GetBackgroundLightId();
            }
            else
                oLight.MakeZero(); // No background light
        }

        PG3_ASSERT(oLight.IsZero() || (lightId >= 0));

        if ((oLightProbability != NULL) && (lightId >= 0))
        {
            LightPickingProbability(
                aSurfPt, aSurfFrame, aSurfMaterial, lightId, aContext, *oLightProbability);
            // TODO: Uncomment this once proper environment map estimate is implemented.
            //       Now there can be zero contribution estimate (and therefore zero picking probability)
            //       even if the actual contribution is non-zero.
            //PG3_ASSERT(oLight.IsZero() || (*oLightProbability > 0.f));
        }
    }

    bool SampleLightsSingle(
        const Vec3f                 &aSurfPt, 
        const Frame                 &aSurfFrame, 
        const AbstractMaterial      &aSurfMaterial,
              LightSamplingContext  &aContext,
              LightSample           &oLightSample
        )
    {
        // We split the planar integral over the surface of all light sources 
        // into sub-integrals - one integral for each light source - and estimate 
        // the sum of all the sub-results using a (discrete, second-level) MC estimator.

        int32_t chosenLightId   = -1;
        float lightProbability  = 0.f;
        PickSingleLight(
            aSurfPt, aSurfFrame, aSurfMaterial, aContext, chosenLightId, lightProbability);

        // Sample the chosen light
        if (chosenLightId >= 0)
        {
            // Choose a random sample on the light
            const AbstractLight* light = mConfig.mScene->GetLightPtr(chosenLightId);
            PG3_ASSERT(light != 0);
            light->SampleIllumination(aSurfPt, aSurfFrame, aSurfMaterial, mRng, oLightSample);
            oLightSample.mLightProbability = lightProbability;

            return true;
        }
        else
            return false;
    }

    // Pick one of the light sources randomly (proportionally to their estimated contribution)
    void PickSingleLight(
        const Vec3f                 &aSurfPt,
        const Frame                 &aSurfFrame, 
        const AbstractMaterial      &aSurfMaterial,
              LightSamplingContext  &aContext,
              int32_t               &oChosenLightId, 
              float                 &oLightProbability)
    {
        const size_t lightCount = mConfig.mScene->GetLightCount();
        if (lightCount == 0)
            oChosenLightId = -1;
        else if (lightCount == 1)
        {
            // If there's just one light, we skip the unnecessary (and fairly expensive) picking process
            oChosenLightId = 0;
            oLightProbability = 1.f;
        }
        else
        {
            // Non-normalized CDF for all light sources
            // TODO: Make it a PT's member to avoid unnecessary allocations?
            std::vector<float> lightContrPseudoCdf(lightCount + 1);

            PG3_ASSERT(aContext.mLightContribEstsCache.size() == lightCount);

            // Estimate the contribution of all available light sources
            float estimatesSum = 0.f;
            lightContrPseudoCdf[0] = 0.f;
            for (uint32_t i = 0; i < lightCount; i++)
            {
                const AbstractLight* light = mConfig.mScene->GetLightPtr(i);
                PG3_ASSERT(light != 0);

                if (!aContext.mValid)
                    // Fill light contribution cache
                    aContext.mLightContribEstsCache[i] =
                        light->EstimateContribution(aSurfPt, aSurfFrame, aSurfMaterial, mRng);

                estimatesSum += aContext.mLightContribEstsCache[i];
                lightContrPseudoCdf[i + 1] = estimatesSum;
            }
            aContext.mValid = true;

            if (estimatesSum > 0.f)
            {
                // Pick a light proportionally to estimates
                const float rndVal = mRng.GetFloat() * estimatesSum;
                uint32_t lightId = 0;
                // TODO: Use std::find to find the chosen light?
                for (; (rndVal > lightContrPseudoCdf[lightId + 1]) && (lightId < lightCount); lightId++);
                oChosenLightId    = (int32_t)lightId;
                oLightProbability =
                    (lightContrPseudoCdf[lightId + 1] - lightContrPseudoCdf[lightId]) / estimatesSum;

                PG3_ASSERT_INTEGER_IN_RANGE(oChosenLightId, 0, (int32_t)(lightCount - 1));
            }
            else
            {
                // Pick a light uniformly
                const float rndVal = mRng.GetFloat();
                oChosenLightId    = std::min((int32_t)(rndVal * lightCount), (int32_t)(lightCount - 1));
                oLightProbability = 1.f / lightCount;

                PG3_ASSERT_INTEGER_IN_RANGE(oChosenLightId, 0, (int32_t)(lightCount - 1));
            }
        }
    }

    // Computes probability of picking the specified light source
    void LightPickingProbability(
        const Vec3f                 &aSurfPt,
        const Frame                 &aSurfFrame, 
        const AbstractMaterial      &aSurfMaterial,
              uint32_t               aLightId, 
              LightSamplingContext  &aContext,
              float                 &oLightProbability)
    {
        const size_t lightCount = mConfig.mScene->GetLightCount();

        PG3_ASSERT((aLightId >= 0) && (aLightId < lightCount));

        if (lightCount == 0)
            oLightProbability = 0.f;
        else if (lightCount == 1)
        {
            // If there's just one light, we skip the unnecessary (and fairly expensive) 
            // process of computing estimated contributions
            oLightProbability = 1.f;
        }
        else
        {
            PG3_ASSERT(aContext.mLightContribEstsCache.size() == lightCount);

            // Estimate the contribution of all available light sources
            float estimatesSum  = 0.f;
            float lightEstimate = 0.f;
            for (uint32_t i = 0; i < lightCount; i++)
            {
                const AbstractLight* light = mConfig.mScene->GetLightPtr(i);
                PG3_ASSERT(light != 0);

                if (!aContext.mValid)
                    // Fill light contribution cache
                    aContext.mLightContribEstsCache[i] =
                        light->EstimateContribution(aSurfPt, aSurfFrame, aSurfMaterial, mRng);

                const float estimate = aContext.mLightContribEstsCache[i];
                if (i == aLightId)
                    lightEstimate = estimate;
                estimatesSum += estimate;
            }
            aContext.mValid = true;

            if (estimatesSum > 0.f)
            {
                // Proportional probability
                oLightProbability = lightEstimate / estimatesSum;
            }
            else
                // Uniform probability
                oLightProbability = 1.f / lightCount;
        }
    }

    void AddSingleLightSampleContribution(
        const LightSample       &aLightSample,
        const Vec3f             &aSurfPt,
        const Frame             &aSurfFrame, 
        const AbstractMaterial  &aSurfMaterial,
        const Vec3f             &aWol,
              SpectrumF         &oLightBuffer)
    {
        if (aLightSample.mSample.Max() <= 0.)
            // The light emmits zero radiance in this direction
            return;
        if (mConfig.mScene->Occluded(aSurfPt, aLightSample.mWig, aLightSample.mDist))
            // The light is not visible from this point
            return;

        if (aLightSample.mPdfW != Math::InfinityF())
            // Planar or angular light sources - compute two-step MC estimator.
            oLightBuffer +=
                  aLightSample.mSample
                * aSurfMaterial.EvalBsdf(aSurfFrame.ToLocal(aLightSample.mWig), aWol)
                / (aLightSample.mPdfW * aLightSample.mLightProbability);
        else
            // Point light - the contribution of a single light is computed 
            // analytically (without MC estimation), there is only one MC 
            // estimation left - the estimation of the sum of contributions 
            // of all light sources.
            oLightBuffer +=
                  aLightSample.mSample
                * aSurfMaterial.EvalBsdf(aSurfFrame.ToLocal(aLightSample.mWig), aWol)
                / aLightSample.mLightProbability;
    }

    void AddMISLightSampleContribution(
        const LightSample       &aLightSample,
        const uint32_t           aLightSamplesCount,
        const uint32_t           aBrdfSamplesCount,
        const Vec3f             &aSurfPt,
        const Frame             &aSurfFrame, 
        const Vec3f             &aWol,
        const AbstractMaterial  &aSurfMaterial,
              Rng               &aRng,
              SpectrumF         &oLightBuffer)
    {
        if (aLightSample.mSample.Max() <= 0.)
            // The light emmits zero radiance in this direction
            return;
        if (mConfig.mScene->Occluded(aSurfPt, aLightSample.mWig, aLightSample.mDist))
            // The light is not visible from this point
            return;

        const Vec3f wil = aSurfFrame.ToLocal(aLightSample.mWig);

        // Since Monte Carlo estimation works only for planar and angular light sources, we can't 
        // use the multiple importance sampling scheme for the whole reflectance integral. We split 
        // the integral into two parts - one for planar and angular light sources, and one 
        // for point light sources. The first part can be handled by both BSDF and light 
        // sampling strategies; therefore, we can combine the two with MIS. The second part 
        // (point lights) can only be hadled by the light sampling strategy.
        //
        // We could separate the two computations completely, what would result in computing 
        // separate sets of light samples for each of the two integrals, but we can use 
        // one light sampling routine to generate samples for both integrals in one place 
        // to make things easier for us (but maybe a little bit confusing at first sight).
        //
        // If we chose an area or angular light, we compute the MIS-ed part and for point 
        // light sampling it means that we generated an empty sample. Vice versa, if we chose 
        // a point light, we compute non-zero estimator for the point lights integral and 
        // for the MIS-ed integral this means we generated an empty sample. PDFs from 
        // the light sampling routine can be used directly without any adaptation 
        // in both computations.
        //
        // TODO: If we look at each of the two integral parts separtately, it is obvious, 
        //       that chosing a light source using a strategy which sometimes choses "no light"
        //       causes worse estimator performance than a technique which always choses a light.
        //       However, estimating the first and the second part separately will require 
        //       filtering a proper set of lights when choosing a light and also when computing 
        //       the probability of picking one.

        if (aLightSample.mPdfW != Math::InfinityF())
        {
            // Planar or angular light source was chosen: Proceed with MIS MC estimator
            MaterialRecord matRecord(wil, aWol);
            aSurfMaterial.EvalBsdf(aRng, matRecord);
            const float bsdfTotalFinitePdfW = matRecord.mPdfW * matRecord.mCompProbability;
            const float lightPdfW = aLightSample.mPdfW * aLightSample.mLightProbability;

            oLightBuffer +=
                    (   aLightSample.mSample
                      * matRecord.mAttenuation
                      * MISWeight2(lightPdfW, aLightSamplesCount, bsdfTotalFinitePdfW, aBrdfSamplesCount))
                    / lightPdfW;
        }
        else
        {
            // Point light was chosen: The contribution of a single light is computed analytically,
            // there is only one MC estimation left - the estimation of the sum of contributions
            // of all light sources.
            oLightBuffer +=
                  (aLightSample.mSample * aSurfMaterial.EvalBsdf(wil, aWol))
                / (aLightSample.mLightProbability * aLightSamplesCount);
        }
    }

    void AddDirectIllumMISBrdfSampleContribution(
        const MaterialRecord        &aMatRecord,
        const uint32_t               aLightSamplesCount,
        const uint32_t               aBrdfSamplesCount,
        const Vec3f                 &aSurfPt,
        const Frame                 &aSurfFrame,
        const AbstractMaterial      &aSurfMaterial,
              LightSamplingContext  &aContext,
              SpectrumF             &oLightBuffer)
    {
        if (aMatRecord.IsBlocker())
            // The material is a complete blocker in this direction
            return;

        SpectrumF LiLight;
        float lightPdfW = 0.f;
        float lightPickingProbability = 0.f;
        GetDirectRadianceFromDirection(
            aSurfPt,
            aSurfFrame,
            aSurfMaterial,
            aMatRecord.mWil,
            aContext,
            LiLight,
            &lightPdfW,
            &lightPickingProbability);

        if (LiLight.Max() <= 0.f)
            // Zero direct radiance is coming from this direction
            return;

        // TODO: Uncomment this once proper environment map estimate is implemented.
        //       Now there can be zero contribution estimate (and therefore zero picking probability)
        //       even if the actual contribution is non-zero.
        //PG3_ASSERT(lightPickingProbability > 0.f);
        PG3_ASSERT(lightPdfW != Math::InfinityF()); // BSDF sampling should never hit a point light

        if (aMatRecord.mPdfW != Math::InfinityF())
        {
            // Finite BSDF: Compute two-step MIS MC estimator. 
            const float bsdfPdfW = aMatRecord.mPdfW * aMatRecord.mCompProbability;
            const float misWeight =
                MISWeight2(
                    bsdfPdfW, aBrdfSamplesCount,
                    lightPdfW * lightPickingProbability, aLightSamplesCount);
            oLightBuffer +=
                  (   aMatRecord.mAttenuation
                    * aMatRecord.ThetaInCosAbs()
                    * LiLight
                    * misWeight)
                / bsdfPdfW;
        }
        else
        {
            // Dirac BSDF: compute the integral directly, without MIS
            oLightBuffer +=
                  (   aMatRecord.mAttenuation
                    * LiLight)
                / (   static_cast<float>(aBrdfSamplesCount)     // Splitting
                    * aMatRecord.mCompProbability);             // Discrete multi-component MC
        }

        PG3_ASSERT_VEC3F_NONNEGATIVE(oLightBuffer);
    }

    float MISWeight2(
        const float         aStrategy1Pdf,
        const uint32_t      aStrategy1Count,
        const float         aStrategy2Pdf,
        const uint32_t      aStrategy2Count
        )
    {
#if defined PG3_USE_BALANCE_MIS_HEURISTIC
        return MISWeight2Balanced(aStrategy1Pdf, aStrategy1Count, aStrategy2Pdf, aStrategy2Count);
#elif defined PG3_USE_POWER_MIS_HEURISTIC
        return MISWeight2Power(aStrategy1Pdf, aStrategy1Count, aStrategy2Pdf, aStrategy2Count);
#else
#error Undefined MIS heuristic mode!
#endif
    }

    float MISWeight2Balanced(
        const float         aStrategy1Pdf,
        const uint32_t      aStrategy1Count,
        const float         aStrategy2Pdf,
        const uint32_t      aStrategy2Count
        )
    {
        PG3_ASSERT_INTEGER_POSITIVE(aStrategy1Count);

        return
              aStrategy1Pdf
            / (aStrategy1Count * aStrategy1Pdf + aStrategy2Count * aStrategy2Pdf);
    }

    float MISWeight2Power(
        const float         aStrategy1Pdf,
        const uint32_t      aStrategy1Count,
        const float         aStrategy2Pdf,
        const uint32_t      aStrategy2Count
        )
    {
        PG3_ASSERT_INTEGER_POSITIVE(aStrategy1Count);

        const float aStrategy1Sum = aStrategy1Count * aStrategy1Pdf;
        const float aStrategy2Sum = aStrategy2Count * aStrategy2Pdf;

        const float aStrategy1Sqr = aStrategy1Sum * aStrategy1Sum;
        const float aStrategy2Sqr = aStrategy2Sum * aStrategy2Sum;

        return
              aStrategy1Sqr
            / (aStrategy1Sqr + aStrategy2Sqr)
            / aStrategy1Count;
    }

protected:
    Rng                     mRng;

    uint32_t                mMinPathLength;
    uint32_t                mMaxPathLength;
    float                   mIndirectIllumClipping;
    uint32_t                mSplittingBudget;

    // debug, temporary
    float mDbgSplittingLevel;
    float mDbgSplittingLightToBrdfSmplRatio;
};
