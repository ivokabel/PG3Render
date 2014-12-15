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

    virtual void RunIteration(uint32_t aIteration)
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

            const Vec2f randomOffset = mRng.GetVec2f();
            const Vec2f basedCoords  = Vec2f(float(x), float(y));
            const Vec2f basedCoordsBound = Vec2f(float(x), float(y));
            const Vec2f sample       = basedCoords + randomOffset;
            //const Vec2f sample       = Clamp(basedCoords + randomOffset, basedCoords, ;

            Ray   ray = mScene.mCamera.GenerateRay(sample);
            Isect isect(1e36f);

            if (mScene.Intersect(ray, isect))
            {
                const Vec3f surfPt = ray.org + ray.dir * isect.dist;
                Frame frame;
                frame.SetFromZ(isect.normal);
                const Vec3f wol = frame.ToLocal(-ray.dir);
                const Material& mat = mScene.GetMaterial(isect.matID);

                ///////////////////////////////////////////////////////////////////////////////////
                // Direct illumination
                ///////////////////////////////////////////////////////////////////////////////////

                SpectrumF LoDirect;
                LoDirect.SetSRGBGreyLight(0.0f);

#if defined DIRECT_ILLUMINATION_SAMPLE_LIGHTS_ONLY

                // We split the planar integral over the surface of all light sources 
                // into sub-integrals - one integral for each light source - and sum up 
                // all the sub-results.

                for (uint32_t i=0; i<mScene.GetLightCount(); i++)
                {
                    const AbstractLight* light = mScene.GetLightPtr(i);
                    PG3_DEBUG_ASSERT(light != 0);

                    // Choose a random sample on the light
                    Vec3f wig; float lightDist;
                    SpectrumF illumSample = light->SampleIllumination(surfPt, frame, mRng, wig, lightDist);
                    
                    if (illumSample.Max() > 0.)
                    {
                        // The illumination sample already contains 
                        // (outgoing radiance * geometric component) / PDF
                        // All what's left is to evaluate visibility and multiply by BRDF
                        if ( ! mScene.Occluded(surfPt, wig, lightDist) )
                            LoDirect += illumSample * mat.EvalBrdf(frame.ToLocal(wig), wol);
                    }
                }

#elif defined DIRECT_ILLUMINATION_SAMPLE_BRDF_ONLY

                // Sample BRDF
                Vec3f wig;
                BRDFSample brdfSample;
                mat.SampleBrdf(mRng, frame, wol, brdfSample, wig);

                if (brdfSample.mSample.Max() > 0.)
                {
                    SpectrumF LiLight;

                    // Get radiance from the direction

                    const float rayMin = EPS_RAY_COS(brdfSample.mThetaInCos);
                    const Ray brdfRay(surfPt, wig, rayMin);
                    Isect brdfIsect(1e36f);
                    if (mScene.Intersect(brdfRay, brdfIsect))
                    {
                        if (brdfIsect.lightID >= 0)
                        {
                            // We hit light source geometry, get outgoing radiance
                            const Vec3f lightPt = brdfRay.org + brdfRay.dir * brdfIsect.dist;
                            const AbstractLight *light = mScene.GetLightPtr(brdfIsect.lightID);
                            LiLight = light->GetEmmision(lightPt, -wol);
                        }
                        else
                        {
                            //debug
                            printf("\nSelf-hit: cos=%.6f, 1-cos=%.6f, rayMin=%.6f\n",
                                   brdfSample.mThetaInCos, 1.0f - brdfSample.mThetaInCos, rayMin);
                            fflush(stdout);

                            // We hit a geometry which is not a light source, 
                            // no direct light contribution for this sample
                            LiLight.Zero();
                        }
                    }
                    else
                    {
                        // No geometry intersection, get radiance from background
                        const BackgroundLight *backgroundLight = mScene.GetBackground();
                        LiLight = backgroundLight->GetEmmision(wig);
                    }

                    // Compute the MC estimator. 
                    // The BRDF sample already contains 
                    //  (BRDF component attenuation * cosine) / (PDF * component probability)
                    // All what's left is to multiply it by radiance from the direction
                    LoDirect = brdfSample.mSample * LiLight;
                }

#else // MIS
                PG3_FATAL_ERROR("Not implemented!");
#endif

                ///////////////////////////////////////////////////////////////////////////////////
                // Emission
                ///////////////////////////////////////////////////////////////////////////////////

                SpectrumF Le;

                const AbstractLight *light = 
                    isect.lightID < 0 ? NULL : mScene.GetLightPtr(isect.lightID);
                if (light != NULL)
                    Le = light->GetEmmision(surfPt, wol);
                else
                    Le.Zero();

                ///////////////////////////////////////////////////////////////////////////////////
                // TODO: Indirect illumination
                ///////////////////////////////////////////////////////////////////////////////////

                SpectrumF LoIndirect;
                LoIndirect.Zero(); // debug

                // ...


                /*
                float dotLN = Dot(isect.normal, -ray.dir);

                // this illustrates how to pick-up the material properties of the intersected surface
                const Material& mat = mScene.GetMaterial( isect.matID );
                const SpectrumF& rhoD = mat.mDiffuseReflectance;

                // this illustrates how to pick-up the area source associated with the intersected surface
                const AbstractLight *light = isect.lightID < 0 ?  0 : mScene.GetLightPtr( isect.lightID );
                // we cannot do anything with the light because it has no interface right now

                if (dotLN > 0)
                    mFramebuffer.AddRadiance(x, y, (rhoD/PI_F) * SpectrumF(dotLN));
                */

                const SpectrumF Lo = Le + LoDirect + LoIndirect;

                mFramebuffer.AddRadiance(x, y, Lo);
            }
            else
            {
                // No intersection - get light from the background

                const BackgroundLight *backgroundLight = mScene.GetBackground();
                SpectrumF Le = backgroundLight->GetEmmision(ray.dir, true);

                mFramebuffer.AddRadiance(x, y, Le);
            }
        }

        mIterations++;
    }

    Rng     mRng;
};
