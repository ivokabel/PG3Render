#pragma once

#include "spectrum.hxx"
#include "math.hxx"
#include "types.hxx"

#include <vector>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstdlib>

//#define EPS_COSINE            1e-6f
#define EPS_RAY                 1e-3f
// Dynamic version (chooses from [EPS_RAY, N*EPS_RAY])
// eps + n * (1-cos) * eps = (1 + n * (1-cos)) * eps
// The smaller the cosine is, the larger epsilon we use to avoid 
// numerical problems, e.g. causing self-intersection when shooting rays from a surface,
// while starting as close to the surface as possible to avoid light leaks
#define EPS_RAY_COS(_cos)       ((1.f + 2.f * (1.f - (_cos))) * EPS_RAY)
#define EPS_DIST                1e-6f

#define IS_MASKED(val, mask)   (((val) & (mask)) == (mask))

float FresnelDielectric(
    float aCosInc,
    float mIOR)
{
    if (mIOR < 0)
        return 1.f;

    float etaIncOverEtaTrans;

    if (aCosInc < 0.f)
    {
        aCosInc = -aCosInc;
        etaIncOverEtaTrans = mIOR;
    }
    else
        etaIncOverEtaTrans = 1.f / mIOR;

    const float sinTrans2 = Sqr(etaIncOverEtaTrans) * (1.f - Sqr(aCosInc));
    const float cosTrans = std::sqrt(std::max(0.f, 1.f - sinTrans2));

    const float term1 = etaIncOverEtaTrans * cosTrans;
    const float rParallel =
        (aCosInc - term1) / (aCosInc + term1);

    const float term2 = etaIncOverEtaTrans * aCosInc;
    const float rPerpendicular =
        (term2 - cosTrans) / (term2 + cosTrans);

    return 0.5f * (Sqr(rParallel) + Sqr(rPerpendicular));
}

// reflect vector through (0,0,1)
Vec3f ReflectLocal(const Vec3f& aVector)
{
    return Vec3f(-aVector.x, -aVector.y, aVector.z);
}

//////////////////////////////////////////////////////////////////////////
// Cosine lobe hemisphere sampling

Vec3f SamplePowerCosHemisphereW(
    const Vec2f  &aSamples,
    const float   aPower,
    float        *oPdfW = NULL)
{
    const float term1 = 2.f * PI_F * aSamples.x;
    const float term2 = std::pow(aSamples.y, 1.f / (aPower + 1.f));
    const float term3 = std::sqrt(1.f - term2 * term2);

    if (oPdfW)
        *oPdfW = (aPower + 1.f) * std::pow(term2, aPower) * (0.5f * INV_PI_F);

    return Vec3f(
        std::cos(term1) * term3,
        std::sin(term1) * term3,
        term2);
}

float PowerCosHemispherePdfW(
    const Vec3f  &aNormal,
    const Vec3f  &aDirection,
    const float   aPower)
{
    const float cosTheta = std::max(0.f, Dot(aNormal, aDirection));

    return (aPower + 1.f) * std::pow(cosTheta, aPower) * (INV_PI_F * 0.5f);
}

float PowerCosHemispherePdfW(
    const Vec3f  &aDirectionLocal,
    const float   aPower)
{
    const float cosTheta = std::max(0.f, aDirectionLocal.z);

    return (aPower + 1.f) * std::pow(cosTheta, aPower) * (INV_PI_F * 0.5f);
}

//////////////////////////////////////////////////////////////////////////
// Disc sampling

Vec2f SampleConcentricDisc(
    const Vec2f &aSamples)
{
    float phi, r;

    float a = 2*aSamples.x - 1;   /* (a,b) is now on [-1,1]^2 */
    float b = 2*aSamples.y - 1;

    if (a > -b)      /* region 1 or 2 */
    {
        if (a > b)   /* region 1, also |a| > |b| */
        {
            r = a;
            phi = (PI_F/4.f) * (b/a);
        }
        else        /* region 2, also |b| > |a| */
        {
            r = b;
            phi = (PI_F/4.f) * (2.f - (a/b));
        }
    }
    else            /* region 3 or 4 */
    {
        if (a < b)   /* region 3, also |a| >= |b|, a != 0 */
        {
            r = -a;
            phi = (PI_F/4.f) * (4.f + (b/a));
        }
        else        /* region 4, |b| >= |a|, but a==0 and b==0 could occur. */
        {
            r = -b;

            if (b != 0)
                phi = (PI_F/4.f) * (6.f - (a/b));
            else
                phi = 0;
        }
    }

    Vec2f res;
    res.x = r * std::cos(phi);
    res.y = r * std::sin(phi);
    return res;
}

float ConcentricDiscPdfA()
{
    return INV_PI_F;
}


//////////////////////////////////////////////////////////////////////////
/// Sample direction in the upper hemisphere with cosine-proportional pdf
/** The returned PDF is with respect to solid angle measure */
Vec3f SampleCosHemisphereW(
    const Vec2f  &aSamples,
    float        *oPdfW = NULL)
{
    const float term1 = 2.f * PI_F * aSamples.x;
    const float term2 = std::sqrt(1.f - aSamples.y);

    const Vec3f ret(
        std::cos(term1) * term2,
        std::sin(term1) * term2,
        std::sqrt(aSamples.y));

    if (oPdfW)
        *oPdfW = ret.z * INV_PI_F;

    return ret;
}

