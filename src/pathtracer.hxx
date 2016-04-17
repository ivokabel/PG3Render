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
            RayIntersection isect(1e36f);
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

                if (pathLength >= kMaxPathLength)
                    // We cut too long paths even when Russian roulette ending is active
                    // to avoid stack overflows
                    return;

                // Russian roulette (based on reflectance of the whole BSDF)
                float rrContinuationProb = 1.0f;
                if (mMaxPathLength == 0)
                {
                    // Const clamping
                    //rrContinuationProb =
                    //    // We do not allow probability 1.0 because that can lead to infinite 
                    //    // random walk in some pathological scenarios (completely white material or 
                    //    // light trap made of glass).
                    //    // Probability of survival after 100 bounces:
                    //    // local probability 98.0% -> 13% for the whole path
                    //    // local probability 99.0% -> 37% for the whole path
                    //    // local probability 99.3% -> 50% for the whole path
                    //    // local probability 99.5% -> 61% for the whole path
                    //    // local probability 99.7% -> 74% for the whole path
                    //    // local probability 99.9% -> 90% for the whole path - may cause stack overflows!
                    //    Math::Clamp(mat.GetRRContinuationProb(wol), 0.0f, 0.997f);

                    // No clamping
                   rrContinuationProb = Math::Clamp(mat.GetRRContinuationProb(wol), 0.f, 1.f);

                    const float rnd = mRng.GetFloat();
                    if (rnd > rrContinuationProb)
                        return;
                }

                // Sample BSDF
                MaterialRecord matRecord(wol);
                mat.SampleBsdf(mRng, matRecord);
                if (matRecord.IsBlocker())
                    // There is no contribution behind this reflection;
                    // we can cut the path without incorporation of bias
                    return;

                // Construct new ray from the current surface point
                currentRay.org  = surfPt;
                currentRay.dir  = surfFrame.ToWorld(matRecord.mWil);
                currentRay.tmin = Utils::Geom::EpsRayCos(matRecord.ThetaInCosAbs());

                if (matRecord.mPdfW != Math::InfinityF())
                    pathThroughput *=
                            (  matRecord.mAttenuation
                             * matRecord.ThetaInCosAbs())
                          / (  matRecord.mPdfW              // Monte Carlo est.
                             * rrContinuationProb           // Russian roulette (optional)
                             * matRecord.mCompProbability); // Discrete multi-component MC
                else
                    pathThroughput *=
                            matRecord.mAttenuation
                          / (  rrContinuationProb           // Russian roulette (optional)
                             * matRecord.mCompProbability); // Discrete multi-component MC

                PG3_ASSERT_VEC3F_NONNEGATIVE(pathThroughput);

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

        RayIntersection isect(1e36f);
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
                // Just emmited radiance (direct ligth computation)
                return;

            if (mat.IsReflectanceZero())
            {
                // Zero reflectivity - there is no chance of contribution behing this reflection;
                // we can safely cut the path without incorporation of bias
                mIntrospectionData.AddCorePathLength(aPathLength, kTerminatedByBlocker);
                return;
            }

            if (((mMaxPathLength > 0) && (aPathLength >= mMaxPathLength)))
            {
                // There's no point in continuing because all of the following computations 
                // need to extend the path and that's not allowed
                mIntrospectionData.AddCorePathLength(aPathLength, kTerminatedByMaxLimit);
                return;
            }
            
            if (aPathLength >= kMaxPathLength)
            {
                // We cut too long paths even when Russian roulette ending is active
                // to avoid stack overflows
                mIntrospectionData.AddCorePathLength(aPathLength, kTerminatedBySafetyLimit);
                return;
            }

            // Splitting
            uint32_t bsdfSamplesCount;
            uint32_t lightSamplesCount;
            float nextStepSplitBudget;
            // TODO: Use BSDF settings
            float splitLevel = mDbgSplittingLevel; // [0,1]: 0: no split, 1: full split
            ComputeSplittingCounts(
                aSplitBudget, splitLevel,
                bsdfSamplesCount, lightSamplesCount, nextStepSplitBudget);

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
                            lightSample, lightSamplesCount, bsdfSamplesCount,
                            surfPt, surfFrame, wol, mat, mRng,
                            oReflectedRadianceEstimate);
                    }
                }
            }

            float rrContinuationProb = 1.0f;
            if (mMaxPathLength == 0)
            {
                // Const clamping
                //rrContinuationProb =
                //    // We do not allow probability 1.0 because that can lead to infinite 
                //    // random walk in some pathological scenarios (completely white material or 
                //    // light trap made of glass).
                //    // Probability of survival after 100 bounces:
                //    // local probability 98.0% -> 13% for the whole path
                //    // local probability 99.0% -> 37% for the whole path
                //    // local probability 99.3% -> 50% for the whole path
                //    // local probability 99.5% -> 61% for the whole path
                //    // local probability 99.7% -> 74% for the whole path
                //    // local probability 99.9% -> 90% for the whole path - may cause stack overflows!
                //    Math::Clamp(mat.GetRRContinuationProb(wol), 0.0f, 0.997f);

                // No clamping
                rrContinuationProb = Math::Clamp(mat.GetRRContinuationProb(wol), 0.f, 1.f);
            }

            // Generate requested amount of BSDF samples for both direct and indirect illumination
            for (uint32_t sampleNum = 0; sampleNum < bsdfSamplesCount; sampleNum++)
            {
                bool bCutIndirect = false;

                // Russian roulette (based on reflectance of the whole BSDF)
                float rnd = -1.0f;
                if (mMaxPathLength == 0)
                {
                    rnd = mRng.GetFloat();
                    if (rnd > rrContinuationProb)
                    {
                        bCutIndirect = true;
                        mIntrospectionData.AddCorePathLength(aPathLength, kTerminatedByRussianRoulette);
                    }
                }

                MaterialRecord matRecord(wol);
                mat.SampleBsdf(mRng, matRecord);
                if (matRecord.IsBlocker())
                    continue;

                SpectrumF   bsdfEmmittedRadiance;
                SpectrumF   bsdfReflectedRadianceEstimate;
                float       bsdfLightPdfW = 0.f;
                int32_t     bsdfLightId = -1;

                const Vec3f wig = surfFrame.ToWorld(matRecord.mWil);
                const float rayMin = Utils::Geom::EpsRayCos(matRecord.ThetaInCosAbs());
                const Ray   bsdfRay(surfPt, wig, rayMin);

                EstimateIncomingRadiancePT(
                    bsdfRay, aPathLength + 1, !bCutIndirect, nextStepSplitBudget,
                    bsdfEmmittedRadiance, bsdfReflectedRadianceEstimate,
                    &surfFrame, &bsdfLightPdfW, &bsdfLightId);

                PG3_ASSERT(!bCutIndirect || bsdfReflectedRadianceEstimate.IsZero());

                // Direct light
                if ((!bsdfEmmittedRadiance.IsZero()) && (bsdfLightId >= 0))
                {
                    // Since Monte Carlo estimation works for finite (non-Dirac) BSDFs only,
                    // we split the integral into two parts - one for finite components and 
                    // one for Dirac components of the BSDF. This is analogical and well working 
                    // together with the separate light sampling scheme used in 
                    // AddMISLightSampleContribution() - see comments there for more information.
                    if (matRecord.mPdfW != Math::InfinityF())
                    {
                        // Finite BSDF: Compute MIS MC estimator. 

                        float bsdfLightPickingProb = 0.f;
                        LightPickingProbability(
                            surfPt, surfFrame, mat, bsdfLightId, lightSamplingCtx,
                            bsdfLightPickingProb);

                        // TODO: Uncomment this once proper environment map estimate is implemented.
                        //       Now there can be zero contribution estimate (and therefore zero picking probability)
                        //       even if the actual contribution is non-zero.
                        //PG3_ASSERT(lightPickingProbability > 0.f);
                        PG3_ASSERT(bsdfLightPdfW != Math::InfinityF()); // BSDF sampling should never hit a point light

                        const float lightPdfW = bsdfLightPdfW * bsdfLightPickingProb;
                        const float bsdfPdfW  = matRecord.mPdfW * matRecord.mCompProbability;
                        oReflectedRadianceEstimate +=
                              (   matRecord.mAttenuation
                                * matRecord.ThetaInCosAbs()
                                * bsdfEmmittedRadiance
                                * MISWeight2(bsdfPdfW, bsdfSamplesCount, lightPdfW, lightSamplesCount)) // MIS
                            / bsdfPdfW;
                    }
                    else
                    {
                        // Dirac BSDF: compute the integral directly, without MIS
                        oReflectedRadianceEstimate +=
                              (   matRecord.mAttenuation
                                * bsdfEmmittedRadiance)
                            / (   static_cast<float>(bsdfSamplesCount)  // Splitting
                                * matRecord.mCompProbability);          // Discrete multi-component MC
                    }

                    PG3_ASSERT_VEC3F_NONNEGATIVE(oReflectedRadianceEstimate);
                }

                // Indirect light
                if (!bCutIndirect && !bsdfReflectedRadianceEstimate.IsZero())
                {
                    SpectrumF indirectRadianceEstimate;
                    if (matRecord.mPdfW != Math::InfinityF())
                    {
                        // Finite BSDF: Compute simple MC estimator. 
                        indirectRadianceEstimate =
                              (   matRecord.mAttenuation
                                * matRecord.ThetaInCosAbs()
                                * bsdfReflectedRadianceEstimate)
                            / (   matRecord.mPdfW               // MC
                                * bsdfSamplesCount              // Splitting
                                * rrContinuationProb            // Russian roulette
                                * matRecord.mCompProbability);  // Discrete multi-component MC
                    }
                    else
                    {
                        // Dirac BSDF: compute the integral directly, without MIS
                        indirectRadianceEstimate =
                              (   matRecord.mAttenuation
                                * bsdfReflectedRadianceEstimate)
                            / (   bsdfSamplesCount              // Splitting
                                * rrContinuationProb            // Russian roulette
                                * matRecord.mCompProbability);  // Discrete multi-component MC
                    }

                    PG3_ASSERT_VEC3F_NONNEGATIVE(indirectRadianceEstimate);

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

            if (aComputeReflectedRadiance)
                // If we were asked for reflected radiance, we need to save path length here,
                // because the caller cannot identify this case without incorporating additional 
                // communication with the callee
                mIntrospectionData.AddCorePathLength(aPathLength - 1u, kTerminatedByBackground);
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
                Math::Lerp(aSplitLevel, 1.f, aSplitBudget));

        // TODO: Light samples count should be controlled by both 
        //  - material glossines (the more glossy it is, the less efficient the light sampling is; for mirros it doesn't work at all)
        //  - user-provided parameter (one may want to use more light samples for complicated geometry)
        oLightSamplesCount =
            std::lroundf(
                std::max(mDbgSplittingLightToBrdfSmplRatio * oBrdfSamplesCount, 1.f));

        oNextStepSplitBudget =
            1 + ((aSplitBudget - oBrdfSamplesCount) / (float)oBrdfSamplesCount);
    }

protected:

// Empirical values for cutting too long paths
#ifdef _DEBUG
    const uint32_t kMaxPathLength = 700;     // crash at 700
#else
    const uint32_t kMaxPathLength = 1500;    // 1700 crashes for sure
#endif

};
