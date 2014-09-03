#pragma once 

#include <vector>
#include <cmath>
#include <omp.h>
#include "renderer.hxx"
#include "rng.hxx"

class EyeLight : public AbstractRenderer
{
public:

    EyeLight(
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

            const Vec2f sample = Vec2f(float(x), float(y)) +
                (aIteration == 1 ? Vec2f(0.5f) : mRng.GetVec2f());

            Ray   ray = mScene.mCamera.GenerateRay(sample);
            Isect isect;
            isect.dist = 1e36f;

            if(mScene.Intersect(ray, isect))
            {
                float dotLN = Dot(isect.normal, -ray.dir);

                if(dotLN > 0)
                    mFramebuffer.AddColor(sample, Vec3f(dotLN));
                else
                    mFramebuffer.AddColor(sample, Vec3f(-dotLN, 0, 0));
            }
        }

        mIterations++;
    }

    Rng              mRng;
};