float CosHemispherePdfW(
    const Vec3f  &aNormal,
    const Vec3f  &aDirection)
{
    return std::max(0.f, Dot(aNormal, aDirection)) * INV_PI_F;
}

float CosHemispherePdfW(
    const Vec3f  &aDirectionLocal)
{
    return std::max(0.f, aDirectionLocal.z) * INV_PI_F;
}

// Sample Triangle
// returns barycentric coordinates
Vec2f SampleUniformTriangle(const Vec2f &aSamples)
{
    const float xSqr = std::sqrt(aSamples.x);

    return Vec2f(1.f - xSqr, aSamples.y * xSqr);
}

//////////////////////////////////////////////////////////////////////////
// Sphere sampling

Vec3f SampleUniformSphereW(
    const Vec2f  &aSamples,
    float        *oPdfSA)
{
    const float term1 = 2.f * PI_F * aSamples.x;
    const float term2 = 2.f * std::sqrt(aSamples.y - aSamples.y * aSamples.y);

    const Vec3f ret(
        std::cos(term1) * term2,
        std::sin(term1) * term2,
        1.f - 2.f * aSamples.y);

    if (oPdfSA)
        //*oPdfSA = 1.f / (4.f * PI_F);
        *oPdfSA = INV_PI_F * 0.25f;

    return ret;
}

float UniformSpherePdfW()
{
    //return (1.f / (4.f * PI_F));
    return INV_PI_F * 0.25f;
}


//////////////////////////////////////////////////////////////////////////
// Utilities for converting PDF between Area (A) and Solid angle (W)
// WtoA = PdfW * cosine / distance_squared
// AtoW = PdfA * distance_squared / cosine

float PdfWtoA(
    const float aPdfW,
    const float aDist,
    const float aCosThere)
{
    return aPdfW * std::abs(aCosThere) / Sqr(aDist);
}

float PdfAtoW(
    const float aPdfA,
    const float aDist,
    const float aCosThere)
{
    return aPdfA * Sqr(aDist) / std::abs(aCosThere);
}


///////////////////////////////////////////////////////////////////////////////
// Others

void SecondsToHumanReadable(const float aSeconds, std::string &oResult)
{
    std::ostringstream outStream;

    uint32_t tmp = (uint32_t)std::roundf(aSeconds);
    if (tmp > 0)
    {
        uint32_t seconds = tmp % 60;
        tmp /= 60;
        uint32_t minutes = tmp % 60;
        tmp /= 60;
        uint32_t hours = tmp % 24;
        tmp /= 24;
        uint32_t days = tmp;

        if (days > 0)
            outStream << days << " d "; // " day" << ((days > 1) ? "s " : " ");
        if (hours > 0)
            outStream << hours << " h "; // " hour" << ((hours > 1) ? "s " : " ");
        if (minutes > 0)
            outStream << minutes << " m "; // " minute" << ((minutes > 1) ? "s " : " ");
        if (seconds > 0)
            outStream << seconds << " s "; // " second" << ((seconds > 1) ? "s " : " ");

        outStream << "(" << std::fixed << std::setprecision(1) << aSeconds << " s)";
    }
    else
        outStream << std::fixed << std::setprecision(2) << aSeconds << " s"; // " seconds";

    oResult = outStream.str();
}

bool GetFileName(const char *aPath, std::string &oResult)
{
    //char path_buffer[_MAX_PATH];
    //char drive[_MAX_DRIVE];
    //char dir[_MAX_DIR];
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];
    errno_t err;

    err = _splitpath_s(aPath, NULL, 0, NULL, 0, fname, _MAX_FNAME, ext, _MAX_EXT);
    if (err != 0)
    {
        //printf("Error splitting the path. Error code %d.\n", err);
        return false;
    }
    //printf("Path extracted with _splitpath_s:\n");
    //printf("  Drive: %s\n", drive);
    //printf("  Dir: %s\n", dir);
    //printf("  Filename: %s\n", fname);
    //printf("  Ext: %s\n", ext);

    oResult  = fname;
    oResult += ext;
    return true;
}

void PrintProgressBarCommon(float aProgress)
{
    PG3_ASSERT_FLOAT_LARGER_THAN_OR_EQUAL_TO(aProgress, 0.0f);

    const uint32_t aBarCount = 30;

    aProgress = Clamp(aProgress, 0.f, 1.f);

    printf("\rProgress:  [");

    for (uint32_t bar = 1; bar <= aBarCount; bar++)
    {
        const float barProgress = (float)bar / aBarCount;
        if (barProgress <= aProgress)
            printf("|");
        else
            printf(".");
    }

    printf("] %.1f%%", std::floor(100.0 * aProgress * 10.0f) / 10.f);
}

void PrintProgressBarIterations(float aProgress, uint32_t aIterations)
{
    PrintProgressBarCommon(aProgress);

    printf(" (%d iter%s)", aIterations, (aIterations != 1) ? "s" : "");

    fflush(stdout);
}

void PrintProgressBarTime(float aProgress, float aTime)
{
    PrintProgressBarCommon(aProgress);

    printf(" (%.1f sec)", aTime);

    fflush(stdout);
}

