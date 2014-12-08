#pragma once

#include <vector>
#include <cmath>
#include <fstream>
#include <string.h>
#include "spectrum.hxx"
#include "utils.hxx"
#include "types.hxx"

#define USE_DOUBLE_FRAMEBUFFER
#ifdef USE_DOUBLE_FRAMEBUFFER
typedef SpectrumD       FramebufferSpectrum;
typedef SRGBSpectrumD   FramebufferSRGBSpectrum;
typedef double          FramebufferFloat;
#else
typedef SpectrumF       FramebufferSpectrum;
typedef SRGBSpectrumF   FramebufferSRGBSpectrum;
typedef float           FramebufferFloat;
#endif

class Framebuffer
{
public:

    Framebuffer()
    {}

    //////////////////////////////////////////////////////////////////////////
    // Accumulation
    void AddRadiance(
        const Vec2f     &aSample,
        const SpectrumF &aRadiance)
    {
        if (aSample.x < 0 || aSample.x >= mResolution.x)
            return;

        if (aSample.y < 0 || aSample.y >= mResolution.y)
            return;

        int32_t x = int32_t(aSample.x);
        int32_t y = int32_t(aSample.y);

        // For some reason the following line only works if I create a SpectrumF instance first
        //mRadiance[x + y * mResX] = mRadiance[x + y * mResX] + aRadiance;
        mRadiance[x + y * mResX] += aRadiance;
    }

    //////////////////////////////////////////////////////////////////////////
    // Methods for framebuffer operations
    void Setup(const Vec2f& aResolution)
    {
        mResolution = aResolution;
        mResX = int32_t(aResolution.x);
        mResY = int32_t(aResolution.y);
        mRadiance.resize(mResX * mResY);
        Clear();
    }

    void Clear()
    {
        memset(&mRadiance[0], 0, sizeof(FramebufferSpectrum) * mRadiance.size());
    }

    void Add(const Framebuffer& aOther)
    {
        for (size_t i=0; i<mRadiance.size(); i++)
            mRadiance[i] = mRadiance[i] + aOther.mRadiance[i];
    }

    void Scale(FramebufferFloat aScale)
    {
        FramebufferSpectrum scaleAttenuation;
        scaleAttenuation.SetGreyAttenuation(aScale);

        for (size_t i = 0; i < mRadiance.size(); i++)
            mRadiance[i] *= scaleAttenuation;
    }

    //////////////////////////////////////////////////////////////////////////
    // Statistics
    FramebufferFloat TotalLuminance()
    {
        FramebufferFloat lum = 0.0;

        for (int32_t y=0; y<mResY; y++)
            for (int32_t x=0; x<mResX; x++)
                lum += mRadiance[x + y*mResX].Luminance();

        return lum;
    }

    //////////////////////////////////////////////////////////////////////////
    // Saving

    //void SavePPM(
    //    const char *aFilename,
    //    float       aGamma = 1.f)
    //{
    //    const float invGamma = 1.f / aGamma;

    //    std::ofstream ppm(aFilename);
    //    ppm << "P3" << std::endl;
    //    ppm << mResX << " " << mResY << std::endl;
    //    ppm << "255" << std::endl;

    //    Vec3f *ptr = &mRadiance[0];

    //    for (int32_t y=0; y<mResY; y++)
    //    {
    //        for (int32_t x=0; x<mResX; x++)
    //        {
    //            ptr = &mRadiance[x + y*mResX];
    //            int32_t r = int32_t(std::pow(ptr->x, invGamma) * 255.f);
    //            int32_t g = int32_t(std::pow(ptr->y, invGamma) * 255.f);
    //            int32_t b = int32_t(std::pow(ptr->z, invGamma) * 255.f);

    //            ppm << std::min(255, std::max(0, r)) << " "
    //                << std::min(255, std::max(0, g)) << " "
    //                << std::min(255, std::max(0, b)) << " ";
    //        }

    //        ppm << std::endl;
    //    }
    //}

    //void SavePFM(const char* aFilename)
    //{
    //    std::ofstream ppm(aFilename, std::ios::binary);
    //    ppm << "PF" << std::endl;
    //    ppm << mResX << " " << mResY << std::endl;
    //    ppm << "-1" << std::endl;

    //    ppm.write(reinterpret_cast<const char*>(&mRadiance[0]),
    //        mRadiance.size() * sizeof(Vec3f));
    //}

    //////////////////////////////////////////////////////////////////////////
    // Saving BMP
    struct BmpHeader
    {
        uint    mFileSize;        // Size of file in bytes
        uint    mReserved01;      // 2x 2 reserved bytes
        uint    mDataOffset;      // Offset in bytes where data can be found (54)

