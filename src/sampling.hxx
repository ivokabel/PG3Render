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

    // Sample triangle
    // Returns barycentric coordinates
    Vec2f SampleUniformTriangle(const Vec2f &aSamples)
    {
        const float xSqr = std::sqrt(aSamples.x);

        return Vec2f(1.f - xSqr, aSamples.y * xSqr);
    }

    Vec3f NormalizedOrthoComp(const Vec3f &aVector, const Vec3f &aBasis)
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aBasis);

        const auto projectionOnBasis   = Dot(aVector, aBasis) * aBasis;
        const auto orthogonalComponent = aVector - projectionOnBasis;
        return Normalize(orthogonalComponent);
    }

    // Sample spherical triangle
    // Implementation of the James Arvo's 1995 paper: Stratified Sampling of Spherical Triangles
    Vec3f SampleUniformSphericalTriangle(
        const Vec3f &aVertexA,
        const Vec3f &aVertexB,
        const Vec3f &aVertexC,
        const float  aCosC,
        const float  aAlpha,
        const float  aTriangleArea,
        const Vec2f &aSamples)
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aVertexA);
        PG3_ASSERT_VEC3F_NORMALIZED(aVertexB);
        PG3_ASSERT_VEC3F_NORMALIZED(aVertexC);
        PG3_ASSERT_FLOAT_VALID(aCosC);
        PG3_ASSERT_FLOAT_NONNEGATIVE(aAlpha);
        PG3_ASSERT_FLOAT_NONNEGATIVE(aTriangleArea);
        PG3_ASSERT_VEC2F_NONNEGATIVE(aSamples);

        const float cosAlpha = std::cos(aAlpha);
        const float sinAlpha = std::sin(aAlpha);

        // Compute surface area of the sub-triangle
        const float areaSub = aSamples.x * aTriangleArea;

        // Compute sin & cos of phi
        const float s = std::sin(areaSub - aAlpha);
        const float t = std::cos(areaSub - aAlpha);

        // Pair (u, v)
        const float u = t - cosAlpha;
        const float v = s + sinAlpha * aCosC;

        // q=cos(beta^)
        // FIXME: Potential division by zero!
        const float q = 
              ((v * t - u * s) * cosAlpha - v)
            / ((v * s + u * t) * sinAlpha);

        // C^, the new vertex of the sub-triangle
        const Vec3f vertexCSub =
              q * aVertexA
            + Math::SafeSqrt(1.f - q * q) * NormalizedOrthoComp(aVertexC, aVertexA);

        // Compute cos(theta)
        const float z = 1.f - aSamples.y * (1.f - Dot(vertexCSub, aVertexB));

        // Construct new point on the sphere
        const Vec3f pointP =
              z * aVertexB
            + Math::SafeSqrt(1.f - z * z) * NormalizedOrthoComp(vertexCSub, aVertexB);

        PG3_ASSERT_VEC3F_NORMALIZED(pointP);

        return pointP;
    }

    Vec3f SampleUniformSphericalTriangle(
        const Vec3f &aVertexA,
        const Vec3f &aVertexB,
        const Vec3f &aVertexC,
        const Vec2f &aSamples)
    {
        float alpha, beta, gamma;
        Geom::ComputeSphericalTriangleAngles(alpha, beta, gamma, aVertexA, aVertexB, aVertexC);

        const float triangleArea = Geom::SphericalTriangleArea(alpha, beta, gamma);

        const float cosC = Dot(aVertexA, aVertexB);

        return SampleUniformSphericalTriangle(
            aVertexA, aVertexB, aVertexC,
            cosC, alpha, triangleArea,
            aSamples);
    }

#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

    static bool _UnitTest_SampleUniformSphericalTriangle_SingleOctant(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const Vec3f                 &aSigns)
    {
        Vec3f signsNormalized;
        for (uint32_t i = 0; i < 3; i++)
            signsNormalized.Get(i) = Math::SignNum(aSigns.Get(i));

        // Name
        std::ostringstream testNameStream;
        testNameStream << "Octant ";
        testNameStream << ((signsNormalized.x >= 0) ? "+" : "-");
        testNameStream << "X";
        testNameStream << ((signsNormalized.y >= 0) ? "+" : "-");
        testNameStream << "Y";
        testNameStream << ((signsNormalized.z >= 0) ? "+" : "-");
        testNameStream << "Z";
        auto testNameStr = testNameStream.str();
        auto testName = testNameStr.c_str();

        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", testName);

        // Generate triangle coords
        auto vertexA = Vec3f(1.f, 0.f, 0.f) * signsNormalized.Get(0);
        auto vertexB = Vec3f(0.f, 1.f, 0.f) * signsNormalized.Get(1);
        auto vertexC = Vec3f(0.f, 0.f, 1.f) * signsNormalized.Get(2);

        // Tests
        for (float u = 0; u <= 1.0001f; u += 0.5f)
            for (float v = 0; v <= 1.0001f; v += 0.5f)
            {
                const Vec3f triangleSample =
                    SampleUniformSphericalTriangle(vertexA, vertexB, vertexC, Vec2f(u, v));

                // Test: In the right octant
                for (uint32_t i = 0; i < 3; i++)
                {
                    if ((signsNormalized.Get(i) * triangleSample.Get(i)) < -0.0001f)
                    {
                        PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s",
                            "The resulting sample is outside the requested octant", testName);
                        return false;
                    }
                }

                // Test: On the sphere
                if (!Math::EqualDelta(triangleSample.LenSqr(), 1.0f, 0.001f))
                {
                    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s",
                        "The resulting sample doesn't lie on the unit sphere", testName);
                    return false;
                }
            }

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", testName);
        return true;
    }

    static bool _UnitTest_SampleUniformSphericalTriangle(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::SampleUniformSphericalTriangle");

        for (float x = -1; x <= 1.0001; x += 2)
            for (float y = -1; y <= 1.0001; y += 2)
                for (float z = -1; z <= 1.0001; z += 2)
                {
                    if (!_UnitTest_SampleUniformSphericalTriangle_SingleOctant(
                            aMaxUtBlockPrintLevel, Vec3f(x, y, z)))
                        return false;
                }

        // TODO: Small, empty triangle?

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::SampleUniformSphericalTriangle");
        return true;
    }

#endif

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
