#pragma once

#include <vector>
#include <cmath>
#include <omp.h>
#include <cassert>
#include "spectrum.hxx"
#include "renderer.hxx"
#include "rng.hxx"

class PathTracer : public AbstractRenderer
{
public:

    PathTracer(
        const Scene& aScene,
        int aSeed = 1234
    ) :
        AbstractRenderer(aScene), mRng(aSeed)
    {}

    virtual void RunIteration(int aIteration)
    {
        aIteration; // unused parameter

        const int resX = int(mScene.mCamera.mResolution.x);
        const int resY = int(mScene.mCamera.mResolution.y);

        for(int pixID = 0; pixID < resX * resY; pixID++)
        {
            //////////////////////////////////////////////////////////////////////////
            // Generate ray
            const int x = pixID % resX;
            const int y = pixID / resX;

            const Vec2f randomOffset = mRng.GetVec2f();
            const Vec2f basedCoords  = Vec2f(float(x), float(y));
            const Vec2f sample       = basedCoords + randomOffset;

            Ray   ray = mScene.mCamera.GenerateRay(sample);
            Isect isect;
            isect.dist = 1e36f;

            if(mScene.Intersect(ray, isect))
            {
                const Vec3f surfPt = ray.org + ray.dir * isect.dist;
                Frame frame;
                frame.SetFromZ(isect.normal);
                const Vec3f wol = frame.ToLocal(-ray.dir);
                const Material& mat = mScene.GetMaterial(isect.matID);

                ///////////////////////////////////////////////////////////////////////////////////
                // Direct illumination
                ///////////////////////////////////////////////////////////////////////////////////

                // We split the planar integral over the surface of all light sources 
                // into sub-integrals - one integral for each light source - and sum up 
                // all the sub-results.

                Spectrum LoDirect;
                LoDirect.SetSRGBGreyLight(0.0f);

                for(int i=0; i<mScene.GetLightCount(); i++)
                {
                    const AbstractLight* light = mScene.GetLightPtr(i);
                    assert(light != 0);

                    // Choose a random sample on the light
                    Vec3f wig; float lightDist;
                    Spectrum illumSample = light->SampleIllumination(surfPt, frame, wig, lightDist, mRng);
                    
                    if(illumSample.Max() > 0.)
                    {
                        // The illumination sample already contains 
                        // (outgoing radiance * geometric component) / PDF
                        // All what's left is to evaluate visibility and multiply by BRDF
                        if( ! mScene.Occluded(surfPt, wig, lightDist) )
                            LoDirect += illumSample * mat.EvalBrdf(frame.ToLocal(wig), wol);
                    }
                }

                ///////////////////////////////////////////////////////////////////////////////////
                // Emission
                ///////////////////////////////////////////////////////////////////////////////////

                Spectrum Le;

                const AbstractLight *light = 
                    isect.lightID < 0 ? NULL : mScene.GetLightPtr(isect.lightID);
                if (light != NULL)
                    Le = light->GetEmmision(surfPt, wol);
                else
                    Le.Zero();

                ///////////////////////////////////////////////////////////////////////////////////
                // TODO: Indirect illumination
                ///////////////////////////////////////////////////////////////////////////////////

                Spectrum LoIndirect;
                LoIndirect.Zero(); // debug

                // ...


                /*
                float dotLN = Dot(isect.normal, -ray.dir);

                // this illustrates how to pick-up the material properties of the intersected surface
                const Material& mat = mScene.GetMaterial( isect.matID );
                const Spectrum& rhoD = mat.mDiffuseReflectance;

                // this illustrates how to pick-up the area source associated with the intersected surface
                const AbstractLight *light = isect.lightID < 0 ?  0 : mScene.GetLightPtr( isect.lightID );
                // we cannot do anything with the light because it has no interface right now

                if(dotLN > 0)
                    mFramebuffer.AddRadiance(sample, (rhoD/PI_F) * Spectrum(dotLN));
                */

                const Spectrum Lo = Le + LoDirect + LoIndirect;

                mFramebuffer.AddRadiance(sample, Lo);
            }
            else
            {
                // No intersection - get light from the background

                const BackgroundLight *backgroundLight = mScene.GetBackground();
                Spectrum Le = backgroundLight->GetEmmision(ray.dir);

                mFramebuffer.AddRadiance(sample, Le);
            }
        }

        mIterations++;
    }

    Rng     mRng;
};
