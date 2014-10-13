#pragma once

#include <vector>
#include <cmath>
#include <fstream>
#include <string.h>
#include "spectrum.hxx"
#include "utils.hxx"

class Framebuffer
{
public:

    Framebuffer()
    {}

    //////////////////////////////////////////////////////////////////////////
    // Accumulation
    void AddRadiance(
        const Vec2f& aSample,
        const Spectrum& aRadiance)
    {
        if (aSample.x < 0 || aSample.x >= mResolution.x)
            return;

        if (aSample.y < 0 || aSample.y >= mResolution.y)
            return;

        int x = int(aSample.x);
        int y = int(aSample.y);

        mRadiance[x + y * mResX] = mRadiance[x + y * mResX] + aRadiance;
    }

    //////////////////////////////////////////////////////////////////////////
    // Methods for framebuffer operations
    void Setup(const Vec2f& aResolution)
    {
        mResolution = aResolution;
        mResX = int(aResolution.x);
        mResY = int(aResolution.y);
        mRadiance.resize(mResX * mResY);
        Clear();
    }

    void Clear()
    {
        memset(&mRadiance[0], 0, sizeof(Spectrum) * mRadiance.size());
    }

    void Add(const Framebuffer& aOther)
    {
        for (size_t i=0; i<mRadiance.size(); i++)
            mRadiance[i] = mRadiance[i] + aOther.mRadiance[i];
    }

    void Scale(float aScale)
    {
        Spectrum scaleAttenuation;
        scaleAttenuation.SetGreyAttenuation(aScale);

        for (size_t i = 0; i < mRadiance.size(); i++)
            mRadiance[i] *= scaleAttenuation;
    }

    //////////////////////////////////////////////////////////////////////////
    // Statistics
    float TotalLuminance()
    {
        float lum = 0;

        for (int y=0; y<mResY; y++)
        {
            for (int x=0; x<mResX; x++)
            {
                lum += Luminance(mRadiance[x + y*mResX]);
            }
        }

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

    //    for (int y=0; y<mResY; y++)
    //    {
    //        for (int x=0; x<mResX; x++)
    //        {
    //            ptr = &mRadiance[x + y*mResX];
    //            int r = int(std::pow(ptr->x, invGamma) * 255.f);
    //            int g = int(std::pow(ptr->y, invGamma) * 255.f);
    //            int b = int(std::pow(ptr->z, invGamma) * 255.f);

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
        uint   mFileSize;        // Size of file in bytes
        uint   mReserved01;      // 2x 2 reserved bytes
        uint   mDataOffset;      // Offset in bytes where data can be found (54)

        uint   mHeaderSize;      // 40B
        int    mWidth;           // Width in pixels
        int    mHeight;          // Height in pixels

        short  mColorPlates;     // Must be 1
        short  mBitsPerPixel;    // We use 24bpp
        uint   mCompression;     // We use BI_RGB ~ 0, uncompressed
        uint   mImageSize;       // mWidth x mHeight x 3B
        uint   mHorizRes;        // Pixels per meter (75dpi ~ 2953ppm)
        uint   mVertRes;         // Pixels per meter (75dpi ~ 2953ppm)
        uint   mPaletteColors;   // Not using palette - 0
        uint   mImportantColors; // 0 - all are important
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

        const float invGamma = 1.f / aGamma;
        for (int y=0; y<mResY; y++)
        {
            for (int x=0; x<mResX; x++)
            {
                // bmp is stored from bottom up
                const Spectrum &spectrum = mRadiance[x + (mResY-y-1)*mResX];
                SRGBSpectrum sRGB;
                spectrum.ConvertToSRGBSpectrum(sRGB);

                typedef unsigned char byte;
                float gammaBgr[3];
                gammaBgr[0] = std::pow(sRGB.z, invGamma) * 255.f;
                gammaBgr[1] = std::pow(sRGB.y, invGamma) * 255.f;
                gammaBgr[2] = std::pow(sRGB.x, invGamma) * 255.f;

                byte bgrB[3];
                bgrB[0] = byte(std::min(255.f, std::max(0.f, gammaBgr[0])));
                bgrB[1] = byte(std::min(255.f, std::max(0.f, gammaBgr[1])));
                bgrB[2] = byte(std::min(255.f, std::max(0.f, gammaBgr[2])));

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

        for (int y=0; y<mResY; y++)
        {
            for (int x=0; x<mResX; x++)
            {
                typedef unsigned char byte;
                byte rgbe[4] = {0,0,0,0};

                const Spectrum &spectrum = mRadiance[x + y*mResX];
                SRGBSpectrum sRGB;
                spectrum.ConvertToSRGBSpectrum(sRGB);
                float v = sRGB.Max();;

                if (v >= 1e-32f)
                {
                    int e;
                    v = float(frexp(v, &e) * 256.f / v);
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

    std::vector<Spectrum>   mRadiance;
    Vec2f                   mResolution;
    int                     mResX;
    int                     mResY;
};
