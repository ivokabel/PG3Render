#pragma once

#include "math.hxx"
#include "spectrum.hxx"
#include "types.hxx"
#include "hard_config.hxx"


//////////////////////////////////////////////////////////////////////////
// Used for generating samples on a light source
class LightSample
{
public:
    // (outgoing radiance * abs(cosine theta_in)) or it's equivalent (e.g. for point lights)
    // Note that this structure is designed for the angular version of the rendering equation 
    // to allow convenient combination of multiple sampling strategies in multiple-importance scheme.
    SpectrumF   mSample;

    float       mPdfW;              // Angular PDF. Equals infinity for point lights.
    float       mLightProbability;  // Probability of picking the light source which generated this sample.
                                    // Should equal 1.0 if there's just one light source in the scene
    Vec3f       mWig;
    float       mDist;
};

