#pragma once

#include <vector>
#include <cmath>
#include <omp.h>
#include <cassert>
#include "renderer.hxx"
#include "rng.hxx"

// Right now this is a copy of EyeLight renderer. The task is to change this 
// to a full-fledged path tracer.
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
        const int resX = int(mScene.mCamera.mResolution.x);
        const int resY = int(mScene.mCamera.mResolution.y);

        for(int pixID = 0; pixID < resX * resY; pixID++)
        {
            //////////////////////////////////////////////////////////////////////////
            // Generate ray
            const int x = pixID % resX;
            const int y = pixID / resX;

            const Vec2f sample = Vec2f(float(x), float(y)) + mRng.GetVec2f();

            Ray   ray = mScene.mCamera.GenerateRay(sample);
            Isect isect;
            isect.dist = 1e36f;

            if(mScene.Intersect(ray, isect))
            {
				const Vec3f surfPt = ray.org + ray.dir * isect.dist;
				Frame frame;
				frame.SetFromZ(isect.normal);
				const Vec3f wol = frame.ToLocal(-ray.dir);

				Vec3f LoDirect = Vec3f(0);
				const Material& mat = mScene.GetMaterial( isect.matID );

				for(int i=0; i<mScene.GetLightCount(); i++)
				{
					const AbstractLight* light = mScene.GetLightPtr(i);
					assert(light != 0);

					Vec3f wig; float lightDist;
					Vec3f illum = light->sampleIllumination(surfPt, frame, wig, lightDist);
					
					if(illum.Max() > 0)
					{
						if( ! mScene.Occluded(surfPt, wig, lightDist) )
							LoDirect += illum * mat.evalBrdf(frame.ToLocal(wig), wol);
					}
				}

				mFramebuffer.AddColor(sample, LoDirect);

				/*
                float dotLN = Dot(isect.normal, -ray.dir);

				// this illustrates how to pick-up the material properties of the intersected surface
				const Material& mat = mScene.GetMaterial( isect.matID );
				const Vec3f& rhoD = mat.mDiffuseReflectance;

				// this illustrates how to pick-up the area source associated with the intersected surface
				const AbstractLight *light = isect.lightID < 0 ?  0 : mScene.GetLightPtr( isect.lightID );
				// we cannot do anything with the light because it has no interface right now

                if(dotLN > 0)
                    mFramebuffer.AddColor(sample, (rhoD/PI_F) * Vec3f(dotLN));
				*/
            }
        }

        mIterations++;
    }

    Rng              mRng;
};
