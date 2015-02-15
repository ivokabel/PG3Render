#pragma once

#include <vector>
#include <cmath>
#include "scene.hxx"
#include "framebuffer.hxx"
#include "types.hxx"

enum Algorithm
{
    kEyeLight,
    kDirectIllumBRDFSampling,
    kDirectIllumLightSamplingAll,
    kDirectIllumLightSamplingSingle,
    kDirectIllumMIS,
    //kIndirectIllum,
    kPathTracing,
    kAlgorithmCount
};

class AbstractRenderer
{
public:

    AbstractRenderer(const Scene& aScene) : mScene(aScene)
    {
        mMinPathLength = 0;
        mMaxPathLength = 2;
        mIterations = 0;
        mFramebuffer.Setup(aScene.mCamera.mResolution);
        mCurrentIsectId = 0;
    }

    virtual ~AbstractRenderer(){}

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator and copy constructor 
    // explicitly, the compiler may complain about not being able 
    // to create default implementations.
    AbstractRenderer & operator=(const AbstractRenderer&) = delete;
    AbstractRenderer(const AbstractRenderer&) = delete;

    virtual void RunIteration(
        const Algorithm     aAlgorithm,
        uint32_t            aIteration) = 0;

    void GetFramebuffer(Framebuffer& oFramebuffer)
    {
        oFramebuffer = mFramebuffer;

        if (mIterations > 0)
            oFramebuffer.Scale(FramebufferFloat(1.) / mIterations);
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

    // Intersection counter to determine whether cached intersecton data are still valid.
    // Used for light source contribution estimates (later used for light picking probabilities).
    uint32_t     mCurrentIsectId;
};
