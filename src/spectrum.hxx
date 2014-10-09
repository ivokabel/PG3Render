#pragma once 

#include "math.hxx"

class SRGBSpectrum;
    
// The Spectrum class stores a Spectral Power Distribution used for various physical quantities.
// The two main usages are: light-related quantities (flux, irradiance, radiance, etc.) and 
// attenuation properties (BRDF/BSDF attenuation and reflectance).
//
// Implementation note:
//
// There are (will be) various implementations with various base functions. The actual 
// implementation is chosen via typedef. Firstly, this allows us to avoid polymorphism, 
// thus avoiding necessity of using virtual methods and therefore allowing the compiler to 
// inline short functions, leading to a more efficient code. Secondly, this also allows us to 
// store the instances directly instead of needing to allocate them dynamically according to 
// the current spectral representation. This allows to improve locality of memory access 
// allowing us to achieve higher efficiency of the code.
typedef SRGBSpectrum Spectrum;
//typedef SampledSpectrum8 Spectrum;

// Warning: 
// The implementations behind the Spectrum alias must me reset to zero spectrum by memsetting 
// the whole structure to 0. Used in framebuffer - if not possible, the code must be rewritten.

// Implementation of Spectral Power Distribution in sRGB colourspace coordinates.
class SRGBSpectrum : public Vec3f
{
public:

    SRGBSpectrum() {};

    // This constructor is needed to use the overloaded operators 
    // from Vec3f, which return objects of type Vec3f.
    SRGBSpectrum(const Vec3f &aVec) : Vec3f(aVec) {};

    // Sets as light quantity which is grey in sRGB colourspace
    void SetSRGBGreyLight(float a)
    {
        x = y = z = a;
    };

    // Sets as light quantity in sRGB colourspace coordinates
    void SetSRGBLight(float r, float g, float b)
    {
        x = r;
        y = g;
        z = b;
    };

    // Sets as attenuation quantity which is neutral - doesn't change the colour of incident light
    void SetGreyAttenuation(float a)
    {
        x = y = z = a;
    };

    // Sets as attenuation quantity which will transform white sRGB incident light to light 
    // with specified sRGB colour coordinates.
    void SetSRGBAttenuation(float r, float g, float b)
    {
        x = r;
        y = g;
        z = b;
    };

    // Converts the internal representation to SRGBSpectrum.
    // TODO: Make this a conversion operator to make things more fancy?
    // TODO: In spectral version: Where do we want to do gamut mapping?
    void ConvertToSRGBSpectrum(SRGBSpectrum &oSpectrum) const
    {
        oSpectrum = *this;
    }

    // Later: ConvertToSampledSpectrum8()?

    // Make this instance zero
    SRGBSpectrum& Zero()
    {
        for (int i = 0; i<3; i++)
            Get(i) = 0.0f;
        return *this;
    }

private:

    // TODO: ? Sanity check (only in debug mode!) ? :
    // Set whether we were initialised as light or attenuation (enum: light, att, none) and 
    // throw an error is we are set incorrectly later on. Is it really helpfull?
};