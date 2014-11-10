#pragma once 

#include <vector>
#include <cmath>
#include <omp.h>
#include "spectrum.hxx"
#include "renderer.hxx"
#include "rng.hxx"
#include "types.hxx"

class EyeLight : public AbstractRenderer
{
public:

    EyeLight(
        const Scene& aScene,
        int32_t      aSeed = 1234
    ) :
        AbstractRenderer(aScene), mRng(aSeed)
    {}

    virtual void RunIteration(uint32_t aIteration)
    {
        const int32_t resX = int32_t(mScene.mCamera.mResolution.x);
        const int32_t resY = int32_t(mScene.mCamera.mResolution.y);

        for (int32_t pixID = 0; pixID < resX * resY; pixID++)
        {
            //////////////////////////////////////////////////////////////////////////
            // Generate ray
            const int32_t x = pixID % resX;
            const int32_t y = pixID / resX;

            const Vec2f sample = Vec2f(float(x), float(y)) +
                (aIteration == 1 ? Vec2f(0.5f) : mRng.GetVec2f());

            Ray   ray = mScene.mCamera.GenerateRay(sample);
            Isect isect;
            isect.dist = 1e36f;

            if (mScene.Intersect(ray, isect))
            {
                float dotLN = Dot(isect.normal, -ray.dir);

                Spectrum color;
                if (dotLN > 0)
                    color.SetSRGBGreyLight(dotLN);
                else
                    color.SetSRGBLight(-dotLN, 0, 0);
                mFramebuffer.AddRadiance(sample, color);
            }
        }

        mIterations++;
    }

    Rng              mRng;
};
