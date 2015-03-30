#pragma once

#include "pathtracerbase.hxx"

class DirectIllumination : public PathTracerBase
{
public:

    DirectIllumination(
        const Config    &aConfig,
        int32_t          aSeed = 1234
    ) :
        PathTracerBase(aConfig, aSeed) {}

    virtual void EstimateIncomingRadiance(
        const Algorithm      aAlgorithm,
        const Ray           &aRay,
              SpectrumF     &oRadiance
        )
    {
        Isect isect(1e36f);
        if (mConfig.mScene->Intersect(aRay, isect))
        {
            mCurrentIsectId++;

            const Vec3f surfPt = aRay.org + aRay.dir * isect.dist;
            Frame surfFrame;
            surfFrame.SetFromZ(isect.normal);
            const Vec3f wol = surfFrame.ToLocal(-aRay.dir);
            const Material& mat = mConfig.mScene->GetMaterial(isect.matID);

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

                for (uint32_t i = 0; i < mConfig.mScene->GetLightCount(); i++)
                {
                    const AbstractLight* light = mConfig.mScene->GetLightPtr(i);
                    PG3_ASSERT(light != 0);

                    // Choose a random sample on the light
                    LightSampleActive lightSample;
                    light->SampleIllumination(surfPt, surfFrame, mRng, lightSample);

                    AddSingleLightSampleContribution(
                        lightSample, surfPt, surfFrame, wol, mat,
                        LoDirect);
                }
            }
            else if (aAlgorithm == kDirectIllumLightSamplingSingle)
            {
                LightSampleActive lightSample;
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
                LightSampleActive lightSample;
                if (SampleLightsSingle(surfPt, surfFrame, lightSample))
                {
                    AddMISLightSampleContribution(
                        lightSample, surfPt, surfFrame, wol, mat,
                        LoDirect);
                }

                // Generate one sample by sampling the BRDF
                BRDFSample brdfSample;
                mat.SampleBrdf(mRng, wol, brdfSample);
                AddDirectIllumMISBrdfSampleContribution(
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
                isect.lightID < 0 ? NULL : mConfig.mScene->GetLightPtr(isect.lightID);
            if (light != NULL)
                Le = light->GetEmmision(surfPt, wol, Vec3f());
            else
                Le.MakeZero();

            ///////////////////////////////////////////////////////////////////////////////////
            // Result
            ///////////////////////////////////////////////////////////////////////////////////

            oRadiance = Le + LoDirect;
        }
        else
        {
            // No intersection - get radiance from the background
            const BackgroundLight *backgroundLight = mConfig.mScene->GetBackground();
            if (backgroundLight != NULL)
                oRadiance = backgroundLight->GetEmmision(aRay.dir, true);
            else
                oRadiance.MakeZero(); // No background light
        }
    }
};
