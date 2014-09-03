#pragma once

#include <vector>
#include <cmath>
#include "scene.hxx"
#include "framebuffer.hxx"

class AbstractRenderer
{
public:

    AbstractRenderer(const Scene& aScene) : mScene(aScene)
    {
        mMinPathLength = 0;
        mMaxPathLength = 2;
        mIterations = 0;
        mFramebuffer.Setup(aScene.mCamera.mResolution);
    }

    virtual ~AbstractRenderer(){}

    virtual void RunIteration(int aIteration) = 0;

    void GetFramebuffer(Framebuffer& oFramebuffer)
    {
        oFramebuffer = mFramebuffer;

        if(mIterations > 0)
            oFramebuffer.Scale(1.f / mIterations);
    }

    //! Whether this renderer was used at all
    bool WasUsed() const { return mIterations > 0; }

public:

    uint         mMaxPathLength;
    uint         mMinPathLength;

protected:

    int          mIterations;
    Framebuffer  mFramebuffer;
    const Scene& mScene;
};
