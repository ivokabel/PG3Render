#pragma once

#include "pathtracerbase.hxx"

class PathTracer : public PathTracerBase
{
protected:

    //class PTNeeMisContext
    //{
    //public:
    //    PTNeeMisContext(const Ray &aRay, uint32_t aPathLength) :
    //        mRay(aRay), mPathLength(aPathLength) {};

    //    // Input parameters
    //    const Ray       &mRay;
    //    const uint32_t   mPathLength;

    //    // Output
    //    SpectrumF        mLe;
    //    SpectrumF        mLr;

    //    // This class is not copyable because of a const member.
    //    // If we don't delete the assignment operator and copy constructor 
    //    // explicitly, the compiler may complain about not being able 
    //    // to create their default implementations.
    //    PTNeeMisContext & operator=(const PTNeeMisContext&) = delete;
    //    PTNeeMisContext(const PTNeeMisContext&) = delete;
    //};

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
        else if (aAlgorithm == kPathTracingNEEMIS)
        {
            // Path tracer with next event estimate and MIS for direct illumination
            SpectrumF emmittedRadiance;
            SpectrumF reflectedRadianceEstimate;
            EstimateIncomingRadiancePTNeeMis(aRay, 1, emmittedRadiance, reflectedRadianceEstimate);
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
                const Material& mat = mConfig.mScene->GetMaterial(isect.matID);

                if ((isect.lightID >= 0) && (pathLength >= mMinPathLength))
                {
                    // Light source - get emmision
                    const AbstractLight *light = mConfig.mScene->GetLightPtr(isect.lightID);
                    if (light != NULL)
                        oRadiance +=
                              light->GetEmmision(surfPt, wol, Vec3f())
                            * pathThroughput;
                }

                if (mat.mDiffuseReflectance.IsZero() && mat.mPhongReflectance.IsZero())
                    // Zero reflectivity - there is no chance of contribution behing this reflection;
                    // we can cut the path without incorporation of bias
                    return;

                if ((mMaxPathLength > 0) && (pathLength >= mMaxPathLength))
                    return;

                // Russian roulette (based on reflectance of the whole BRDF)
                float reflectanceEstimate = 1.0f;
                if (mMaxPathLength == 0)
                {
                    reflectanceEstimate = Clamp(mat.GetRRContinuationProb(wol), 0.0f, 1.0f);
                    const float rnd = mRng.GetFloat();
                    if (rnd > reflectanceEstimate)
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

                pathThroughput *=
                        brdfSample.mSample
                      / (  brdfSample.mPdfW * brdfSample.mCompProbability // Monte Carlo est.
                         * reflectanceEstimate);                          // Russian roulette (optional)

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

    void EstimateIncomingRadiancePTNeeMis(
        const Ray       &aRay,
        const uint32_t   aPathLength,
        SpectrumF       &oEmmittedRadiance,
        SpectrumF       &oReflectedRadianceEstimate,
        const Frame     *aSurfFrame = NULL, // Only needed when you to compute PDF of a const env. light source sample
        float           *oEmmittedLightPdfW = NULL,
        float           *oEmmittedLightPickingProb = NULL
        )
    {
        PG3_ASSERT((mMaxPathLength == 0) || (aPathLength <= mMaxPathLength));

        oEmmittedRadiance.MakeZero();
        oReflectedRadianceEstimate.MakeZero();

        Isect isect(1e36f);
        if (mConfig.mScene->Intersect(aRay, isect))
        {
            // We hit some geometry

            const Vec3f surfPt = aRay.org + aRay.dir * isect.dist;
            Frame surfFrame;
            surfFrame.SetFromZ(isect.normal);
            const Vec3f wol = surfFrame.ToLocal(-aRay.dir);
            const Material& mat = mConfig.mScene->GetMaterial(isect.matID);

            if ((isect.lightID >= 0) && (aPathLength >= mMinPathLength))
            {
                // Light source - get emmision
                const AbstractLight *light = mConfig.mScene->GetLightPtr(isect.lightID);
                if (light != NULL)
                {
                    oEmmittedRadiance +=
                        light->GetEmmision(surfPt, wol, aRay.org, oEmmittedLightPdfW, aSurfFrame);

                    if ((oEmmittedLightPickingProb != NULL) && (aSurfFrame != NULL))
                    {
                        // Note: We compute light picking probability before illumination 
                        // computation of a next surface point starta; therefore, we don't 
                        // increase the intersection ID (mCurrentIsectId) and cached data 
                        // for light picking probability computation are still valid.
                        LightPickingProbability(aRay.org, *aSurfFrame, isect.lightID, *oEmmittedLightPickingProb);
                        // TODO: Uncomment this once proper environment map estimate is implemented.
                        //       Now there can be zero contribution estimate (and therefore zero picking probability)
                        //       even if the actual contribution is non-zero.
                        //PG3_ASSERT(emmittedLightSample.mRadiance.IsZero() || (*oLightProbability > 0.f));
                    }
                }
            }

            if (mat.mDiffuseReflectance.IsZero() && mat.mPhongReflectance.IsZero())
                // Zero reflectivity - there is no chance of contribution behing this reflection;
                // we can cut the path without incorporation of bias
                return;

            if ((mMaxPathLength > 0) && (aPathLength >= mMaxPathLength))
                return;

            mCurrentIsectId++;

            // Generate one sample by sampling the lights for direct illumination
            // ...if one more step is allowed
            if ((aPathLength + 1) >= mMinPathLength)
            {
                LightSampleActive lightSample;
                if (SampleLightsSingle(surfPt, surfFrame, lightSample))
                {
                    AddMISLightSampleContribution(
                        lightSample, surfPt, surfFrame, wol, mat,
                        oReflectedRadianceEstimate);
                }
            }

            // Russian roulette (based on reflectance of the whole BRDF)
            float reflectanceEstimate = 1.0f;
            if (mMaxPathLength == 0)
            {
                reflectanceEstimate = Clamp(mat.GetRRContinuationProb(wol), 0.0f, 1.0f);
                const float rnd = mRng.GetFloat();
                if (rnd > reflectanceEstimate)
                    return;
            }

            // Generate one sample by sampling the BRDF for both direct and indirect illumination
            BRDFSample brdfSample;
            mat.SampleBrdf(mRng, wol, brdfSample);
            if (brdfSample.mSample.Max() > 0.f)
            {
                SpectrumF   brdfEmmittedRadiance;
                SpectrumF   brdfReflectedRadianceEstimate;
                float       brdfLightPdfW = 0.f;
                float       brdfLightPickingProb = 0.f;

                const Vec3f wig = surfFrame.ToWorld(brdfSample.mWil);
                const float rayMin = EPS_RAY_COS(brdfSample.mWil.z);
                const Ray   brdfRay(surfPt, wig, rayMin);

                EstimateIncomingRadiancePTNeeMis(
                    brdfRay, aPathLength + 1,
                    brdfEmmittedRadiance, brdfReflectedRadianceEstimate,
                    &surfFrame, &brdfLightPdfW, &brdfLightPickingProb);

                // MIS for direct light
                if (!brdfEmmittedRadiance.IsZero())
                {
                    // TODO: Uncomment this once proper environment map estimate is implemented.
                    //       Now there can be zero contribution estimate (and therefore zero picking probability)
                    //       even if the actual contribution is non-zero.
                    //PG3_ASSERT(lightPickingProbability > 0.f);
                    PG3_ASSERT(brdfLightPdfW != INFINITY_F); // BRDF sampling should never hit a point light

                    // Compute multiple importance sampling MC estimator. 
                    oReflectedRadianceEstimate +=
                          (brdfSample.mSample * brdfEmmittedRadiance) // FIXME: sample contains only one component, will it work with MIS???
                        / (   (   brdfSample.mPdfW * brdfSample.mCompProbability
                                + brdfLightPdfW * brdfLightPickingProb)
                            * reflectanceEstimate); // Russian roulette
                }

                // Simple MC for reflected light
                if (!brdfReflectedRadianceEstimate.IsZero())
                {
                    oReflectedRadianceEstimate +=
                          (brdfSample.mSample * brdfReflectedRadianceEstimate)
                        / (   brdfSample.mPdfW
                            * brdfSample.mCompProbability
                            * reflectanceEstimate); // Russian roulette
                }
            }
        }
        else
        {
            // No intersection - get radiance from the background
            if (aPathLength >= mMinPathLength)
            {
                const BackgroundLight *backgroundLight = mConfig.mScene->GetBackground();
                const int32_t backgroundLightId = mConfig.mScene->GetBackgroundLightId();
                if (backgroundLight != NULL)
                {
                    oEmmittedRadiance +=
                        backgroundLight->GetEmmision(aRay.dir, true, oEmmittedLightPdfW, aSurfFrame);

                    if ((oEmmittedLightPickingProb != NULL) && (aSurfFrame != NULL))
                    {
                        LightPickingProbability(
                            aRay.org, 
                            *aSurfFrame, 
                            backgroundLightId, 
                            *oEmmittedLightPickingProb);
                        // TODO: Uncomment this once proper environment map estimate is implemented.
                        //       Now there can be zero contribution estimate (and therefore zero picking probability)
                        //       even if the actual contribution is non-zero.
                        //PG3_ASSERT(emmittedLightSample.mRadiance.IsZero() || (*oLightProbability > 0.f));
                    }
                }
            }
        }
    }
};
