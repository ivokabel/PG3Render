#pragma once

#include "unit_testing.hxx"
#include "utils.hxx"
#include "geom.hxx"
#include "rng.hxx"
#include "math.hxx"
#include "types.hxx"

///////////////////////////////////////////////////////////////////////////////
// Microfacet-related code
///////////////////////////////////////////////////////////////////////////////
namespace Microfacet
{
    // Jacobian of the reflection transform
    float ReflectionJacobian(
        const Vec3f &aWol,  // Generated (reflected) direction
        const Vec3f &aMicrofacetNormal)
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aWol);
        PG3_ASSERT_VEC3F_NORMALIZED(aMicrofacetNormal);

        const float cosThetaOM = Dot(aMicrofacetNormal, aWol);
        const float cosThetaOMClamped = std::max(cosThetaOM, 0.000001f);

        const float transfJacobian = 1.0f / (4.0f * cosThetaOMClamped);

        PG3_ASSERT_FLOAT_NONNEGATIVE(transfJacobian);

        return transfJacobian;
    }

    // Jacobian of the refraction transform
    float RefractionJacobian(
        const Vec3f &aWil,          // Fixed incident direction
        const Vec3f &aWol,          // Generated (refracted) direction
        const Vec3f &aMicrofacetNormal,
        const float  aEtaOutIn      // Incoming n / Outgoing n
        )
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aWol);
        PG3_ASSERT_VEC3F_NORMALIZED(aWil);
        PG3_ASSERT_VEC3F_NORMALIZED(aMicrofacetNormal);
        PG3_ASSERT_FLOAT_LARGER_THAN(aEtaOutIn, 0.0f);

        const float cosThetaOM = Dot(aMicrofacetNormal, aWol);
        const float cosThetaIM = Dot(aMicrofacetNormal, aWil);

        const float numerator      = std::abs(cosThetaOM);
        const float denominator    = Math::Sqr(aEtaOutIn * cosThetaIM + cosThetaOM);
        const float transfJacobian = (numerator / std::max(denominator, 0.000001f));

        PG3_ASSERT_FLOAT_NONNEGATIVE(transfJacobian);

        return transfJacobian;
    }

    // Halfway vector (microfacet normal) for reflection
    // Incoming/outgoing directions on different sides of the macro surface are NOT allowed.
    Vec3f HalfwayVectorReflectionLocal(
        const Vec3f& aWil,
        const Vec3f& aWol
        )
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aWil);
        PG3_ASSERT_VEC3F_NORMALIZED(aWol);

        Vec3f halfwayVec = aWil + aWol;
    
        const float length = halfwayVec.Length();
        if (Math::IsTiny(length))
            // This happens if an only if the in/out vectors are on the same line 
            // (in the oposite directions)
            halfwayVec = Vec3f(0.0f, 0.0f, 1.0f); // Geometrical normal
        else
            halfwayVec /= length; // Normalize using the already computed length

        // Must point into the positive half-space
        halfwayVec *= (halfwayVec.z >= 0.0f) ? 1.0f : -1.0f;

        PG3_ASSERT_VEC3F_NORMALIZED(halfwayVec);

        return halfwayVec;
    }

    // Halfway vector (microfacet normal) for refraction
    // Incoming and outgoing direction must be on oposite sides of the macro surface.
    Vec3f HalfwayVectorRefractionLocal(
        const Vec3f &aWil,
        const Vec3f &aWol,
        const float  aEtaInOut  // out n / in n
        )
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aWil);
        PG3_ASSERT_VEC3F_NORMALIZED(aWol);
        PG3_ASSERT_MSG(
            ((aWil.z >= 0) && (aWol.z <= 0)) ||
            ((aWil.z <= 0) && (aWol.z >= 0)),
            "Incoming (z: %.12f) and outgoing (z: %.12f) directions must be on oposite sides of the geometrical surface!",
            aWil.z, aWol.z);
        PG3_ASSERT_FLOAT_LARGER_THAN(aEtaInOut, 0.0f);

        // Compute non-yet-normalized halfway vector. Note that this can yield nonsensical results 
        // for invalid in-out configurations and has to be handled later. This is usually done 
        // in the masking function, which checks whether the in and out directions are on the proper 
        // sides of the microfacet.
        Vec3f halfwayVec = aWil + aWol * aEtaInOut;
    
        const float length = halfwayVec.Length();
        if (Math::IsTiny(length))
            // This happens if and only if the in/out vectors are on the same line 
            // (in the oposite directions) and eta equals 1
            halfwayVec = aWil;
        else
            halfwayVec /= length; // Normalize using the already computed length

        // Must point into the positive half-space
        halfwayVec *= (halfwayVec.z >= 0.0f) ? 1.0f : -1.0f;

        PG3_ASSERT_VEC3F_NORMALIZED(halfwayVec);

        return halfwayVec;
    }

    #ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

    bool _UT_HalfwayVectorRefractionLocal_TestSingleInOutConfiguration(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel,
        const float  thetaIn,
        const float  thetaOut,
        const float  phiOut,
        const float  upperN,
        const float  lowerN)
    {
        const Vec3f dirIn  = Geom::CreateDirection(thetaIn, 0.0f);
        const Vec3f dirOut = Geom::CreateDirection(thetaOut, phiOut);

        const bool isDirInBelow  = dirIn.z  < 0.0f;
        const bool isDirOutBelow = dirOut.z < 0.0f;

        PG3_UT_BEGIN(
            aMaxUtBlockPrintLevel, eutblSubTestLevel2,
            "In-Out: In: (% .2f, % .2f, % .2f), Out: (% .2f, % .2f, % .2f)",
            dirIn.x, dirIn.y, dirIn.z,
            dirOut.x, dirOut.y, dirOut.z);

        // If both directions are on the same side, it is an invalid configuration for refraction,
        // but we want to test those too
        const float etaInOut =
                (isDirOutBelow ? lowerN : upperN)
            / (isDirInBelow  ? lowerN : upperN);

        // Compute half vector
        const Vec3f halfVector =
            HalfwayVectorRefractionLocal(dirIn, dirOut, etaInOut);

        // Halfway vector validity
        const float cosThetaIM = Dot(dirIn, halfVector);
        const float cosThetaOM = Dot(dirOut, halfVector);
        const bool isHalfwayVectorValid =
                // Incident and refracted directions must be on the oposite sides of the microfacet
                ((cosThetaIM  * cosThetaOM) <= 0.0f)
                // Up directions cannot face the microfacet from below and vice versa
            && ((dirIn.z  * cosThetaIM) >= -0.0f)
            && ((dirOut.z * cosThetaOM) >= -0.0f);

        // Tests
        if (isDirInBelow == isDirOutBelow)
        {
            if (isHalfwayVectorValid)
            {
                PG3_UT_FAILED(
                    aMaxUtBlockPrintLevel, eutblSubTestLevel2,
                    "In-Out: In: (% .2f, % .2f, % .2f), Out: (% .2f, % .2f, % .2f)",
                    "In and out directions are on the same side of the macro surface, but halfway vector is valid!",
                    dirIn.x, dirIn.y, dirIn.z,
                    dirOut.x, dirOut.y, dirOut.z);
                return false;
            }
        }
        else if (isDirInBelow != isDirOutBelow)
        {
            if (isHalfwayVectorValid)
            {
                // Test behaviour using the Geom::Refract() function
                Vec3f dirOutComputed;
                bool isAboveMicrofacet;
                const auto eta = lowerN / upperN;
                Geom::Refract(dirOutComputed, isAboveMicrofacet, dirIn, halfVector, eta);

                // Refraction validity
                const float cosThetaI = Dot(dirIn, halfVector);
                const float etaInternal = (cosThetaI > 0.0f) ? (1.0f / eta) : eta;
                const float cosThetaTSqr = 1 - (1 - cosThetaI * cosThetaI) * (etaInternal * etaInternal);
                const float refractionValidityCoef = cosThetaTSqr;

                // Evaluate
                if (refractionValidityCoef >= 0.0f)
                {
                    // Check out directions
                    const float outDirsDistance = (dirOutComputed - dirOut).Length();
                    const bool outDirsEqual = outDirsDistance < 0.0005f;

                    if (!outDirsEqual)
                    {
                        PG3_UT_FAILED(
                            aMaxUtBlockPrintLevel, eutblSubTestLevel2,
                            "In-Out: In: (% .2f, % .2f, % .2f), Out: (% .2f, % .2f, % .2f)",
                            "Both halfway vector and refraction are valid, but out directions are not equal!",
                            dirIn.x, dirIn.y, dirIn.z,
                            dirOut.x, dirOut.y, dirOut.z);
                        return false;
                    }
                }
                else if (refractionValidityCoef < -0.0001f)
                {
                    PG3_UT_FAILED(
                        aMaxUtBlockPrintLevel, eutblSubTestLevel2,
                        "In-Out: In: (% .2f, % .2f, % .2f), Out: (% .2f, % .2f, % .2f)",
                        "Halfway vector is valid, but refraction is not!",
                        dirIn.x, dirIn.y, dirIn.z,
                        dirOut.x, dirOut.y, dirOut.z);
                    return false;
                }
            }
        }
        else
        {
            PG3_UT_FATAL_ERROR(
                aMaxUtBlockPrintLevel, eutblSubTestLevel2,
                "In-Out: In: (% .2f, % .2f, % .2f), Out: (% .2f, % .2f, % .2f)",
                "Unexpected case!",
                dirIn.x, dirIn.y, dirIn.z,
                dirOut.x, dirOut.y, dirOut.z);
            return false;
        }

        PG3_UT_PASSED(
            aMaxUtBlockPrintLevel, eutblSubTestLevel2,
            "In-Out: In: (% .2f, % .2f, % .2f), Out: (% .2f, % .2f, % .2f)",
            dirIn.x, dirIn.y, dirIn.z,
            dirOut.x, dirOut.y, dirOut.z);

        return true;
    }

    bool _UT_HalfwayVectorRefractionLocal_TestSingleInHalfvectorConfiguration(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel,
        const float  thetaIn,
        const float  thetaHalfwayVector,
        const float  phiHalfwayVector,
        const float  upperN,
        const float  lowerN)
    {
        const Vec3f dirIn       = Geom::CreateDirection(thetaIn, 0.0f);
        const Vec3f halfVector  = Geom::CreateDirection(thetaHalfwayVector, phiHalfwayVector);

        if (halfVector.z < 0.0f)
            return true;

        PG3_UT_BEGIN(
            aMaxUtBlockPrintLevel, eutblSubTestLevel2,
            "In-HalfwayVector: In: (% .2f, % .2f, % .2f), HalfwayVector: (% .2f, % .2f, % .2f)",
            dirIn.x, dirIn.y, dirIn.z,
            halfVector.x, halfVector.y, halfVector.z);

        // Refract
        Vec3f dirOut;
        bool isAboveMicrofacet;
        const auto etaAbs = lowerN / upperN;
        Geom::Refract(dirOut, isAboveMicrofacet, dirIn, halfVector, etaAbs);

        // Refraction validity
        const float cosThetaI = Dot(dirIn, halfVector);
        const float etaInternal = (cosThetaI > 0.0f) ? (1.0f / etaAbs) : etaAbs;
        const float cosThetaTSqr = 1 - (1 - cosThetaI * cosThetaI) * (etaInternal * etaInternal);
        const float refractionValidityCoef = cosThetaTSqr;

        // Tests
        const float cosThetaIM = Dot(dirIn,  halfVector);
        const float cosThetaOM = Dot(dirOut, halfVector);

        if (   (refractionValidityCoef >= 0.0f)
            && ((dirIn.z  * cosThetaIM) >= 0.0f)
            && ((dirOut.z * cosThetaOM) >= 0.0f))
        {
            const bool isDirInBelow = dirIn.z  < 0.0f;
            const bool isDirOutBelow = dirOut.z < 0.0f;

            if (isDirInBelow == isDirOutBelow)
            {
                PG3_UT_FAILED(
                    aMaxUtBlockPrintLevel, eutblSubTestLevel2,
                    "In-HalfwayVector: In: (% .2f, % .2f, % .2f), HalfwayVector: (% .2f, % .2f, % .2f)",
                    "In and out directions are on the same side of the macro surface!",
                    dirIn.x, dirIn.y, dirIn.z,
                    halfVector.x, halfVector.y, halfVector.z);
                return false;
            }

            // Compute half vector
            const float etaInOut =
                    (isDirOutBelow ? lowerN : upperN)
                / (isDirInBelow  ? lowerN : upperN);
            const Vec3f halfVectorComputed =
                HalfwayVectorRefractionLocal(dirIn, dirOut, etaInOut);

            // Halfway vector validity
            const float cosThetaIMComp = Dot(dirIn,  halfVectorComputed);
            const float cosThetaOMComp = Dot(dirOut, halfVectorComputed);
            const bool isHalfwayVectorValid =
                    // Incident and refracted directions must be on the oposite sides of the microfacet
                    ((cosThetaIMComp  * cosThetaOMComp) <= 0.0f)
                    // Up directions cannot face the microfacet from below and vice versa
                && ((dirIn.z  * cosThetaIMComp) >= -0.0f)
                && ((dirOut.z * cosThetaOMComp) >= -0.0f);
            const float subCoef1 = -1.0f * cosThetaIMComp * cosThetaOMComp;
            const float subCoef2 = dirIn.z  * cosThetaIMComp;
            const float subCoef3 = dirOut.z * cosThetaOMComp;
            const float halwayVectCompValidityCoef = Math::Min3(subCoef1, subCoef2, subCoef3);

            // Sanity test
            if (isHalfwayVectorValid != (halwayVectCompValidityCoef >= 0.0f))
            {
                PG3_UT_FATAL_ERROR(
                    aMaxUtBlockPrintLevel, eutblSubTestLevel2,
                    "In-HalfwayVector: In: (% .2f, % .2f, % .2f), HalfwayVector: (% .2f, % .2f, % .2f)",
                    "Refraction valitidy sanity test failed!",
                    dirIn.x, dirIn.y, dirIn.z,
                    halfVector.x, halfVector.y, halfVector.z);
                return false;
            }

            // Evaluate
            if (halwayVectCompValidityCoef >= 0.0f)
            {
                // Check out directions
                const float halfVectorsDistance = (halfVectorComputed - halfVector).Length();
                const bool halfVectorsEqual = halfVectorsDistance < 0.0001f;

                if (!halfVectorsEqual)
                {
                    PG3_UT_FAILED(
                        aMaxUtBlockPrintLevel, eutblSubTestLevel2,
                        "In-HalfwayVector: In: (% .2f, % .2f, % .2f), HalfwayVector: (% .2f, % .2f, % .2f)",
                        "Both refraction and halfway vector are valid, but halfway vectors are not equal!",
                        dirIn.x, dirIn.y, dirIn.z,
                        halfVector.x, halfVector.y, halfVector.z);
                    return false;
                }
            }
            else if (halwayVectCompValidityCoef < -0.0001f)
            {
                PG3_UT_FAILED(
                    aMaxUtBlockPrintLevel, eutblSubTestLevel2,
                    "In-HalfwayVector: In: (% .2f, % .2f, % .2f), HalfwayVector: (% .2f, % .2f, % .2f)",
                    "Refraction is valid, but halfway vector is not!",
                    dirIn.x, dirIn.y, dirIn.z,
                    halfVector.x, halfVector.y, halfVector.z);
                return false;
            }
        }

        PG3_UT_PASSED(
            aMaxUtBlockPrintLevel, eutblSubTestLevel2,
            "In-HalfwayVector: In: (% .2f, % .2f, % .2f), HalfwayVector: (% .2f, % .2f, % .2f)",
            dirIn.x, dirIn.y, dirIn.z,
            halfVector.x, halfVector.y, halfVector.z);

        return true;
    }

    bool _UT_HalfwayVectorRefractionLocal_TestInterface(
        UnitTestBlockLevel  aMaxUtBlockPrintLevel,
        const float         aUpperN, // above macro surface
        const float         aLowerN  // below macro surface
        )
    {
        // Deterministic directions generation

        const float thetaInStepCount    = 32;
        const float thetaInStart        = 0.0f * Math::kPiF;
        const float thetaInEnd          = 2.0f * Math::kPiF;

        const float thetaOutStepCount   = 32;
        const float thetaOutStart       = 0.0f * Math::kPiF;
        const float thetaOutEnd         = 2.0f * Math::kPiF;

        const float phiOutStepCount     = 16;
        const float phiOutStart         = 0.0f * Math::kPiF;
        const float phiOutEnd           = 2.0f * Math::kPiF;

        PG3_UT_BEGIN(
            aMaxUtBlockPrintLevel, eutblSubTestLevel1,
            "Air(%.2f)/glass(%.2f) interface, deterministics directions (%.0fx%.0fx%.0f)",
            aUpperN, aLowerN, thetaInStepCount, thetaOutStepCount, phiOutStepCount);

        const float thetaInStep = (thetaInEnd - thetaInStart) / (thetaInStepCount - 1);
        for (float thetaIn = thetaInStart; thetaIn <= (thetaInEnd + 0.001f); thetaIn += thetaInStep)
        {
            const float thetaOutStep = (thetaOutEnd - thetaOutStart) / (thetaOutStepCount - 1);
            for (float thetaOut = thetaOutStart; thetaOut <= (thetaOutEnd + 0.001f); thetaOut += thetaOutStep)
            {
                const float phiOutStep = (phiOutEnd - phiOutStart) / (phiOutStepCount - 1);
                for (float phiOut = phiOutStart; phiOut <= (phiOutEnd + 0.001f); phiOut += phiOutStep)
                {
                    bool success;

                    success =
                        _UT_HalfwayVectorRefractionLocal_TestSingleInOutConfiguration(
                            aMaxUtBlockPrintLevel,
                            thetaIn, thetaOut, phiOut,
                            aUpperN, aLowerN);
                    if (!success)
                        return false;

                    success =
                        _UT_HalfwayVectorRefractionLocal_TestSingleInHalfvectorConfiguration(
                            aMaxUtBlockPrintLevel,
                            thetaIn, thetaOut, phiOut,
                            aUpperN, aLowerN);
                    if (!success)
                        return false;
                }
            }
        }

        PG3_UT_PASSED(
            aMaxUtBlockPrintLevel, eutblSubTestLevel1,
            "Air(%.2f)/glass(%.2f) interface, deterministics directions (%.0fx%.0fx%.0f)",
            aUpperN, aLowerN, thetaInStepCount, thetaOutStepCount, phiOutStepCount);

        // Monte Carlo testing

        const uint32_t randomSamplesCount = 32 * 32 * 64;
        Rng rng(1998);

        PG3_UT_BEGIN(
            aMaxUtBlockPrintLevel, eutblSubTestLevel1,
            "Air(%.2f)/glass(%.2f) interface, random directions (%d)",
            aUpperN, aLowerN, randomSamplesCount);

        for (uint32_t sample = 0; sample < randomSamplesCount; sample++)
        {
            // Sample spheres uniformly
            const Vec3f samples = rng.GetVec3f();
            const float thetaIn     = std::acos(1.f - 2.f * samples.x);
            const float thetaOut    = std::acos(1.f - 2.f * samples.y);
            const float phiOut      = samples.z * 2.0f * Math::kPiF;

            bool success;

            success = _UT_HalfwayVectorRefractionLocal_TestSingleInOutConfiguration(
                aMaxUtBlockPrintLevel,
                thetaIn, thetaOut, phiOut,
                aUpperN, aLowerN);
            if (!success)
                return false;

            success = _UT_HalfwayVectorRefractionLocal_TestSingleInHalfvectorConfiguration(
                aMaxUtBlockPrintLevel,
                thetaIn, thetaOut, phiOut,
                aUpperN, aLowerN);
            if (!success)
                return false;
        }

        PG3_UT_PASSED(
            aMaxUtBlockPrintLevel, eutblSubTestLevel1,
            "Air(%.2f)/glass(%.2f) interface, random directions (%d)",
            aUpperN, aLowerN, randomSamplesCount);

        return true;
    }

    bool _UT_HalfwayVectorRefractionLocal(
        UnitTestBlockLevel   aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest, "Microfacet::HalfwayVectorRefractionLocal()");

        if (!_UT_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.00f, 1.51f))
            return false;
        if (!_UT_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.51f, 1.00f))
            return false;

        if (!_UT_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.10f, 1.00f))
            return false;
        if (!_UT_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.00f, 1.10f))
            return false;

        if (!_UT_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.51f, 1.55f))
            return false;
        if (!_UT_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.55f, 1.51f))
            return false;

        PG3_UT_PASSED(
            aMaxUtBlockPrintLevel, eutblWholeTest,
            "Microfacet::HalfwayVectorRefractionLocal()");

        return true;
    }

    #endif

    float DistributionGgx(
        const Vec3f &aMicrofacetNormal,
        const float  aRoughnessAlpha)
    {
        PG3_ASSERT_FLOAT_NONNEGATIVE(aRoughnessAlpha);

        if (aMicrofacetNormal.z <= 0.f)
            return 0.0f;

        // TODO: Optimize using the simplified version (only containing cosine, not tangent)

        const float roughnessAlphaSqr = aRoughnessAlpha * aRoughnessAlpha;
        const float cosThetaSqr = aMicrofacetNormal.z * aMicrofacetNormal.z;
        const float tanThetaSqr = Geom::TanThetaSqr(aMicrofacetNormal);
        const float temp1 = roughnessAlphaSqr + tanThetaSqr;
        const float temp2 = cosThetaSqr * temp1;

        const float result = roughnessAlphaSqr / (Math::kPiF * temp2 * temp2);

        PG3_ASSERT_FLOAT_NONNEGATIVE(result);

        return result;
    }

    float SmithMaskingFunctionGgx(
        const Vec3f &aDir,  // the direction to compute masking for (either incoming or outgoing)
        const Vec3f &aMicrofacetNormal,
        const float  aRoughnessAlpha)
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aDir);
        PG3_ASSERT_VEC3F_NORMALIZED(aMicrofacetNormal);
        PG3_ASSERT_FLOAT_NONNEGATIVE(aRoughnessAlpha);

        if (aMicrofacetNormal.z <= 0)
            return 0.0f;

        const float cosThetaVM = Dot(aDir, aMicrofacetNormal);
        if ((aDir.z * cosThetaVM) < 0.0f)
            return 0.0f; // up direction is below microfacet or vice versa

        const float roughnessAlphaSqr = aRoughnessAlpha * aRoughnessAlpha;
        const float tanThetaSqr = Geom::TanThetaSqr(aDir);
        const float root = std::sqrt(1.0f + roughnessAlphaSqr * tanThetaSqr);

        const float result = 2.0f / (1.0f + root);

        PG3_ASSERT_FLOAT_IN_RANGE(result, 0.0f, 1.0f);

        return result;
    }

    // GGX sampling based directly on the distribution of microfacets.
    // Generates a lot of back-faced microfacets.
    // Returns whether the input and output directions are above microfacet.
    bool SampleGgxAllNormals(
        const Vec3f &aWol,
        const float  aRoughnessAlpha,
        const Vec2f &aUniSample,
                Vec3f &oReflectDir)
    {
        // Sample microfacet direction (D(omega_m) * cos(theta_m))
        const float tmp1 =
                (aRoughnessAlpha * std::sqrt(aUniSample.x))
            / std::sqrt(1.0f - aUniSample.x);
        const float thetaM  = std::atan(tmp1);
        const float phiM    = 2 * Math::kPiF * aUniSample.y;

        const float cosThetaM   = std::cos(thetaM);
        const float sinThetaM   = std::sin(thetaM);
        const float cosPhiM     = std::cos(phiM);
        const float sinPhiM     = std::sin(phiM);
        const Vec3f microfacetDir(
            sinThetaM * sinPhiM,
            sinThetaM * cosPhiM,
            cosThetaM);

        // Reflect from the microfacet
        bool isAboveMicrofacet;
        Geom::Reflect(oReflectDir, isAboveMicrofacet, aWol, microfacetDir);

        return isAboveMicrofacet;
    }

    // Sampling density of GGX sampling based directly on the distribution of microfacets
    float GgxSamplingPdfAllNormals(
        const Vec3f &aWol,
        const Vec3f &aWil,
        const float  aRoughnessAlpha)
    {
        // Distribution value
        const Vec3f halfwayVec          = Microfacet::HalfwayVectorReflectionLocal(aWil, aWol);
        const float microFacetDistrVal  = Microfacet::DistributionGgx(halfwayVec, aRoughnessAlpha);
        const float microfacetPdf       = microFacetDistrVal * halfwayVec.z;

        const float transfJacobian = Microfacet::ReflectionJacobian(aWil, halfwayVec);

        return microfacetPdf * transfJacobian;
    }

    // Sample GGX P^{22}_{\omega_o}(slope, 1, 1) from [Heitz2014] for given incident direction theta
    void SampleGgxP11(
                Vec2f &aSlope,
        const float  aThetaI, 
        const Vec2f &aUniSample)
    {
        if (aThetaI < 0.0001f)
        {
            // Normal incidence - avoid division by zero later
            const float sampleXClamped  = std::min(aUniSample.x, 0.9999f);
            const float radius          = Math::SafeSqrt(sampleXClamped / (1 - sampleXClamped));
            const float phi             = 2 * Math::kPiF * aUniSample.y;
            const float sinPhi          = std::sin(phi);
            const float cosPhi          = std::cos(phi);
            aSlope = Vec2f(radius * cosPhi, radius * sinPhi);
        }
        else
        {
            const float tanThetaI    = std::tan(aThetaI);
            const float tanThetaIInv = 1.0f / tanThetaI;
            const float G1 = 2.0f / (1.0f + Math::SafeSqrt(1.0f + 1.0f / (tanThetaIInv * tanThetaIInv)));

            // Sample x dimension (marginalized PDF - can be sampled directly via CDF^-1)
            float A = 2.0f * aUniSample.x / G1 - 1.0f; // TODO: dividing by G1, which can be zero?!?
            if (std::abs(A) == 1.0f)
                A -= Math::SignNum(A) * 1e-4f; // avoid division by zero later
            const float B = tanThetaI;
            const float tmpFract = 1.0f / (A * A - 1.0f);
            const float D = Math::SafeSqrt(B * B * tmpFract * tmpFract - (A * A - B * B) * tmpFract);
            const float slopeX1 = B * tmpFract - D;
            const float slopeX2 = B * tmpFract + D;
            aSlope.x = (A < 0.0f || slopeX2 > (1.0f / tanThetaI)) ? slopeX1 : slopeX2;

            PG3_ASSERT_FLOAT_VALID(aSlope.x);

            // Sample y dimension
            // Using conditional PDF; however, CDF is not directly invertible, so we use rational fit of CDF^-1.
            // We sample just one half-space - PDF is symmetrical in y dimension.
            // We use improved fit from Mitsuba renderer rather than the original fit from the paper.
            float ySign;
            float yHalfSample;
            if (aUniSample.y > 0.5f) // pick one positive/negative interval
            {
                ySign       = 1.0f;
                yHalfSample = 2.0f * (aUniSample.y - 0.5f);
            }
            else
            {
                ySign       = -1.0f;
                yHalfSample = 2.0f * (0.5f - aUniSample.y);
            }
            const float z =
                    (yHalfSample * (yHalfSample * (yHalfSample *
                        -(float)0.365728915865723 + (float)0.790235037209296) - 
                        (float)0.424965825137544) + (float)0.000152998850436920)
                /
                    (yHalfSample * (yHalfSample * (yHalfSample * (yHalfSample *
                        (float)0.169507819808272 - (float)0.397203533833404) - 
                        (float)0.232500544458471) + (float)1) - (float)0.539825872510702);
            aSlope.y = ySign * z * std::sqrt(1.0f + aSlope.x * aSlope.x);

            PG3_ASSERT_FLOAT_VALID(aSlope.y);
        }

        PG3_ASSERT_VEC2F_VALID(aSlope);
    }

    // GGX sampling based on "Importance Sampling Microfacet-Based BSDFs using the Distribution of 
    // Visible Normals" by Eric Heitz and Eugene D'Eon [Heitz2014]. It generates only the front-facing
    // microfacets resulting in less wasted samples with sample weights bound to [0, 1].
    //
    // FIXME: There is a problem with memory alignment because it crashes on SSE movap instruction
    // accessing address not aligned to 16 bytes when compiling for SSE2 extension. 
    // This is, although very unlikely, possibly a compiler bug. Workaround: commpiling for AVX
    Vec3f SampleGgxVisibleNormals(
        const Vec3f &aWol,
        const float  aRoughnessAlpha,
        const Vec2f &aUniSample)
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aWol);

        // Stretch Wol to canonical, unit roughness space
        const Vec3f wolStretch =
            Normalize(Vec3f(
                aWol.x * aRoughnessAlpha,
                aWol.y * aRoughnessAlpha,
                std::max(aWol.z, 0.0f)));

        float thetaWolStretch = 0.0f;
        float phiWolStretch   = 0.0f;
        if (wolStretch.z < 0.999f)
        {
            thetaWolStretch = std::acos(wolStretch.z);
            phiWolStretch   = std::atan2(wolStretch.y, wolStretch.x);
        }
        const float sinPhi = std::sin(phiWolStretch);
        const float cosPhi = std::cos(phiWolStretch);

        // Sample visible slopes for unit isotropic roughness, and given incident direction theta
        Vec2f slopeStretch;
        SampleGgxP11(slopeStretch, thetaWolStretch, aUniSample);

        // Rotate
        slopeStretch = Vec2f(
            slopeStretch.x * cosPhi - slopeStretch.y * sinPhi,
            slopeStretch.x * sinPhi + slopeStretch.y * cosPhi);

        // Unstretch back to non-unit roughness
        Vec2f slope(
            slopeStretch.x * aRoughnessAlpha,
            slopeStretch.y * aRoughnessAlpha);

        PG3_ASSERT_VEC2F_VALID(slope);

        // Compute normal
        const float slopeLengthInv = 1.0f / Vec3f(slope.x, slope.y, 1.0f).Length();
        Vec3f microfacetDir(
            -slope.x * slopeLengthInv,
            -slope.y * slopeLengthInv,
            slopeLengthInv);

        PG3_ASSERT_FLOAT_VALID(slopeLengthInv);
        PG3_ASSERT_VEC3F_VALID(microfacetDir);

        return microfacetDir;
    }

    // Sampling density of GGX sampling based on [Heitz2014]
    float GgxSamplingPdfVisibleNormals(
        const Vec3f &aWol,
        const Vec3f &aHalfwayVec,
        const float  distrVal,
        const float  aRoughnessAlpha)
    {
        // TODO: Reuse computations when (if) combined with sampling routine??

        PG3_ASSERT_VEC3F_NORMALIZED(aWol);
        PG3_ASSERT_VEC3F_NORMALIZED(aHalfwayVec);
        PG3_ASSERT_FLOAT_NONNEGATIVE(aWol.z);

        if (aHalfwayVec.z <= 0.f)
            return 0.0f;

        const float masking     = Microfacet::SmithMaskingFunctionGgx(aWol, aHalfwayVec, aRoughnessAlpha);
        const float cosThetaOM  = Dot(aHalfwayVec, aWol);
        const float cosThetaO   = std::max(aWol.z, 0.00001f);

        const float microfacetPdf = (masking * std::abs(cosThetaOM) * distrVal) / cosThetaO;

        PG3_ASSERT_FLOAT_NONNEGATIVE(microfacetPdf);

        return microfacetPdf;
    }
} // namespace Microfacet

