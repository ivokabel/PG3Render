#pragma once 

#include "math.hxx"
#include "spectrum.hxx"

class Material
{
public:
    Material()
    {
        Reset();
    }

    void Reset()
    {
        mDiffuseReflectance = Spectrum(0);
        mPhongReflectance   = Spectrum(0);
        mPhongExponent      = 1.f;
    }

	Spectrum evalBrdf( const Vec3f& wil, const Vec3f& wol ) const
	{
		if( wil.z <= 0 && wol.z <= 0)
			return Spectrum(0);

		Spectrum diffuseComponent(mDiffuseReflectance / PI_F);

		// Spectrum glossyComponent  = 

		return diffuseComponent /* + glossyComponent */;
	}

    Spectrum mDiffuseReflectance;
    Spectrum mPhongReflectance;
    float mPhongExponent;
};
