#pragma once

#include "spectrum.hxx"
#include "renderer.hxx"
#include "rng.hxx"
#include "types.hxx"
#include "hardsettings.hxx"

#include <vector>
#include <cmath>
#include <omp.h>
#include <cassert>

class PathTracer : public AbstractRenderer
{
public:

    PathTracer(
        const Scene &aScene,
        int32_t      aSeed = 1234
    ) :
        AbstractRenderer(aScene), mRng(aSeed)
    {}

    virtual void RunIteration(
        const Algorithm     aAlgorithm,
        uint32_t            aIteration)
    {
        aIteration; // unused parameter

        const uint32_t resX = uint32_t(mScene.mCamera.mResolution.x);
        const uint32_t resY = uint32_t(mScene.mCamera.mResolution.y);

        for (uint32_t pixID = 0; pixID < resX * resY; pixID++)
        {
            //////////////////////////////////////////////////////////////////////////
            // Generate ray
            const uint32_t x = pixID % resX;
            const uint32_t y = pixID / resX;

            const Vec2f randomOffset     = mRng.GetVec2f();
            const Vec2f basedCoords      = Vec2f(float(x), float(y));
            const Vec2f basedCoordsBound = Vec2f(float(x), float(y));
            const Vec2f sample           = basedCoords + randomOffset;

            Ray ray = mScene.mCamera.GenerateRay(sample);
            Isect isect(1e36f);

            if (mScene.Intersect(ray, isect))
            {
                const Vec3f surfPt = ray.org + ray.dir * isect.dist;
                Frame surfFrame;
                surfFrame.SetFromZ(isect.normal);
                const Vec3f wol = surfFrame.ToLocal(-ray.dir);
                const Material& mat = mScene.GetMaterial(isect.matID);

                ///////////////////////////////////////////////////////////////////////////////////
                // Direct illumination
                ///////////////////////////////////////////////////////////////////////////////////

                SpectrumF LoDirect;
                LoDirect.MakeZero();

                if (aAlgorithm == kDirectIllumLightSamplingAll)
                {
                    // We split the planar integral over the surface of all light sources 
                    // into sub-integrals - one integral for each light source - and sum up 
                    // all the sub-results into one.

                    for (uint32_t i=0; i<mScene.GetLightCount(); i++)
                    {
                        const AbstractLight* light = mScene.GetLightPtr(i);
                        PG3_ASSERT(light != 0);

                        // Choose a random sample on the light
                        LightSample lightSample;
                        light->SampleIllumination(surfPt, surfFrame, mRng, lightSample);
                    
                        if (lightSample.mSample.Max() > 0.)
                        {
                            if (!mScene.Occluded(surfPt, lightSample.mWig, lightSample.mDist))
                            {
                                if (lightSample.mPdfW != INFINITY_F)
                                    // Planar or angular light sources - compute two-step MC estimator.
                                    // The illumination sample already contains 
                                    // (outgoing radiance * geometric component).
                                    LoDirect += 
                                          lightSample.mSample 
                                        * mat.EvalBrdf(surfFrame.ToLocal(lightSample.mWig), wol)
                                        / (lightSample.mPdfW * lightSample.mLightProbability);
                                else
                                    // Point light - the contribution of a single light is computed 
                                    // analytically (without MC estimation), there is only one MC 
                                    // estimation left - the estimation of the sum of contributions 
                                    // of all light sources.
                                    LoDirect +=
                                          lightSample.mSample 
                                        * mat.EvalBrdf(surfFrame.ToLocal(lightSample.mWig), wol)
                                        / lightSample.mLightProbability;
                            }
                        }
                    }
                }
                else if (aAlgorithm == kDirectIllumLightSamplingSingle)
                {
                    LightSample lightSample;
                    if (SampleLightsSingle(surfPt, surfFrame, lightSample) && 
                        (lightSample.mSample.Max() > 0.))
                    {
                        if (!mScene.Occluded(surfPt, lightSample.mWig, lightSample.mDist))
                        {
                            if (lightSample.mPdfW != INFINITY_F)
                                // Planar or angular light sources - compute two-step MC estimator.
                                // The illumination sample already contains 
                                // (outgoing radiance * geometric component).
                                LoDirect += 
                                      lightSample.mSample 
                                    * mat.EvalBrdf(surfFrame.ToLocal(lightSample.mWig), wol)
                                    / (lightSample.mPdfW * lightSample.mLightProbability);
                            else
                                // Point light - the contribution of a single light is computed 
                                // analytically (without MC estimation), there is only one MC 
                                // estimation left - the estimation of the sum of contributions 
                                // of all light sources.
                                LoDirect +=
                                      lightSample.mSample 
                                    * mat.EvalBrdf(surfFrame.ToLocal(lightSample.mWig), wol)
                                    / lightSample.mLightProbability;
                        }
                    }
                }
                else if (aAlgorithm == kDirectIllumBRDFSampling)
                {
                    // Sample BRDF
                    BRDFSample brdfSample;
                    mat.SampleBrdf(mRng, wol, brdfSample);
                    Vec3f wig = surfFrame.ToWorld(brdfSample.mWil);

                    if (brdfSample.mSample.Max() > 0.)
                    {
                        SpectrumF LiLight;

                        // Get radiance from the direction
                        const float rayMin = EPS_RAY_COS(brdfSample.mWil.z);
                        const Ray brdfRay(surfPt, wig, rayMin);
                        Isect brdfIsect(1e36f);
                        if (mScene.Intersect(brdfRay, brdfIsect))
                        {
                            if (brdfIsect.lightID >= 0)
                            {
                                // We hit light source geometry, get outgoing radiance
                                const Vec3f lightPt = brdfRay.org + brdfRay.dir * brdfIsect.dist;
                                const AbstractLight *light = mScene.GetLightPtr(brdfIsect.lightID);
                                Frame frame;
                                frame.SetFromZ(brdfIsect.normal);
                                const Vec3f ligthWol = frame.ToLocal(-brdfRay.dir);
                                LiLight = light->GetEmmision(lightPt, ligthWol);
                            }
                            else
                            {
                                // We hit a geometry which is not a light source, 
                                // no direct light contribution for this sample
                                LiLight.MakeZero();
                            }
                        }
                        else
                        {
                            // No geometry intersection, get radiance from background
                            const BackgroundLight *backgroundLight = mScene.GetBackground();
                            if (backgroundLight != NULL)
                                LiLight = backgroundLight->GetEmmision(wig);
                            else
                                LiLight.MakeZero(); // No background light
                        }

                        // Compute the two-step MC estimator. 
                        // The BRDF sample already contains (BRDF component attenuation * cosine thea_in)
                        LoDirect = 
                              (brdfSample.mSample * LiLight)
                            / (brdfSample.mPdfW * brdfSample.mCompProbability);
                    }
                }
                else if (aAlgorithm == kDirectIllumMIS)
                {
                    PG3_FATAL_ERROR("Not implemented!");

                    // Multiple importance sampling

                    // TODO: Generate one sample by sampling the lights
                    // - get the sample and its angular pdf
                    // - get pdf of the BRDF sampling technique for the direction

                    // TODO: Generate one sample by sampling the BRDF
                    // - get the sample and its angular pdf
                    // - get pdf of light sampling technique for the direction (requires shooting a 

                    // TODO: Combine the two samples using the balance heuristics
                }
                else
                {
                    PG3_FATAL_ERROR("Not implemented!");
                }


                ///////////////////////////////////////////////////////////////////////////////////
                // Emission
                ///////////////////////////////////////////////////////////////////////////////////

                SpectrumF Le;

                const AbstractLight *light = 
                    isect.lightID < 0 ? NULL : mScene.GetLightPtr(isect.lightID);
                if (light != NULL)
                    Le = light->GetEmmision(surfPt, wol);
                else
                    Le.MakeZero();

                ///////////////////////////////////////////////////////////////////////////////////
                // TODO: Indirect illumination
                ///////////////////////////////////////////////////////////////////////////////////

                SpectrumF LoIndirect;
                LoIndirect.MakeZero(); // debug

                // TODO: ...

                ///////////////////////////////////////////////////////////////////////////////////
                // Total illumination
                ///////////////////////////////////////////////////////////////////////////////////

                const SpectrumF Lo = Le + LoDirect + LoIndirect;
                mFramebuffer.AddRadiance(x, y, Lo);
            }
            else
            {
                // No intersection - get radiance from the background

                const BackgroundLight *backgroundLight = mScene.GetBackground();
                SpectrumF Le = backgroundLight->GetEmmision(ray.dir, true);

                mFramebuffer.AddRadiance(x, y, Le);
            }
        }

        mIterations++;
    }

