#pragma once

#include "spectrum.hxx"
#include <ImfRgbaFile.h>    // OpenEXR
//#include "pdf.hxx"
#include "debugging.hxx"
#include "math.hxx"
#include "distribution.hxx"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Adopted from SmallUPBP project and used as a reference for my own implementations.
// The image-loading code was used directly without almost any significant change.
///////////////////////////////////////////////////////////////////////////////////////////////////

class InputImage
{
public:
    InputImage(unsigned int width, unsigned int height)
    {
        mWidth  = width;
        mHeight = height;

        mData = new Spectrum[mWidth * mHeight];
    }

    ~InputImage()
    {
        if (mData)
            delete mData;
        mData   = NULL;
        mWidth  = 0;
        mHeight = 0;
    }

    Spectrum& ElementAt(unsigned int x, unsigned int y)
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(x, 0, mWidth);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(y, 0, mHeight);

        return mData[mWidth*y + x];
    }

    const Spectrum& ElementAt(unsigned int x, unsigned int y) const
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(x, 0, mWidth);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(y, 0, mHeight);

        return mData[mWidth*y + x];
    }

    Spectrum& ElementAt(unsigned int idx)
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(idx, 0, mWidth*mHeight);

        return mData[idx];
    }

    const Spectrum& ElementAt(unsigned int idx) const
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(idx, 0, mWidth*mHeight);

        return mData[idx];
    }

    Vec2ui Size() const
    {
        return Vec2ui(mWidth, mHeight);
    }

    unsigned int Width() const
    {
        return mWidth;
    }

    unsigned int Height() const
    { 
        return mHeight; 
    }

    Spectrum*       mData;
    unsigned int    mWidth;
    unsigned int    mHeight;
};

class EnvironmentMap
{
public:
    // Loads an OpenEXR image with an environment map with latitude-longitude mapping.
    EnvironmentMap(const std::string filename, float rotate, float scale) :
        mPlan2AngPdfCoeff(1.0f / (2.0f * PI_F * PI_F))
    {
        mImage = NULL;
        mDistribution = NULL;

        try
        {
            std::cout << "Loading:   Environment map '" << filename << "'" << std::endl;
            mImage = LoadImage(filename.c_str(), rotate, scale);
        }
        catch (...)
        {
            PG3_FATAL_ERROR("Environment map load failed!");
        }

        mDistribution = ConvertImageToPdf(mImage);
    }

    ~EnvironmentMap()
    {
        delete(mImage);
        delete(mDistribution);
    }

    // Samples direction on unit sphere proportionally to the luminance of the map. 
    // Returns the sample PDF and optionally it's radiance.
    Vec3f Sample(
        const Vec2f &aSamples,
        float       &oPdfW,
        Spectrum    *oRadiance = NULL) const
    {
        PG3_DEBUG_ASSERT(mImage != NULL);

        const Vec2ui imageSize = mImage->Size();
        Vec2f uv;
        float pdf;

        mDistribution->SampleContinuous(aSamples, uv, &pdf);
        PG3_DEBUG_ASSERT(pdf > 0.f);

        // Convert the sample's planar PDF over the rectangle [0,1]x[0,1] to 
        // the angular PDF on the unit sphere over the appropriate trapezoid
        //
        // angular pdf = planar pdf * planar segment surf. area / sphere segment surf. area
        //             = planar pdf * (1 / (width*height)) / (2*Pi*Pi*Sin(MidTheta) / (width*height))
        //             = planar pdf / (2*Pi*Pi*Sin(MidTheta))
        //
        // FIXME: Uniform sampling of a segment of the 2D distribution doesn't yield 
        //        uniform sampling of a corresponding segment on a sphere 
        //        - the closer we are to the poles, the denser the sampling will be 
        //        (even though the overall probability of the segment is correct).
        //
        // \int_a^b{1/hdh} = [ln(h)]_a^b = ln(b) - ln(a)
        oPdfW = pdf * mPlan2AngPdfCoeff / sinMidTheta(uv.y);

        const Vec3f direction = LatLong2Dir(uv);

#ifdef PG3_DEBUG_ASSERT_ENABLED
        {
            Vec2f uvdbg = Dir2LatLong(direction);
            PG3_DEBUG_ASSERT_FLOAT_EQUAL(uvdbg.y, uv.y, 1E-4f);
            // the closer we get to the poles, the closer the points are in the x coordinate
            float densityCompensation = uv.y * (1.0f - uv.y);
            float distanceXDirect    = fabs(uvdbg.x - uv.x);
            float distanceXOverZero  = -distanceXDirect + 1.0f;
            float distanceX = std::min(distanceXDirect, distanceXOverZero) * densityCompensation;
            PG3_DEBUG_ASSERT_FLOAT_LESS_THAN(distanceX, 1E-7f);
        }
#endif

        if (oRadiance)
            *oRadiance = LookupRadiance(uv, false);

        return direction;
    }

