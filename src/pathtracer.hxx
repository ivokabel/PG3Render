#pragma once

#include "pathtracerbase.hxx"

class PathTracer : public PathTracerBase
{
public:

    PathTracer(
        const Config    &aConfig,
        int32_t          aSeed = 1234
    ) :
        PathTracerBase(aConfig, aSeed)
    {}

    virtual void EstimateIncomingRadiance(
        const Algorithm      aAlgorithm,
        const Ray           &aRay,
              SpectrumF     &oRadiance
        )
    {
        if (aAlgorithm == kPathTracingNaive)
        {
            // Simple path tracer
            EstimateIncomingRadiancePTNaive(aRay, oRadiance);
        }
        else if (aAlgorithm == kPathTracing)
        {
            // Path tracer with next event estimate and MIS for direct illumination
            SpectrumF emmittedRadiance;
            SpectrumF reflectedRadianceEstimate;
            EstimateIncomingRadiancePT(
                aRay, 1, true, (float)mSplittingBudget,
                emmittedRadiance, reflectedRadianceEstimate);
            oRadiance = emmittedRadiance + reflectedRadianceEstimate;
        }
        else
        {
            PG3_FATAL_ERROR("Unknown algorithm!");
        }
    }

protected:

    void EstimateIncomingRadiancePTNaive(
        const Ray           &aRay,
              SpectrumF     &oRadiance
        )
    {
        oRadiance.MakeZero();

        SpectrumF pathThroughput;
        pathThroughput.SetGreyAttenuation(1.0f);
        Ray currentRay = aRay;

        for (uint32_t pathLength = 1;;)
        {
            Isect isect(1e36f);
            if (mConfig.mScene->Intersect(currentRay, isect))
            {
                // We hit some geometry

                const Vec3f surfPt = currentRay.org + currentRay.dir * isect.dist;
                Frame surfFrame;
                surfFrame.SetFromZ(isect.normal);
                const Vec3f wol = surfFrame.ToLocal(-currentRay.dir);
                const AbstractMaterial& mat = mConfig.mScene->GetMaterial(isect.matID);

                if ((isect.lightID >= 0) && (pathLength >= mMinPathLength))
                {
                    // Light source - get emmision
                    const AbstractLight *light = mConfig.mScene->GetLightPtr(isect.lightID);
                    if (light != NULL)
                        oRadiance +=
                              light->GetEmmision(surfPt, wol, Vec3f())
                            * pathThroughput;
                }

                if (mat.IsReflectanceZero())
                    // Zero reflectivity - there is no chance of contribution behing this reflection;
                    // we can safely cut the path without incorporation of bias
                    return;

                if ((mMaxPathLength > 0) && (pathLength >= mMaxPathLength))
                    return;

                // Russian roulette (based on reflectance of the whole BRDF)
                float rrContinuationProb = 1.0f;
                if (mMaxPathLength == 0)
                {
                    rrContinuationProb =
                        // We do not allow probability 1.0 because that can lead to infinite 
                        // random walk in some pathological scenarios (completely white material).
                        // After 100 bounces with probability of 98%, the whole path has 
                        // probability of cca 13% of survival.
                        Clamp(mat.GetRRContinuationProb(wol), 0.0f, 0.98f);
                    const float rnd = mRng.GetFloat();
                    if (rnd > rrContinuationProb)
                        return;
                }

                // Sample BRDF
                BRDFSample brdfSample;
                mat.SampleBrdf(mRng, wol, brdfSample);
                if (brdfSample.mSample.Max() <= 0.)
                    // There is no contribution behing this reflection;
                    // we can cut the path without incorporation of bias
                    return;

                // Construct new ray from the current surface point
                currentRay.org  = surfPt;
                currentRay.dir  = surfFrame.ToWorld(brdfSample.mWil);
                currentRay.tmin = EPS_RAY_COS(brdfSample.mWil.z);

                if (brdfSample.mPdfW != INFINITY_F)
                    pathThroughput *=
                            brdfSample.mSample
                          / (  brdfSample.mPdfW             // Monte Carlo est.
                             * rrContinuationProb           // Russian roulette (optional)
                             * brdfSample.mCompProbability);// Discrete multi-component MC
                else
                    pathThroughput *=
                            brdfSample.mSample
                          / (  rrContinuationProb           // Russian roulette (optional)
                             * brdfSample.mCompProbability);// Discrete multi-component MC

                pathLength++;
            }
            else
            {
                // No intersection - get radiance from the background
                if (pathLength >= mMinPathLength)
                {
                    const BackgroundLight *backgroundLight = mConfig.mScene->GetBackground();
                    if (backgroundLight != NULL)
                        oRadiance +=
                              backgroundLight->GetEmmision(currentRay.dir, true)
                            * pathThroughput;
                }

                // End of path
                return;
            }
        }
    }

