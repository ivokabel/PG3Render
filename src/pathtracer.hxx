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
        //else if (aAlgorithm == kPathTracingNaive)
        //{
        //    // Path tracer with next event estimate and mis for direct illumination
        //    EstimateIncomingRadiancePTNeeMis(aRay, oRadiance);
        //}
        else
        {
            PG3_FATAL_ERROR("Unknown algorithm!");
        }
    }

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
};