        uint    mHeaderSize;      // 40B
        int32_t mWidth;           // Width in pixels
        int32_t mHeight;          // Height in pixels

        short   mColorPlates;     // Must be 1
        short   mBitsPerPixel;    // We use 24bpp
        uint    mCompression;     // We use BI_RGB ~ 0, uncompressed
        uint    mImageSize;       // mWidth x mHeight x 3B
        uint    mHorizRes;        // Pixels per meter (75dpi ~ 2953ppm)
        uint    mVertRes;         // Pixels per meter (75dpi ~ 2953ppm)
        uint    mPaletteColors;   // Not using palette - 0
        uint    mImportantColors; // 0 - all are important
    };

    void SaveBMP(
        const char *aFilename,
        float       aGamma = 1.f)
    {
        std::ofstream bmp(aFilename, std::ios::binary);
        BmpHeader header;
        bmp.write("BM", 2);
        header.mFileSize   = uint(sizeof(BmpHeader) + 2) + mResX * mResX * 3;
        header.mReserved01 = 0;
        header.mDataOffset = uint(sizeof(BmpHeader) + 2);
        header.mHeaderSize = 40;
        header.mWidth      = mResX;
        header.mHeight     = mResY;
        header.mColorPlates     = 1;
        header.mBitsPerPixel    = 24;
        header.mCompression     = 0;
        header.mImageSize       = mResX * mResY * 3;
        header.mHorizRes        = 2953;
        header.mVertRes         = 2953;
        header.mPaletteColors   = 0;
        header.mImportantColors = 0;

        bmp.write((char*)&header, sizeof(header));

        const FramebufferFloat invGamma = (FramebufferFloat)1.f / aGamma;
        for (int32_t y=0; y<mResY; y++)
        {
            for (int32_t x=0; x<mResX; x++)
            {
                // bmp is stored from bottom up
                const FramebufferSpectrum &spectrum = mRadiance[x + (mResY-y-1)*mResX];
                FramebufferSRGBSpectrum sRGB;
                spectrum.ConvertToSRGBSpectrum(sRGB);

                typedef unsigned char byte;
                FramebufferFloat gammaBgr[3];
                gammaBgr[0] = std::pow(sRGB.z, invGamma) * FramebufferFloat(255.);
                gammaBgr[1] = std::pow(sRGB.y, invGamma) * FramebufferFloat(255.);
                gammaBgr[2] = std::pow(sRGB.x, invGamma) * FramebufferFloat(255.);

                byte bgrB[3];
                bgrB[0] = byte(std::min(FramebufferFloat(255.), std::max(FramebufferFloat(0.), gammaBgr[0])));
                bgrB[1] = byte(std::min(FramebufferFloat(255.), std::max(FramebufferFloat(0.), gammaBgr[1])));
                bgrB[2] = byte(std::min(FramebufferFloat(255.), std::max(FramebufferFloat(0.), gammaBgr[2])));

                bmp.write((char*)&bgrB, sizeof(bgrB));
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Saving HDR
    void SaveHDR(const char* aFilename)
    {
        std::ofstream hdr(aFilename, std::ios::binary);

        hdr << "#?RADIANCE" << '\n';
        hdr << "# PG3Render" << '\n';
        hdr << "FORMAT=32-bit_rle_rgbe" << '\n' << '\n';
        hdr << "-Y " << mResY << " +X " << mResX << '\n';

        for (int32_t y=0; y<mResY; y++)
        {
            for (int32_t x=0; x<mResX; x++)
            {
                typedef unsigned char byte;
                byte rgbe[4] = {0,0,0,0};

                const FramebufferSpectrum &spectrum = mRadiance[x + y*mResX];
                FramebufferSRGBSpectrum sRGB;
                spectrum.ConvertToSRGBSpectrum(sRGB);
                FramebufferFloat v = sRGB.Max();

                if (v >= (FramebufferFloat)1e-32)
                {
                    int32_t e;
                    v = frexp(v, &e) * FramebufferFloat(256.) / v;
                    rgbe[0] = byte(sRGB.x * v);
                    rgbe[1] = byte(sRGB.y * v);
                    rgbe[2] = byte(sRGB.z * v);
                    rgbe[3] = byte(e + 128);
                }

                hdr.write((char*)&rgbe[0], 4);
            }
        }
    }

private:

    std::vector<FramebufferSpectrum>    mRadiance;
    Vec2f                               mResolution;
    int32_t                             mResX;
    int32_t                             mResY;
};
