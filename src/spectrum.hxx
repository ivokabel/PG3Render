#pragma once 

#include "math.hxx"
#include "types.hxx"

// Implementation of Spectral Power Distribution in sRGB colourspace coordinates.
template<typename T>
class SRGBSpectrumBase : public Vec3Base<T>
{
    static_assert(std::is_arithmetic<T>::value, "T must be arithmetic");

public:

    SRGBSpectrumBase() {};

    // This constructor is needed to use the overloaded operators 
    // from Vec3Base, which return objects of type Vec3Base.
    SRGBSpectrumBase(const Vec3Base<T> &aVec) : Vec3Base<T>(aVec) {};

    // Sets as light quantity which is grey in sRGB colourspace
    void SetSRGBGreyLight(T a)
    {
        x = y = z = a;
    };

    // Sets as light quantity in sRGB colourspace coordinates
    void SetSRGBLight(T r, T g, T b)
    {
        x = r;
        y = g;
        z = b;
    };

    // Sets as attenuation quantity which is neutral - doesn't change the colour of incident light
    void SetGreyAttenuation(T a)
    {
        x = y = z = a;
    };

    // Sets as attenuation quantity which will transform white sRGB incident light to light 
    // with specified sRGB colour coordinates.
    void SetSRGBAttenuation(T r, T g, T b)
    {
        x = r;
        y = g;
        z = b;
    };

    // Converts the internal representation to SRGBSpectrumBase.
    // TODO: Make this a conversion operator to make things more fancy?
    // TODO: In spectral version: Where do we want to do gamut mapping?
    void ConvertToSRGBSpectrum(SRGBSpectrumBase &oSpectrum) const
    {
        oSpectrum = *this;
    }

    // Later: ConvertToSampledSpectrum8()?

    // Make this instance zero
    SRGBSpectrumBase& MakeZero()
    {
        for (uint32_t i = 0; i<3; i++)
            Get(i) = 0.0f;
        return *this;
    }

    // sRGB luminance
    T Luminance() const
    {
        return
            0.212671f * x +
            0.715160f * y +
            0.072169f * z;
    }

private:

    // TODO: ? Sanity check (only in debug mode!) ? :
    // Set whether we were initialised as light or attenuation (enum: light, att, none) and 
    // throw an error is we are set incorrectly later on. Is it really helpfull?
};

// Specializations
typedef SRGBSpectrumBase<float>     SRGBSpectrumF;
typedef SRGBSpectrumBase<double>    SRGBSpectrumD;

// The SpectrumX class stores a Spectral Power Distribution used for various physical quantities.
// The two main usages are: light-related quantities (flux, irradiance, radiance, etc.) and 
// attenuation properties (BSDF attenuation and reflectance).
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

// Warning: 
// The implementations behind the SpectrumX alias must allow resetting to zero spectrum 
// by memsetting the whole structure to 0. Used in framebuffer - if not possible, 
// the affected code must be rewritten.

typedef SRGBSpectrumF   SpectrumF;
typedef SRGBSpectrumD   SpectrumD;
//typedef SampledSpectrum8F  SpectrumF;
//typedef SampledSpectrum8D SpectrumD;