    // Gets radiance stored for the given direction and optionally its PDF. The direction
    // must be non-zero but not necessarily normalized.
    Spectrum Lookup(
        const Vec3f &aDirection, 
        bool         bDoBilinFiltering,
        float       *oPdfW = NULL) const
    {
        PG3_DEBUG_ASSERT(!aDirection.IsZero());

        const Vec3f normDir     = aDirection / aDirection.Length();
        const Vec2f uv          = Dir2LatLong(normDir);
        const Spectrum radiance = LookupRadiance(uv, bDoBilinFiltering);

        oPdfW; // unused parameter
        //if (oPdfW)
        //{
        //    Vec2f uv = Dir2LatLong(normDir);
        //    *oPdfW = mPlan2AngPdfCoeff * mDistribution->Pdf(uv) / sinMidTheta(uv[1]);
        //    if (*oPdfW == 0.0f) radiance = Spectrum(0);
        //}

        return radiance;
    }

private:
    // Loads, scales and rotates an environment map from an OpenEXR image on the given path.
    InputImage* LoadImage(const char *filename, float rotate, float scale) const
    {
        rotate = FmodX(rotate, 1.0f);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(rotate, 0.0f, 1.0f);

        Imf::RgbaInputFile file(filename, 1);
        Imath::Box2i dw = file.dataWindow();

        int width   = dw.max.x - dw.min.x + 1;
        int height  = dw.max.y - dw.min.y + 1;
        Imf::Rgba* rgbaData = new Imf::Rgba[height*width];

        file.setFrameBuffer(rgbaData - dw.min.x - dw.min.y * width, 1, width);
        file.readPixels(dw.min.y, dw.max.y);

        InputImage* image = new InputImage(width, height);

        int c = 0;
        int iRot = (int)(rotate * width);
        for (unsigned int j = 0; j < image->Height(); j++)
        {
            for (unsigned int i = 0; i < image->Width(); i++)
            {
                int x = i + iRot;
                if (x >= width) x -= width;
                image->ElementAt(x, j).SetSRGBLight(
                    rgbaData[c].r * scale,
                    rgbaData[c].g * scale,
                    rgbaData[c].b * scale
                    );
                c++;
            }
        }

        delete rgbaData;

        return image;
    }

    // Generates a 2D distribution with latitude-longitude mapping 
    // based on the luminance of the provided environment map image
    Distribution2D* ConvertImageToPdf(const InputImage* image) const
    {
        // Prepare source distribution data from the original environment map image data, 
        // i.e. convert image values so that the probability of a pixel within 
        // the lattitute-longitude parametrization is equal to the angular probability of 
        // the projected segment on a unit sphere.

        const Vec2ui size   = image->Size();
        float *srcData      = new float[size.x * size.y];

        for (unsigned int row = 0; row < size.y; ++row)
        {
            // We compute the relative surface area of the current segment on the unit sphere.
            // We can ommit the height of the segment because it only changes the result 
            // by a multiplication constant and thus doesn't affect the shape of the resulting PDF.
            const float segmAvgV    = ((float)row + 0.5f) / size.y;
            const float sinAvgTheta = sinf(PI_F * segmAvgV);

            const unsigned int rowOffset = row * size.x;

            for (unsigned int column = 0; column < size.x; ++column)
            {
                const float luminance = 
                    Luminance(image->ElementAt(column, row));
                srcData[rowOffset + column] =
                    sinAvgTheta * luminance;
            }
        }

        return new Distribution2D(srcData, size.x, size.y);
    }

