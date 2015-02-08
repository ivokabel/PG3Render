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

                        AddSingleLightSampleContribution(
                            lightSample, surfPt, surfFrame, wol, mat, 
                            LoDirect);
                    }
                }
                else if (aAlgorithm == kDirectIllumLightSamplingSingle)
                {
                    LightSample lightSample;
                    if (SampleLightsSingle(surfPt, surfFrame, lightSample))
                        AddSingleLightSampleContribution(
                            lightSample, surfPt, surfFrame, wol, mat, 
                            LoDirect);
                }
                else if (aAlgorithm == kDirectIllumBRDFSampling)
                {
                    // Sample BRDF
                    BRDFSample brdfSample;
                    mat.SampleBrdf(mRng, wol, brdfSample);
                    if (brdfSample.mSample.Max() > 0.)
                    {
                        SpectrumF LiLight;
                        GetDirectRadianceFromDirection(
                            surfPt,
                            surfFrame,
                            brdfSample.mWil,
                            LiLight);

                        // Compute the two-step MC estimator. 
                        LoDirect = 
                              (brdfSample.mSample * LiLight)
                            / (brdfSample.mPdfW * brdfSample.mCompProbability);
                    }
                }
                else if (aAlgorithm == kDirectIllumMIS)
                {
                    // Multiple importance sampling

                    // Generate one sample by sampling the lights
                    LightSample lightSample;
                    if (SampleLightsSingle(surfPt, surfFrame, lightSample))
                    {
                        AddMISLightSampleContribution(
                            lightSample, surfPt, surfFrame, wol, mat,
                            LoDirect);
                    }

                    // Generate one sample by sampling the BRDF
                    BRDFSample brdfSample;
                    mat.SampleBrdf(mRng, wol, brdfSample);
                    AddMISBrdfSampleContribution(
                        brdfSample, surfPt, surfFrame,
                        LoDirect);
                }
                else
                {
                    PG3_FATAL_ERROR("Unknown algorithm!");
                }


                ///////////////////////////////////////////////////////////////////////////////////
                // Emission
                ///////////////////////////////////////////////////////////////////////////////////

                SpectrumF Le;

                const AbstractLight *light = 
                    isect.lightID < 0 ? NULL : mScene.GetLightPtr(isect.lightID);
                if (light != NULL)
                    Le = light->GetEmmision(surfPt, wol, Vec3f(), NULL, Frame()); // last 2 params are not used
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
                SpectrumF Le = backgroundLight->GetEmmision(ray.dir, true, NULL, Frame());

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
        PickSingleLight(aSurfPt, aSurfFrame, chosenLightId, lightProbability);

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

    void GetDirectRadianceFromDirection(
        const Vec3f     &aSurfPt,
        const Frame     &aSurfFrame,
        const Vec3f     &aWil,
              SpectrumF &oLight,
              float     *oPdfW = NULL,
              float     *oLightProbability = NULL
        )
    {
        int32_t lightId = -1;

        const Vec3f wig = aSurfFrame.ToWorld(aWil);
        const float rayMin = EPS_RAY_COS(aWil.z);
        const Ray brdfRay(aSurfPt, wig, rayMin);
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
                oLight = light->GetEmmision(lightPt, ligthWol, aSurfPt, oPdfW, aSurfFrame);

                lightId = brdfIsect.lightID;
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
            const BackgroundLight *backgroundLight = mScene.GetBackground();
            if (backgroundLight != NULL)
            {
                oLight = backgroundLight->GetEmmision(wig, false, oPdfW, aSurfFrame);
                lightId = mScene.GetBackgroundLightId();
            }
            else
                oLight.MakeZero(); // No background light
        }

        PG3_ASSERT(oLight.IsZero() || (lightId >= 0));

        if ((oLightProbability != NULL) && (lightId >= 0))
        {
            LightPickingProbability(aSurfPt, aSurfFrame, lightId, *oLightProbability);
            // TODO: Uncomment this once proper environment map estimate is implemented.
            //       Now there can be zero contribution estimate (and therefore zero picking probability)
            //       even if the actual contribution is non-zero.
            //PG3_ASSERT(oLight.IsZero() || (*oLightProbability > 0.f));
        }
    }

    // Pick one of the light sources randomly (proportionally to their estimated contribution)
    void PickSingleLight(
        const Vec3f         &aSurfPt,
        const Frame         &aSurfFrame, 
              int32_t       &oChosenLightId, 
              float         &oLightProbability)
    {
        const size_t lightCount = mScene.GetLightCount();
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
            // TODO: Make it a PT's memeber to avoid unnecessary allocations?
            std::vector<float> lightContrPseudoCdf(lightCount + 1);

            // Estimate the contribution of all available light sources
            float estimatesSum = 0.f;
            lightContrPseudoCdf[0] = 0.f;
            for (uint32_t i = 0; i < lightCount; i++)
            {
                const AbstractLight* light = mScene.GetLightPtr(i);
                PG3_ASSERT(light != 0);

                estimatesSum += light->EstimateContribution(aSurfPt, aSurfFrame, mRng);
                lightContrPseudoCdf[i + 1] = estimatesSum;
            }

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

                PG3_ASSERT_VAL_IN_RANGE(oChosenLightId, 0, (int32_t)(lightCount - 1));
            }
            else
            {
                // Pick a light uniformly
                const float rndVal = mRng.GetFloat();
                oChosenLightId    = std::min((int32_t)(rndVal * lightCount), (int32_t)(lightCount - 1));
                oLightProbability = 1.f / lightCount;

                PG3_ASSERT_VAL_IN_RANGE(oChosenLightId, 0, (int32_t)(lightCount - 1));
            }
        }
    }

    // Computes probability of picking the specified light source
    void LightPickingProbability(
        const Vec3f         &aSurfPt,
        const Frame         &aSurfFrame, 
              uint32_t       aLightId, 
              float         &oLightProbability)
    {
        const size_t lightCount = mScene.GetLightCount();

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
            // Estimate the contribution of all available light sources
            float estimatesSum  = 0.f;
            float lightEstimate = 0.f;
            for (uint32_t i = 0; i < lightCount; i++)
            {
                const AbstractLight* light = mScene.GetLightPtr(i);
                PG3_ASSERT(light != 0);

                const float estimate = light->EstimateContribution(aSurfPt, aSurfFrame, mRng);
                if (i == aLightId)
                    lightEstimate = estimate;
                estimatesSum += estimate;
            }

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
        const LightSample   &aLightSample,
        const Vec3f         &aSurfPt,
        const Frame         &aSurfFrame, 
        const Vec3f         &aWol,
        const Material      &aMat,
              SpectrumF     &oLightBuffer)
    {
        if (aLightSample.mSample.Max() <= 0.)
            // The light emmits zero radiance in this direction
            return;
        if (mScene.Occluded(aSurfPt, aLightSample.mWig, aLightSample.mDist))
            // The light is not visible from this point
            return;

        if (aLightSample.mPdfW != INFINITY_F)
            // Planar or angular light sources - compute two-step MC estimator.
            oLightBuffer +=
                  aLightSample.mSample
                * aMat.EvalBrdf(aSurfFrame.ToLocal(aLightSample.mWig), aWol)
                / (aLightSample.mPdfW * aLightSample.mLightProbability);
        else
            // Point light - the contribution of a single light is computed 
            // analytically (without MC estimation), there is only one MC 
            // estimation left - the estimation of the sum of contributions 
            // of all light sources.
            oLightBuffer +=
                  aLightSample.mSample
                * aMat.EvalBrdf(aSurfFrame.ToLocal(aLightSample.mWig), aWol)
                / aLightSample.mLightProbability;
    }

    void AddMISLightSampleContribution(
        const LightSample   &aLightSample,
        const Vec3f         &aSurfPt,
        const Frame         &aSurfFrame, 
        const Vec3f         &aWol,
        const Material      &aMat,
              SpectrumF     &oLightBuffer)
    {
        if (aLightSample.mSample.Max() <= 0.)
            // The light emmits zero radiance in this direction
            return;
        if (mScene.Occluded(aSurfPt, aLightSample.mWig, aLightSample.mDist))
            // The light is not visible from this point
            return;

        const Vec3f wil = aSurfFrame.ToLocal(aLightSample.mWig);

        // Since the BRDF sampling only works for planar and angular light sources, we can't use 
        // the multiple importance sampling scheme for the whole reflectance integral. We split 
        // the integral into two parts - one for planar and angular light sources, and one 
        // for point light sources. The first part can be handled by both BRDF and light 
        // sampling strategies; therefore, we can combine the two with MIS. The second part 
        // (point lights) can only be hadled by the light sampling strategy.
        //
        // We could separate the two computations completely, what whould result in computing 
        // separate sets of light samples for each of the two integrals, but we can use 
        // one light sampling routine to generate samples for both integrals in one place 
        // to make things easier for us (but maybe a little bit confusing at first sight).
        //
        // If we chose an area or angular light, we compute the MIS-ed part and for point 
        // light sampling it means that we generated an empty sample. Vice versa, if we chose 
        // a point light, we compute non-zero estimator for the point lights integral and 
        // for the MIS-ed integral this means we generated and empty sample. PDFs from 
        // the light sampling routine can be used directly without any adaptation 
        // in both computations.
        //
        // TODO: If we look at each of the two integral parts separtately, it is obvious, 
        //       that chosing a light source using a strategy which sometimes choses "no light"
        //       causes worse estimator performance than a technique which always choses a light.
        //       However, estimating the first and the second part separately will require 
        //       filtering a proper set of lights when choosing a light and also when computing 
        //       the probability of picking one.

        if (aLightSample.mPdfW != INFINITY_F)
        {
            // Planar or angular light source was chosen

            // PDF of the BRDF sampling technique for the chosen light direction
            const float brdfPdfW = aMat.GetPdfW(aWol, wil);

            // MIS MC estimator
            oLightBuffer +=
                  aLightSample.mSample
                * aMat.EvalBrdf(wil, aWol)
                / (aLightSample.mPdfW * aLightSample.mLightProbability + brdfPdfW);
        }
        else
        {
            // Point light was chosen

            // The contribution of a single light is computed analytically, there is only one 
            // MC estimation left - the estimation of the sum of contributions of all light sources.
            oLightBuffer +=
                  aLightSample.mSample
                * aMat.EvalBrdf(wil, aWol)
                / aLightSample.mLightProbability;
        }
    }

    void AddMISBrdfSampleContribution(
        const BRDFSample    &aBrdfSample,
        const Vec3f         &aSurfPt,
        const Frame         &aSurfFrame,
              SpectrumF     &oLightBuffer)
    {
        if (aBrdfSample.mSample.Max() <= 0.f)
            // The material is a complete blocker in this direction
            return;

        SpectrumF LiLight;
        float lightPdfW = 0.f;
        float lightPickingProbability = 0.f;
        GetDirectRadianceFromDirection(
            aSurfPt,
            aSurfFrame,
            aBrdfSample.mWil,
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
        PG3_ASSERT(lightPdfW != INFINITY_F); // BRDF sampling should never hit a point light

        // Compute multiple importance sampling MC estimator. 
        oLightBuffer +=
              (aBrdfSample.mSample * LiLight) // FIXME: the sample contains only one component, will it work with MIS???
            / (aBrdfSample.mPdfW * aBrdfSample.mCompProbability + lightPdfW * lightPickingProbability);
    }

    Rng     mRng;
};
