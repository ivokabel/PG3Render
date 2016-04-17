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
        LightSamplingContext lightSamplingCtx(mConfig.mScene->GetLightCount());

        RayIntersection isect(1e36f);
        if (mConfig.mScene->Intersect(aRay, isect))
        {
            const Vec3f surfPt = aRay.PointAt(isect.dist);
            Frame surfFrame;
            surfFrame.SetFromZ(isect.normal);
            const Vec3f wol = surfFrame.ToLocal(-aRay.dir);
            const AbstractMaterial& mat = mConfig.mScene->GetMaterial(isect.matID);

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
                    LightSample lightSample;
                    light->SampleIllumination(surfPt, surfFrame, mat, mRng, lightSample);

                    AddSingleLightSampleContribution(
                        lightSample, surfPt, surfFrame, mat, wol,
                        LoDirect);
                }
            }
            else if (aAlgorithm == kDirectIllumLightSamplingSingle)
            {
                LightSample lightSample;
                if (SampleLightsSingle(surfPt, surfFrame, mat, lightSamplingCtx, lightSample))
                    AddSingleLightSampleContribution(
                        lightSample, surfPt, surfFrame, mat, wol,
                        LoDirect);
            }
            else if (aAlgorithm == kDirectIllumBsdfSampling)
            {
                // Sample BSDF
                MaterialRecord matRecord(wol);
                mat.SampleBsdf(mRng, matRecord);
                if (!matRecord.IsBlocker())
                {
                    SpectrumF LiLight;
                    GetDirectRadianceFromDirection(
                        surfPt,
                        surfFrame,
                        mat,
                        matRecord.mWil,
                        lightSamplingCtx,
                        LiLight);

                    if (matRecord.mPdfW != Math::InfinityF())
                        // Finite BSDF: Compute the two-step MC estimator.
                        LoDirect =
                              (   matRecord.mAttenuation
                                * matRecord.ThetaInCosAbs()
                                * LiLight)
                            / (   matRecord.mPdfW               // Monte Carlo est.
                                * matRecord.mCompProbability);  // Discrete multi-component MC
                    else
                        // Dirac BSDF: compute the integral analytically
                        LoDirect =
                              (   matRecord.mAttenuation
                                * LiLight)
                            / matRecord.mCompProbability;      // Discrete multi-component MC

                    PG3_ASSERT_VEC3F_NONNEGATIVE(LoDirect);
                }
            }
            else if (aAlgorithm == kDirectIllumMis)
            {
                // Multiple importance sampling

                // Generate one sample by sampling the lights
                LightSample lightSample;
                if (SampleLightsSingle(surfPt, surfFrame, mat, lightSamplingCtx, lightSample))
                {
                    AddMISLightSampleContribution(
                        lightSample, 1, 1, surfPt, surfFrame, wol, mat, mRng,
                        LoDirect);
                }

                // Generate one sample by sampling the BSDF
                MaterialRecord matRecord(wol);
                mat.SampleBsdf(mRng, matRecord);
                AddDirectIllumMISBrdfSampleContribution(
                    matRecord, 1, 1, surfPt, surfFrame, mat, lightSamplingCtx,
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