    // Returns direction on unit sphere such that its longitude equals 2*PI*u and its latitude equals PI*v.
    PG3_PROFILING_NOINLINE
    Vec3f LatLong2Dir(const Vec2f &uv) const
    {
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(uv.x, 0.f, 1.f);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(uv.y, 0.f, 1.f);

        const float phi   = -(uv.x - 0.5f) * 2 * PI_F; // we rotate in the opposite direction
        const float theta = uv.y * PI_F;

        const float sinTheta = sin(theta);

        return Vec3f(sinTheta * cos(phi), sinTheta * sin(phi), cos(theta));
    }

    // Returns vector [u,v] in [0,1]x[0,1]. The direction must be non-zero and normalized.
    PG3_PROFILING_NOINLINE
    Vec2f Dir2LatLong(const Vec3f &aDirection) const
    {
        PG3_DEBUG_ASSERT(!aDirection.IsZero() /*(aDirection.Length() == 1.0f)*/);

        // TODO: Use optimized formulae from Jarda's writeout

        const float phi   = -atan2f(aDirection.y, aDirection.x); // we rotate in the opposite direction
        const float theta = acosf(aDirection.z);

        // Convert from [-Pi,Pi] to [0,1]
        //const float uTemp = fmodf(phi * 0.5f * INV_PI_F, 1.0f);
        //const float u = Clamp(uTemp, 0.f, 1.f); // TODO: maybe not necessary
        const float u = Clamp(0.5f + phi * 0.5f * INV_PI_F, 0.f, 1.f);

        // Convert from [0,Pi] to [0,1]
        const float v = Clamp(theta * INV_PI_F, 0.f, 1.0f);

        return Vec2f(u, v);
    }

    // Returns radiance for the given lat long coordinates. Optionally does bilinear filtering.
    PG3_PROFILING_NOINLINE 
    Spectrum LookupRadiance(const Vec2f &uv, bool bDoBilinFiltering) const
    {
        PG3_DEBUG_ASSERT(mImage != NULL);

        const Vec2ui imageSize = mImage->Size();

        // Convert uv coords to mImage coordinates
        Vec2f xy = uv * Vec2f((float)imageSize.x, (float)imageSize.y);

        return mImage->ElementAt(
            Clamp((unsigned int)xy.x, 0u, imageSize.x - 1),
            Clamp((unsigned int)xy.y, 0u, imageSize.y - 1));

        // TODO: bilinear filtering
        bDoBilinFiltering;








        //int width = mImage->Width();
        //int height = mImage->Height();

        //float xf = u * width;
        //float yf = v * height;

        //int xi1 = Utils::clamp<int>((int)xf, 0, width - 1);
        //int yi1 = Utils::clamp<int>((int)yf, 0, height - 1);

        //int xi2 = xi1 == width - 1 ? xi1 : xi1 + 1;
        //int yi2 = yi1 == height - 1 ? yi1 : yi1 + 1;

        //float tx = xf - (float)xi1;
        //float ty = yf - (float)yi1;

        //return (1 - ty) * ((1 - tx) * mImage->ElementAt(xi1, yi1) + tx * mImage->ElementAt(xi2, yi1))
        //    + ty * ((1 - tx) * mImage->ElementAt(xi1, yi2) + tx * mImage->ElementAt(xi2, yi2));
    }

    // The sine of latitude of the midpoint of a map pixel defined by the given v coordinate.
    float sinMidTheta(const float v) const
    {
        PG3_DEBUG_ASSERT(mImage != NULL);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(v, 0.0f, 1.0f);

        const float height  = (float)mImage->Height();

        const int segment   = std::min((int)(v * height), (int)height - 1);
        const float result  = sinf(PI_F * (segment + 0.5f) / height);

        PG3_DEBUG_ASSERT(result > 0.f && result <= 1.f);

        return result;
    }

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator and copy constructor 
    // explicitly, the compiler may complain about not being able 
    // to create default implementations.
    EnvironmentMap & operator=(const EnvironmentMap&) = delete;
    EnvironmentMap(const EnvironmentMap&) = delete;

    InputImage*     mImage;             // Environment map itself
    Distribution2D* mDistribution;      // 2D distribution of the environment map
    const float     mPlan2AngPdfCoeff;  // Coefficient for conversion from planar to angular PDF
};