    bool SampleLightsSingle(
        const Vec3f &aSurfPt, 
        const Frame &aSurfFrame, 
        LightSample &oLightSample)
    {
        // We split the planar integral over the surface of all light sources 
        // into sub-integrals - one integral for each light source - and estimate 
        // the sum of all the sub-results using a (discrete, second-level) MC estimator.

        int32_t chosenLightId   = -1;
        float lightProbability  = 0.f;

        // Pick one of the light sources randomly
        // (proportionally to their estimated contribution)
        const size_t lightCount = mScene.GetLightCount();
        if (lightCount == 0)
            chosenLightId = -1;
        else if (lightCount == 1)
        {
            // If there's just one light, we skip the unnecessary (and fairly expensive) picking process
            chosenLightId    = 0;
            lightProbability = 1.f;
        }
        else
        {
            // Non-normalized CDF for all light sources
            // TODO: Make it a PT's memeber to avoid unnecessary allocations?
            std::vector<float> lightContrPseudoCdf(lightCount + 1); 

            // Estimate the contribution of all available light sources
            float estimatesSum = 0.f;
            lightContrPseudoCdf[0] = 0.f;
            for (uint32_t i = 0; i < lightCount; i++)
            {
                const AbstractLight* light = mScene.GetLightPtr(i);
                PG3_ASSERT(light != 0);

                // Choose a random sample on the light
                estimatesSum +=
                    //1.f; // debug
                    light->EstimateContribution(aSurfPt, aSurfFrame, mRng);
                lightContrPseudoCdf[i + 1] = estimatesSum;
            }

            if (estimatesSum > 0.f)
            {
                // Pick a light
                const float rndVal = mRng.GetFloat() * estimatesSum;
                uint32_t lightId = 0;
                // TODO: Use std::find to find the chosen light?
                for (; (rndVal > lightContrPseudoCdf[lightId + 1]) && (lightId < lightCount); lightId++);
                chosenLightId    = (int32_t)lightId;
                lightProbability = 
                    (lightContrPseudoCdf[lightId + 1] - lightContrPseudoCdf[lightId]) / estimatesSum;
            }
            else
                chosenLightId = -1;
        }

        // Sample the chosen light
        if (chosenLightId >= 0)
        {
            // Choose a random sample on the light
            const AbstractLight* light = mScene.GetLightPtr(chosenLightId);
            PG3_ASSERT(light != 0);
            light->SampleIllumination(aSurfPt, aSurfFrame, mRng, oLightSample);
            oLightSample.mLightProbability = lightProbability;

            return true;
        }
        else
            return false;
    }

    Rng     mRng;
};
