#pragma once 

#include <vector>
#include <cmath>
#include <omp.h>
#include "spectrum.hxx"
#include "renderer.hxx"
#include "math.hxx"
#include "rng.hxx"
#include "types.hxx"

class NormalVisualiser : public AbstractRenderer
{
public:

    NormalVisualiser(
        const Config    &aConfig,
        int32_t          aSeed = 1234
        ) :
        AbstractRenderer(aConfig), mRng(aSeed)
    {}

    virtual void RunIteration(
        const Algorithm     aAlgorithm,
        uint32_t            aIteration) override
    {
        aAlgorithm; // unused param

        const int32_t resX = int32_t(mConfig.mScene->mCamera.mResolution.x);
        const int32_t resY = int32_t(mConfig.mScene->mCamera.mResolution.y);

        for (int32_t pixID = 0; pixID < resX * resY; pixID++)
        {
            // Generate ray
            const int32_t x = pixID % resX;
            const int32_t y = pixID / resX;

            const Vec2f sample =
                Vec2f(float(x), float(y)) +
                (aIteration == 0 ? Vec2f(0.5f) : mRng.GetVec2f());

            Ray ray = mConfig.mScene->mCamera.GenerateRay(sample);
            RayIntersection isect;
            isect.dist = 1e36f;

            // Intersect & Shade
            if (mConfig.mScene->Intersect(ray, isect))
            {
                const auto normal = isect.normal;
                SpectrumF color = SpectrumF().SetSRGBLight(
                    Math::Clamp(normal.x / 2.f + 0.5f, 0.f, 1.f),
                    Math::Clamp(normal.y / 2.f + 0.5f, 0.f, 1.f),
                    Math::Clamp(normal.z / 2.f + 0.5f, 0.f, 1.f));

                mFramebuffer.AddRadiance(x, y, color);
            }
        }

        mIterations++;
    }

    Rng mRng;
};
