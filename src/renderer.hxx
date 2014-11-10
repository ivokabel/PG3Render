#pragma once

#include <vector>
#include <cmath>
#include "scene.hxx"
#include "framebuffer.hxx"
#include "types.hxx"

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

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator and copy constructor 
    // explicitly, the compiler may complain about not being able 
    // to create default implementations.
    AbstractRenderer & operator=(const AbstractRenderer&) = delete;
    AbstractRenderer(const AbstractRenderer&) = delete;

    virtual void RunIteration(uint32_t aIteration) = 0;

    void GetFramebuffer(Framebuffer& oFramebuffer)
    {
        oFramebuffer = mFramebuffer;

        if (mIterations > 0)
            oFramebuffer.Scale(1.f / mIterations);
    }

    //! Whether this renderer was used at all
    bool WasUsed() const { return mIterations > 0; }

public:

    uint         mMaxPathLength;
    uint         mMinPathLength;

protected:

    uint32_t     mIterations;
    Framebuffer  mFramebuffer;
    const Scene &mScene;
};
