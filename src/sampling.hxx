#pragma once

#include "utils.hxx"
#include "debugging.hxx"
#include "math.hxx"

///////////////////////////////////////////////////////////////////////////////
// Sampling routines
///////////////////////////////////////////////////////////////////////////////
namespace Sampling
{
    // Disc sampling
    Vec2f SampleConcentricDisc(
        const Vec2f &aSamples)
    {
        float phi, r;

        float a = 2 * aSamples.x - 1;   /* (a,b) is now on [-1,1]^2 */
        float b = 2 * aSamples.y - 1;

        if (a > -b)      /* region 1 or 2 */
        {
            if (a > b)   /* region 1, also |a| > |b| */
            {
                r = a;
                phi = (Math::kPiF / 4.f) * (b / a);
            }
            else        /* region 2, also |b| > |a| */
            {
                r = b;
                phi = (Math::kPiF / 4.f) * (2.f - (a / b));
            }
        }
        else            /* region 3 or 4 */
        {
            if (a < b)   /* region 3, also |a| >= |b|, a != 0 */
            {
                r = -a;
                phi = (Math::kPiF / 4.f) * (4.f + (b / a));
            }
            else        /* region 4, |b| >= |a|, but a==0 and b==0 could occur. */
            {
                r = -b;

                if (b != 0)
                    phi = (Math::kPiF / 4.f) * (6.f - (a / b));
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
        return Math::kPiInvF;
    }

    // Sample Triangle
    // returns barycentric coordinates
    Vec2f SampleUniformTriangle(const Vec2f &aSamples)
    {
        const float xSqr = std::sqrt(aSamples.x);

        return Vec2f(1.f - xSqr, aSamples.y * xSqr);
    }

    // Cosine lobe hemisphere sampling
    Vec3f SamplePowerCosHemisphereW(
        const Vec2f  &aSamples,
        const float   aPower,
        float        *oPdfW = nullptr)
    {
        const float term1 = 2.f * Math::kPiF * aSamples.x;
        const float term2 = std::pow(aSamples.y, 1.f / (aPower + 1.f));
        const float term3 = std::sqrt(1.f - term2 * term2);

        if (oPdfW)
            *oPdfW = (aPower + 1.f) * std::pow(term2, aPower) * (0.5f * Math::kPiInvF);

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

        return (aPower + 1.f) * std::pow(cosTheta, aPower) * (Math::kPiInvF * 0.5f);
    }

    float PowerCosHemispherePdfW(
        const Vec3f  &aDirectionLocal,
        const float   aPower)
    {
        const float cosTheta = std::max(0.f, aDirectionLocal.z);

        return (aPower + 1.f) * std::pow(cosTheta, aPower) * (Math::kPiInvF * 0.5f);
    }

    // Sample direction in the upper hemisphere with cosine-proportional pdf
    // The returned PDF is with respect to solid angle measure
    Vec3f SampleCosHemisphereW(
        const Vec2f &aSamples,
        float       *oPdfW = nullptr)
    {
        const float term1 = 2.f * Math::kPiF * aSamples.x;
        const float term2 = std::sqrt(1.f - aSamples.y);

        const Vec3f ret(
            std::cos(term1) * term2,
            std::sin(term1) * term2,
            std::sqrt(aSamples.y));

        if (oPdfW)
            *oPdfW = ret.z * Math::kPiInvF;

        PG3_ASSERT_FLOAT_NONNEGATIVE(ret.z);

        return ret;
    }

    float CosHemispherePdfW(
        const Vec3f  &aNormal,
        const Vec3f  &aDirection)
    {
        return std::max(0.f, Dot(aNormal, aDirection)) * Math::kPiInvF;
    }

    float CosHemispherePdfW(
        const Vec3f  &aDirectionLocal)
    {
        return std::max(0.f, aDirectionLocal.z) * Math::kPiInvF;
    }

    Vec3f SampleCosSphereParamPdfW(
        const Vec3f &aSamples,
        const bool   sampleUpperHemisphere,
        const bool   sampleLowerHemisphere,
        float       &oPdfW)
    {
        Vec3f wil = SampleCosHemisphereW(aSamples.GetXY(), &oPdfW);

        if (sampleUpperHemisphere && sampleLowerHemisphere)
        {
            // Chose one hemisphere randomly and reduce the pdf accordingly
            if (aSamples.z < 0.5f)
                wil *= -1.0f;
            oPdfW *= 0.5f;
        }
        else if (sampleLowerHemisphere)
            wil *= -1.0f; // Just switch to lower hemisphere

        return wil;
    }

    float UniformSpherePdfW()
    {
        //return (1.f / (4.f * Math::kPiF));
        return Math::kPiInvF * 0.25f;
    }

    Vec3f SampleUniformSphereW(
        const Vec2f  &aSamples,
        float        *oPdfSA)
    {
        const float phi      = 2.f * Math::kPiF * aSamples.x;
        const float sinTheta = 2.f * std::sqrt(aSamples.y - aSamples.y * aSamples.y);
        const float cosTheta = 1.f - 2.f * aSamples.y;

        PG3_ERROR_CODE_NOT_TESTED("");

        const Vec3f ret = Geom::CreateDirection(sinTheta, cosTheta, std::sin(phi), std::cos(phi));

        if (oPdfSA)
            *oPdfSA = UniformSpherePdfW();

        return ret;
    }


    //////////////////////////////////////////////////////////////////////////
    // Utilities for converting PDF between Area (A) and Solid angle (W)
    // WtoA = PdfW * cosine / distance_squared
    // AtoW = PdfA * distance_squared / cosine
    //////////////////////////////////////////////////////////////////////////

    float PdfWtoA(
        const float aPdfW,
        const float aDist,
        const float aCosThere)
    {
        return aPdfW * std::abs(aCosThere) / Math::Sqr(aDist);
    }

    float PdfAtoW(
        const float aPdfA,
        const float aDist,
        const float aCosThere)
    {
        return aPdfA * Math::Sqr(aDist) / std::abs(aCosThere);
    }
} // namespace Sampling