    void EstimateIncomingRadiancePT(
        const Ray       &aRay,
        const uint32_t   aPathLength,
        const bool       aComputeReflectedRadiance,
        const float      aSplitBudget,
        SpectrumF       &oEmmittedRadiance,
        SpectrumF       &oReflectedRadianceEstimate,
        const Frame     *aSurfFrame = NULL, // Only needed when you to compute PDF of a const env. light source sample
        float           *oEmmittedLightPdfW = NULL,
        int32_t         *oLightID = NULL
        )
    {
        PG3_ASSERT((mMaxPathLength == 0) || (aPathLength <= mMaxPathLength));

        oEmmittedRadiance.MakeZero();
        oReflectedRadianceEstimate.MakeZero();

        LightSamplingContext lightSamplingCtx(mConfig.mScene->GetLightCount());

        Isect isect(1e36f);
        if (mConfig.mScene->Intersect(aRay, isect))
        {
            // We hit some geometry

            const Vec3f surfPt = aRay.PointAt(isect.dist);
            Frame surfFrame;
            surfFrame.SetFromZ(isect.normal);
            const Vec3f wol = surfFrame.ToLocal(-aRay.dir);
            const AbstractMaterial& mat = mConfig.mScene->GetMaterial(isect.matID);

            // If light source was hit, get its emmision
            if ((isect.lightID >= 0) && (aPathLength >= mMinPathLength))
            {
                const AbstractLight *light = mConfig.mScene->GetLightPtr(isect.lightID);
                if (light != NULL)
                {
                    oEmmittedRadiance +=
                        light->GetEmmision(surfPt, wol, aRay.org, oEmmittedLightPdfW, aSurfFrame);
                    if (oLightID != NULL)
                        *oLightID = isect.lightID;
                }
            }

            if (!aComputeReflectedRadiance)
                return;
            if (mat.IsReflectanceZero())
                // Zero reflectivity - there is no chance of contribution behing this reflection;
                // we can safely cut the path without incorporation of bias
                return;
            if ((mMaxPathLength > 0) && (aPathLength >= mMaxPathLength))
                return;

            // Splitting
            uint32_t brdfSamplesCount;
            uint32_t lightSamplesCount;
            float nextStepSplitBudget;
            // TODO: Use BRDF settings
            float splitLevel = mDbgSplittingLevel; // [0,1]: 0: no split, 1: full split
            ComputeSplittingCounts(
                aSplitBudget, splitLevel,
                brdfSamplesCount, lightSamplesCount, nextStepSplitBudget);

            // Generate requested amount of light samples for direct illumination
            // ...if one more path step is allowed
            if ((aPathLength + 1) >= mMinPathLength)
            {
                for (uint32_t sampleNum = 0; sampleNum < lightSamplesCount; sampleNum++)
                {
                    LightSample lightSample;
                    if (SampleLightsSingle(surfPt, surfFrame, mat, lightSamplingCtx, lightSample))
                    {
                        AddMISLightSampleContribution(
                            lightSample, lightSamplesCount, brdfSamplesCount,
                            surfPt, surfFrame, wol, mat,
                            oReflectedRadianceEstimate);
                    }
                }
            }

            float rrContinuationProb = 1.0f;
            if (mMaxPathLength == 0)
                rrContinuationProb =
                    // We do not allow probability 1.0 because that can lead to infinite 
                    // random walk in some pathological scenarios (completely white material or 
                    // light trap made of glass).
                    // Probability of survival after 100 bounces:
                    // local probability 98.0% -> 13% for the whole path
                    // local probability 99.0% -> 37% for the whole path
                    // local probability 99.5% -> 61% for the whole path
                    // local probability 99.7% -> 74% for the whole path
                    // local probability 99.9% -> 90% for the whole path - may cause stack overflows!
                    Clamp(mat.GetRRContinuationProb(wol), 0.0f, 0.997f);

            // Generate requested amount of BRDF samples for both direct and indirect illumination
            for (uint32_t sampleNum = 0; sampleNum < brdfSamplesCount; sampleNum++)
            {
                bool bCutIndirect = false;

                // Russian roulette (based on reflectance of the whole BRDF)
                if (mMaxPathLength == 0)
                {
                    const float rnd = mRng.GetFloat();
                    if (rnd > rrContinuationProb)
                        bCutIndirect = true;
                }

                BRDFSample brdfSample;
                mat.SampleBrdf(mRng, wol, brdfSample);
                if (brdfSample.mSample.Max() <= 0.f)
                    continue;

                SpectrumF   brdfEmmittedRadiance;
                SpectrumF   brdfReflectedRadianceEstimate;
                float       brdfLightPdfW = 0.f;
                int32_t     brdfLightId = -1;

                const Vec3f wig = surfFrame.ToWorld(brdfSample.mWil);
                const float rayMin = EPS_RAY_COS(brdfSample.mWil.z);
                const Ray   brdfRay(surfPt, wig, rayMin);

                EstimateIncomingRadiancePT(
                    brdfRay, aPathLength + 1, !bCutIndirect, nextStepSplitBudget,
                    brdfEmmittedRadiance, brdfReflectedRadianceEstimate,
                    &surfFrame, &brdfLightPdfW, &brdfLightId);

                PG3_ASSERT(!bCutIndirect || brdfReflectedRadianceEstimate.IsZero());

                // Direct light
                if ((!brdfEmmittedRadiance.IsZero()) && (brdfLightId >= 0))
                {
                    // Since Monte Carlo estimation works only for finite (non-Dirac) BSDFs,
                    // we split the integral into two parts - one for finite components and 
                    // one for Dirac components of the BSDF. This is analogical and well working 
                    // together with the separate light sampling scheme used in 
                    // AddMISLightSampleContribution() - see comments there for more information.
                    if (brdfSample.mPdfW != INFINITY_F)
                    {
                        // Finite BRDF: Compute MIS MC estimator. 

                        float brdfLightPickingProb = 0.f;
                        LightPickingProbability(
                            surfPt, surfFrame, mat, brdfLightId, lightSamplingCtx,
                            brdfLightPickingProb);

                        // TODO: Uncomment this once proper environment map estimate is implemented.
                        //       Now there can be zero contribution estimate (and therefore zero picking probability)
                        //       even if the actual contribution is non-zero.
                        //PG3_ASSERT(lightPickingProbability > 0.f);
                        PG3_ASSERT(brdfLightPdfW != INFINITY_F); // BRDF sampling should never hit a point light

                        const float lightPdfW = brdfLightPdfW * brdfLightPickingProb;
                        const float brdfPdfW  = brdfSample.mPdfW * brdfSample.mCompProbability;
                        oReflectedRadianceEstimate +=
                              (brdfSample.mSample * brdfEmmittedRadiance)
                            * MISWeight2(brdfPdfW, brdfSamplesCount, lightPdfW, lightSamplesCount) // MIS
                            / brdfPdfW;
                    }
                    else
                    {
                        // Dirac BRDF: compute the integral directly, without MIS
                        oReflectedRadianceEstimate +=
                              (brdfSample.mSample * brdfEmmittedRadiance)
                            / (   static_cast<float>(brdfSamplesCount)  // Splitting
                                * brdfSample.mCompProbability);         // Discrete multi-component MC
                    }
                }

                // Indirect light
                if (!bCutIndirect && !brdfReflectedRadianceEstimate.IsZero())
                {
                    SpectrumF indirectRadianceEstimate;
                    if (brdfSample.mPdfW != INFINITY_F)
                    {
                        // Finite BRDF: Compute simple MC estimator. 
                        indirectRadianceEstimate =
                              (brdfSample.mSample * brdfReflectedRadianceEstimate)
                            / (   brdfSample.mPdfW              // MC
                                * brdfSamplesCount              // Splitting
                                * rrContinuationProb            // Russian roulette
                                * brdfSample.mCompProbability); // Discrete multi-component MC
                    }
                    else
                    {
                        // Dirac BRDF: compute the integral directly, without MIS
                        indirectRadianceEstimate =
                              (brdfSample.mSample * brdfReflectedRadianceEstimate)
                            / (   brdfSamplesCount              // Splitting
                                * rrContinuationProb            // Russian roulette
                                * brdfSample.mCompProbability); // Discrete multi-component MC
                    }

                    // Clip fireflies
                    if (mIndirectIllumClipping > 0.f)
                        indirectRadianceEstimate.ClipProportionally(mIndirectIllumClipping);

                    oReflectedRadianceEstimate += indirectRadianceEstimate;
                }
            }
        }
        else
        {
            // No intersection - get radiance from the background
            if (aPathLength >= mMinPathLength)
            {
                const BackgroundLight *backgroundLight = mConfig.mScene->GetBackground();
                if (backgroundLight != NULL)
                {
                    oEmmittedRadiance +=
                        backgroundLight->GetEmmision(aRay.dir, true, oEmmittedLightPdfW, aSurfFrame);
                    if (oLightID != NULL)
                        *oLightID = mConfig.mScene->GetBackgroundLightId();;
                }
            }
        }
    }

    void ComputeSplittingCounts(
        float        aSplitBudget,
        float        aSplitLevel, // [0,1]: 0: no split, 1: full split
        uint32_t    &oBrdfSamplesCount,
        uint32_t    &oLightSamplesCount,
        float       &oNextStepSplitBudget
        )
    {
        // TODO: Control by material glossines
        oBrdfSamplesCount =
            std::lroundf(
                LinInterpol(aSplitLevel, 1.f, aSplitBudget));

        // TODO: Light samples count should be controlled by both 
        //  - material glossines (the more glossy it is, the less efficient the light sampling is; for mirros it doesn't work at all)
        //  - user-provided parameter (one may want to use more light samples for complicated geometry)
        oLightSamplesCount =
            std::lroundf(
                std::max(mDbgSplittingLightToBrdfSmplRatio * oBrdfSamplesCount, 1.f));

        oNextStepSplitBudget =
            1 + ((aSplitBudget - oBrdfSamplesCount) / (float)oBrdfSamplesCount);
    }
};
