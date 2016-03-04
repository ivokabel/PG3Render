#pragma once

#include "unittesting.hxx"
#include "rng.hxx"
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

//////////////////////////////////////////////////////////////////////////
// Fresnel-related routines
//////////////////////////////////////////////////////////////////////////

float FresnelDielectric(
    float aCosThetaI,
    float aEtaAbs)         // internal IOR / external IOR
{
    PG3_ASSERT_FLOAT_LARGER_THAN(aEtaAbs, 0.0f);

    if (aEtaAbs < 0)
        return 1.f;

    float workingEta;
    if (aCosThetaI < 0.f)
    {
        aCosThetaI = -aCosThetaI;
        workingEta = aEtaAbs;
    }
    else
        workingEta = 1.f / aEtaAbs;

    const float sinThetaTSqr    = Sqr(workingEta) * (1.f - Sqr(aCosThetaI));
    const float cosThetaT       = SafeSqrt(1.f - sinThetaTSqr);

    if (sinThetaTSqr > 1.0f)
        return 1.0f; // TIR

    // Perpendicular (senkrecht) polarization
    const float term2                   = workingEta * aCosThetaI;
    const float reflPerpendicularSqrt   = (term2 - cosThetaT) / (term2 + cosThetaT);
    const float reflPerpendicular       = Sqr(reflPerpendicularSqrt);

    // Parallel polarization
    const float term1               = workingEta * cosThetaT;
    const float reflParallelSqrt    = (aCosThetaI - term1) / (aCosThetaI + term1);
    const float reflParallel        = Sqr(reflParallelSqrt);

    const float reflectance = 0.5f * (reflParallel + reflPerpendicular);

    PG3_ASSERT_FLOAT_IN_RANGE(reflParallel, 0.0f, 1.0f);
    PG3_ASSERT_FLOAT_IN_RANGE(reflPerpendicular, 0.0f, 1.0f);
    PG3_ASSERT_FLOAT_IN_RANGE(reflectance, 0.0f, 1.0f);
    PG3_ASSERT_FLOAT_LARGER_THAN_OR_EQUAL_TO(reflPerpendicular + 0.001f, reflParallel);

    return reflectance;
}

float FresnelConductor(
    float aCosThetaI,
    float aEtaAbs,         // internal IOR / external IOR
    float aAbsorbance
    )
{
    PG3_ASSERT_FLOAT_LARGER_THAN(aEtaAbs, 0.0f);
    PG3_ASSERT_FLOAT_LARGER_THAN(aAbsorbance, 0.0f);

    if (aCosThetaI < -0.00001f)
        // Hitting the surface from the inside - no reflectance. This can be caused for example by 
        // a numerical error in object intersection code.
        return 0.0f;

    aCosThetaI = Clamp(aCosThetaI, 0.0f, 1.0f);

//#define USE_ART_FRESNEL
#define USE_MITSUBA_FRESNEL

#ifdef USE_ART_FRESNEL

    const float cosThetaSqr = Sqr(aCosThetaI);
    const float sinThetaSqr = std::max(1.0f - cosThetaSqr, 0.0f);
    const float sinTheta    = std::sqrt(sinThetaSqr);

    const float iorSqr    = Sqr(aEtaAbs);
    const float absorbSqr = Sqr(aAbsorbance);

    const float tmp1 = iorSqr - absorbSqr - sinThetaSqr;
    const float tmp2 = std::sqrt(Sqr(tmp1) + 4 * iorSqr * absorbSqr);

    const float aSqr     = (tmp2 + tmp1) * 0.5f;
    const float bSqr     = (tmp2 - tmp1) * 0.5f;
    const float aSqrMul2 = 2 * std::sqrt(aSqr);

    float tanTheta, tanThetaSqr;

    if (!IsTiny(aCosThetaI))
    {
        tanTheta    = sinTheta / aCosThetaI;
        tanThetaSqr = Sqr(tanTheta);
    }
    else
    {
        tanTheta    = HUGE_F;
        tanThetaSqr = HUGE_F;
    }

    const float reflPerpendicular =
          (aSqr + bSqr - aSqrMul2 * aCosThetaI + cosThetaSqr)
        / (aSqr + bSqr + aSqrMul2 * aCosThetaI + cosThetaSqr);

    const float reflParallel =
          reflPerpendicular
        * (  (aSqr + bSqr - aSqrMul2 * sinTheta * tanTheta + sinThetaSqr * tanThetaSqr)
           / (aSqr + bSqr + aSqrMul2 * sinTheta * tanTheta + sinThetaSqr * tanThetaSqr));

    const float reflectance = 0.5f * (reflParallel + reflPerpendicular);

    PG3_ASSERT_FLOAT_IN_RANGE(reflPerpendicular, 0.0f, 1.0f);
    PG3_ASSERT_FLOAT_IN_RANGE(reflParallel,      0.0f, 1.0f);
    PG3_ASSERT_FLOAT_IN_RANGE(reflectance,       0.0f, 1.0f);
    PG3_ASSERT_FLOAT_LARGER_THAN_OR_EQUAL_TO(reflPerpendicular, reflParallel);

#elif defined USE_MITSUBA_FRESNEL

    // Taken and modified from Mitsuba renderer, which states:
    //      "Modified from "Optics" by K.D. Moeller, University Science Books, 1988"

    const float cosThetaI2 = aCosThetaI * aCosThetaI;
    const float sinThetaI2 = 1 - cosThetaI2;
    const float sinThetaI4 = sinThetaI2 * sinThetaI2;

    const float aEta2        = aEtaAbs * aEtaAbs;
    const float aAbsorbance2 = aAbsorbance * aAbsorbance;

    const float temp1 = aEta2 - aAbsorbance2 - sinThetaI2;
    const float a2pb2 = SafeSqrt(temp1 * temp1 + 4 * aAbsorbance2 * aEta2);
    const float a = SafeSqrt(0.5f * (a2pb2 + temp1));

    const float term1 = a2pb2 + cosThetaI2;
    const float term2 = 2 * a * aCosThetaI;

    const float Rs2 = (term1 - term2) / (term1 + term2);

    const float term3 = a2pb2 * cosThetaI2 + sinThetaI4;
    const float term4 = term2 * sinThetaI2;

    const float Rp2 = Rs2 * (term3 - term4) / (term3 + term4);

    const float reflectance = 0.5f * (Rp2 + Rs2); // non-polarising reflectance

    PG3_ASSERT_FLOAT_IN_RANGE(Rs2,          0.0f, 1.0f);
    PG3_ASSERT_FLOAT_IN_RANGE(Rp2,          0.0f, 1.0f);
    PG3_ASSERT_FLOAT_IN_RANGE(reflectance,  0.0f, 1.0f);
    PG3_ASSERT_FLOAT_LARGER_THAN_OR_EQUAL_TO(Rs2 + 0.00001f, Rp2);

#else
#error Unspecified Fresnel version!
#endif

    return reflectance;
}

//////////////////////////////////////////////////////////////////////////
// Geometry routines
//////////////////////////////////////////////////////////////////////////

// Theta - inclination angle
// Phi   - azimuth angle
Vec3f CreateDirection(
    const float aSinTheta,
    const float aCosTheta,
    const float aSinPhi,
    const float aCosPhi
    )
{
    PG3_ASSERT_FLOAT_VALID(aSinTheta);
    PG3_ASSERT_FLOAT_VALID(aCosTheta);
    PG3_ASSERT_FLOAT_VALID(aSinPhi);
    PG3_ASSERT_FLOAT_VALID(aCosPhi);

    return Vec3f(aSinTheta * aCosPhi, aSinTheta * aSinPhi, aCosTheta);
}

// Theta - inclination angle
// Phi   - azimuth angle
Vec3f CreateDirection(
    const float aTheta,
    const float aPhi)
{
    PG3_ASSERT_FLOAT_VALID(aTheta);
    PG3_ASSERT_FLOAT_VALID(aPhi);

    return CreateDirection(
        std::sin(aTheta),
        std::cos(aTheta),
        std::sin(aPhi),
        std::cos(aPhi));
}

float TanThetaSqr(const Vec3f & aDirLocal)
{
    PG3_ASSERT_VEC3F_NORMALIZED(aDirLocal);

    const float cosThetaSqr = aDirLocal.z * aDirLocal.z;
    const float sinThetaSqr = 1.f - cosThetaSqr;
    if (sinThetaSqr <= 0.0f)
        return 0.0f;
    else
        return sinThetaSqr / cosThetaSqr;
}

// Reflect vector through (0,0,1)
Vec3f ReflectLocal(const Vec3f& aDirIn)
{
    return Vec3f(-aDirIn.x, -aDirIn.y, aDirIn.z);
}

// Reflect vector through given normal.
// Both vectors are expected to be normalized.
// Returns whether the input/output direction is in the half-space defined by the normal.
void Reflect(
          Vec3f &oDirOut,
          bool  &oIsAboveSurface,
    const Vec3f &aDirIn,
    const Vec3f &aNormal)
{
    PG3_ASSERT_VEC3F_NORMALIZED(aDirIn);
    PG3_ASSERT_VEC3F_NORMALIZED(aNormal);

    const float dot = Dot(aDirIn, aNormal); // projection of aDirIn on normal
    oDirOut = (2.0f * dot) * aNormal - aDirIn;

    PG3_ASSERT_VEC3F_NORMALIZED(oDirOut);

    oIsAboveSurface = dot > 0.0f;
}

void Refract(
          Vec3f &oDirOut,
          bool  &oIsDirInAboveSurface,
    const Vec3f &aDirIn,
    const Vec3f &aNormal,
          float  aEtaAbs           // internal IOR / external IOR
          )
{
    PG3_ASSERT_VEC3F_NORMALIZED(aDirIn);
    PG3_ASSERT_VEC3F_NORMALIZED(aNormal);
    PG3_ASSERT_FLOAT_LARGER_THAN(aEtaAbs, 0.0f);

    const float cosThetaI = Dot(aDirIn, aNormal);

    oIsDirInAboveSurface = (cosThetaI > 0.0f);

    if (oIsDirInAboveSurface)
        aEtaAbs = 1.0f / aEtaAbs;

    const float cosThetaTSqr = 1 - (1 - cosThetaI * cosThetaI) * (aEtaAbs * aEtaAbs);

    if (cosThetaTSqr < 0.0f)
    {
        // Total internal reflection
        oDirOut.Set(0.0f, 0.0f, 0.0f);
        return;
    }

    float cosThetaT = std::sqrt(cosThetaTSqr);
    cosThetaT = (cosThetaI > 0.0f) ? -cosThetaT : cosThetaT;

    oDirOut = aNormal * (cosThetaI * aEtaAbs + cosThetaT) - aDirIn * aEtaAbs;

    PG3_ASSERT_VEC3F_NORMALIZED(oDirOut);
}

// Jacobian of the reflection transform
float MicrofacetReflectionJacobian(
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
float MicrofacetRefractionJacobian(
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
    const float denominator    = Sqr(aEtaOutIn * cosThetaIM + cosThetaOM);
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
    if (IsTiny(length))
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
    if (IsTiny(length))
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

#ifdef UNIT_TESTS_CODE_ENABLED

bool _UnitTest_HalfwayVectorRefractionLocal_TestSingleInOutConfiguration(
    const UnitTestBlockLevel aMaxUtBlockPrintLevel,
    const float  thetaIn,
    const float  thetaOut,
    const float  phiOut,
    const float  upperN,
    const float  lowerN)
{
    const Vec3f dirIn  = CreateDirection(thetaIn, 0.0f);
    const Vec3f dirOut = CreateDirection(thetaOut, phiOut);

    const bool isDirInBelow  = dirIn.z  < 0.0f;
    const bool isDirOutBelow = dirOut.z < 0.0f;

    UT_BEGIN(
        aMaxUtBlockPrintLevel, eutblSingleStep,
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
            UT_END_FAILED(
                aMaxUtBlockPrintLevel, eutblSingleStep,
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
            // Test behaviour using the Refract() function
            Vec3f dirOutComputed;
            bool isAboveMicrofacet;
            const auto eta = lowerN / upperN;
            Refract(dirOutComputed, isAboveMicrofacet, dirIn, halfVector, eta);

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
                    UT_END_FAILED(
                        aMaxUtBlockPrintLevel, eutblSingleStep,
                        "In-Out: In: (% .2f, % .2f, % .2f), Out: (% .2f, % .2f, % .2f)",
                        "Both halfway vector and refraction are valid, but out directions are not equal!",
                        dirIn.x, dirIn.y, dirIn.z,
                        dirOut.x, dirOut.y, dirOut.z);
                    return false;
                }
            }
            else if (refractionValidityCoef < -0.0001f)
            {
                UT_END_FAILED(
                    aMaxUtBlockPrintLevel, eutblSingleStep,
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
        UT_FATAL_ERROR(
            aMaxUtBlockPrintLevel, eutblSingleStep,
            "In-Out: In: (% .2f, % .2f, % .2f), Out: (% .2f, % .2f, % .2f)",
            "Unexpected case!",
            dirIn.x, dirIn.y, dirIn.z,
            dirOut.x, dirOut.y, dirOut.z);
        return false;
    }

    UT_END_PASSED(
        aMaxUtBlockPrintLevel, eutblSingleStep,
        "In-Out: In: (% .2f, % .2f, % .2f), Out: (% .2f, % .2f, % .2f)",
        dirIn.x, dirIn.y, dirIn.z,
        dirOut.x, dirOut.y, dirOut.z);

    return true;
}

bool _UnitTest_HalfwayVectorRefractionLocal_TestSingleInHalfvectorConfiguration(
    const UnitTestBlockLevel aMaxUtBlockPrintLevel,
    const float  thetaIn,
    const float  thetaHalfwayVector,
    const float  phiHalfwayVector,
    const float  upperN,
    const float  lowerN)
{
    const Vec3f dirIn       = CreateDirection(thetaIn, 0.0f);
    const Vec3f halfVector  = CreateDirection(thetaHalfwayVector, phiHalfwayVector);

    if (halfVector.z < 0.0f)
        return true;

    UT_BEGIN(
        aMaxUtBlockPrintLevel, eutblSingleStep,
        "In-HalfwayVector: In: (% .2f, % .2f, % .2f), HalfwayVector: (% .2f, % .2f, % .2f)",
        dirIn.x, dirIn.y, dirIn.z,
        halfVector.x, halfVector.y, halfVector.z);

    // Refract
    Vec3f dirOut;
    bool isAboveMicrofacet;
    const auto etaAbs = lowerN / upperN;
    Refract(dirOut, isAboveMicrofacet, dirIn, halfVector, etaAbs);

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
            UT_END_FAILED(
                aMaxUtBlockPrintLevel, eutblSingleStep,
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
        const float halwayVectCompValidityCoef = Min3(subCoef1, subCoef2, subCoef3);

        // Sanity test
        if (isHalfwayVectorValid != (halwayVectCompValidityCoef >= 0.0f))
        {
            UT_FATAL_ERROR(
                aMaxUtBlockPrintLevel, eutblSingleStep,
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
                UT_END_FAILED(
                    aMaxUtBlockPrintLevel, eutblSingleStep,
                    "In-HalfwayVector: In: (% .2f, % .2f, % .2f), HalfwayVector: (% .2f, % .2f, % .2f)",
                    "Both refraction and halfway vector are valid, but halfway vectors are not equal!",
                    dirIn.x, dirIn.y, dirIn.z,
                    halfVector.x, halfVector.y, halfVector.z);
                return false;
            }
        }
        else if (halwayVectCompValidityCoef < -0.0001f)
        {
            UT_END_FAILED(
                aMaxUtBlockPrintLevel, eutblSingleStep,
                "In-HalfwayVector: In: (% .2f, % .2f, % .2f), HalfwayVector: (% .2f, % .2f, % .2f)",
                "Refraction is valid, but halfway vector is not!",
                dirIn.x, dirIn.y, dirIn.z,
                halfVector.x, halfVector.y, halfVector.z);
            return false;
        }
    }

    UT_END_PASSED(
        aMaxUtBlockPrintLevel, eutblSingleStep,
        "In-HalfwayVector: In: (% .2f, % .2f, % .2f), HalfwayVector: (% .2f, % .2f, % .2f)",
        dirIn.x, dirIn.y, dirIn.z,
        halfVector.x, halfVector.y, halfVector.z);

    return true;
}

bool _UnitTest_HalfwayVectorRefractionLocal_TestInterface(
    UnitTestBlockLevel  aMaxUtBlockPrintLevel,
    const float         aUpperN, // above macro surface
    const float         aLowerN  // below macro surface
    )
{
    // Deterministic directions generation

    const float thetaInStepCount    = 32;
    const float thetaInStart        = 0.0f * PI_F;
    const float thetaInEnd          = 2.0f * PI_F;

    const float thetaOutStepCount   = 32;
    const float thetaOutStart       = 0.0f * PI_F;
    const float thetaOutEnd         = 2.0f * PI_F;

    const float phiOutStepCount     = 16;
    const float phiOutStart         = 0.0f * PI_F;
    const float phiOutEnd           = 2.0f * PI_F;

    UT_BEGIN(
        aMaxUtBlockPrintLevel, eutblSubTest,
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
                    _UnitTest_HalfwayVectorRefractionLocal_TestSingleInOutConfiguration(
                        aMaxUtBlockPrintLevel,
                        thetaIn, thetaOut, phiOut,
                        aUpperN, aLowerN);
                if (!success)
                    return false;

                success =
                    _UnitTest_HalfwayVectorRefractionLocal_TestSingleInHalfvectorConfiguration(
                        aMaxUtBlockPrintLevel,
                        thetaIn, thetaOut, phiOut,
                        aUpperN, aLowerN);
                if (!success)
                    return false;
            }
        }
    }

    UT_END_PASSED(
        aMaxUtBlockPrintLevel, eutblSubTest,
        "Air(%.2f)/glass(%.2f) interface, deterministics directions (%.0fx%.0fx%.0f)",
        aUpperN, aLowerN, thetaInStepCount, thetaOutStepCount, phiOutStepCount);

    // Monte Carlo testing

    const uint32_t randomSamplesCount = 32 * 32 * 64;
    Rng rng(1998);

    UT_BEGIN(
        aMaxUtBlockPrintLevel, eutblSubTest,
        "Air(%.2f)/glass(%.2f) interface, random directions (%d)",
        aUpperN, aLowerN, randomSamplesCount);

    for (uint32_t sample = 0; sample < randomSamplesCount; sample++)
    {
        // Sample spheres uniformly
        const Vec3f samples = rng.GetVec3f();
        const float thetaIn     = std::acos(1.f - 2.f * samples.x);
        const float thetaOut    = std::acos(1.f - 2.f * samples.y);
        const float phiOut      = samples.z * 2.0f * PI_F;

        bool success;

        success = _UnitTest_HalfwayVectorRefractionLocal_TestSingleInOutConfiguration(
            aMaxUtBlockPrintLevel,
            thetaIn, thetaOut, phiOut,
            aUpperN, aLowerN);
        if (!success)
            return false;

        success = _UnitTest_HalfwayVectorRefractionLocal_TestSingleInHalfvectorConfiguration(
            aMaxUtBlockPrintLevel,
            thetaIn, thetaOut, phiOut,
            aUpperN, aLowerN);
        if (!success)
            return false;
    }

    UT_END_PASSED(
        aMaxUtBlockPrintLevel, eutblSubTest,
        "Air(%.2f)/glass(%.2f) interface, random directions (%d)",
        aUpperN, aLowerN, randomSamplesCount);

    return true;
}

bool _UnitTest_HalfwayVectorRefractionLocal(
    UnitTestBlockLevel   aMaxUtBlockPrintLevel)
{
    UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest, "HalfwayVectorRefractionLocal()");

    if (!_UnitTest_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.00f, 1.51f))
        return false;
    if (!_UnitTest_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.51f, 1.00f))
        return false;

    if (!_UnitTest_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.10f, 1.00f))
        return false;
    if (!_UnitTest_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.00f, 1.10f))
        return false;

    if (!_UnitTest_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.51f, 1.55f))
        return false;
    if (!_UnitTest_HalfwayVectorRefractionLocal_TestInterface(aMaxUtBlockPrintLevel, 1.55f, 1.51f))
        return false;

    UT_END_PASSED(
        aMaxUtBlockPrintLevel, eutblWholeTest,
        "HalfwayVectorRefractionLocal()");

    return true;
}

#endif

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

// Sample Triangle
// returns barycentric coordinates
Vec2f SampleUniformTriangle(const Vec2f &aSamples)
{
    const float xSqr = std::sqrt(aSamples.x);

    return Vec2f(1.f - xSqr, aSamples.y * xSqr);
}

//////////////////////////////////////////////////////////////////////////
// Cosine-Weighted Sphere sampling
//////////////////////////////////////////////////////////////////////////

// Sample direction in the upper hemisphere with cosine-proportional pdf
// The returned PDF is with respect to solid angle measure
Vec3f SampleCosHemisphereW(
    const Vec2f &aSamples,
    float       *oPdfW = NULL)
{
    const float term1 = 2.f * PI_F * aSamples.x;
    const float term2 = std::sqrt(1.f - aSamples.y);

    const Vec3f ret(
        std::cos(term1) * term2,
        std::sin(term1) * term2,
        std::sqrt(aSamples.y));

    if (oPdfW)
        *oPdfW = ret.z * INV_PI_F;

    PG3_ASSERT_FLOAT_NONNEGATIVE(ret.z);

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

//////////////////////////////////////////////////////////////////////////
// Sphere sampling
//////////////////////////////////////////////////////////////////////////

float UniformSpherePdfW()
{
    //return (1.f / (4.f * PI_F));
    return INV_PI_F * 0.25f;
}

Vec3f SampleUniformSphereW(
    const Vec2f  &aSamples,
    float        *oPdfSA)
{
    const float phi      = 2.f * PI_F * aSamples.x;
    const float sinTheta = 2.f * std::sqrt(aSamples.y - aSamples.y * aSamples.y);
    const float cosTheta = 1.f - 2.f * aSamples.y;

    PG3_ERROR_CODE_NOT_TESTED("");

    const Vec3f ret = CreateDirection(sinTheta, cosTheta, std::sin(phi), std::cos(phi));

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
// Microfacets
//////////////////////////////////////////////////////////////////////////

float MicrofacetDistributionGgx(
    const Vec3f &aMicrofacetNormal,
    const float  aRoughnessAlpha)
{
    PG3_ASSERT_FLOAT_NONNEGATIVE(aRoughnessAlpha);

    if (aMicrofacetNormal.z <= 0.f)
        return 0.0f;

    const float roughnessAlphaSqr = aRoughnessAlpha * aRoughnessAlpha;
    const float cosThetaSqr = aMicrofacetNormal.z * aMicrofacetNormal.z;
    const float tanThetaSqr = TanThetaSqr(aMicrofacetNormal);
    const float temp1 = roughnessAlphaSqr + tanThetaSqr;
    const float temp2 = cosThetaSqr * temp1;

    const float result = roughnessAlphaSqr / (PI_F * temp2 * temp2);

    PG3_ASSERT_FLOAT_NONNEGATIVE(result);

    return result;
}

float MicrofacetSmithMaskingFunctionGgx(
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
    const float tanThetaSqr = TanThetaSqr(aDir);
    const float root = std::sqrt(1.0f + roughnessAlphaSqr * tanThetaSqr); // TODO: Optimize sqrt

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
    const Vec2f &aSample,
          Vec3f &oReflectDir)
{
    // Sample microfacet direction (D(omega_m) * cos(theta_m))
    const float tmp1 =
          (aRoughnessAlpha * std::sqrt(aSample.x))
        / std::sqrt(1.0f - aSample.x);
    const float thetaM  = std::atan(tmp1);
    const float phiM    = 2 * PI_F * aSample.y;

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
    Reflect(oReflectDir, isAboveMicrofacet, aWol, microfacetDir);

    return isAboveMicrofacet;
}

// Sampling density of GGX sampling based directly on the distribution of microfacets
float GgxSamplingPdfAllNormals(
    const Vec3f &aWol,
    const Vec3f &aWil,
    const float  aRoughnessAlpha)
{
    // Distribution value
    const Vec3f halfwayVec          = HalfwayVectorReflectionLocal(aWil, aWol);
    const float microFacetDistrVal  = MicrofacetDistributionGgx(halfwayVec, aRoughnessAlpha);
    const float microfacetPdf       = microFacetDistrVal * halfwayVec.z;

    const float transfJacobian = MicrofacetReflectionJacobian(aWil, halfwayVec);

    return microfacetPdf * transfJacobian;
}

// Sample GGX P^{22}_{\omega_o}(slope, 1, 1) from [Heitz2014] for given incident direction theta
void SampleGgxP11(
          Vec2f &aSlope,
    const float  aThetaI, 
    const Vec2f &aSample)
{
    if (aThetaI < 0.0001f)
    {
        // Normal incidence - avoid division by zero later
        const float sampleXClamped  = std::min(aSample.x, 0.9999f);
        const float radius          = SafeSqrt(sampleXClamped / (1 - sampleXClamped));
        const float phi             = 2 * PI_F * aSample.y;
        const float sinPhi          = std::sin(phi);
        const float cosPhi          = std::cos(phi);
        aSlope = Vec2f(radius * cosPhi, radius * sinPhi);
    }
    else
    {
        const float tanThetaI    = std::tan(aThetaI);
        const float tanThetaIInv = 1.0f / tanThetaI;
        const float G1 =
            2.0f / (1.0f + SafeSqrt(1.0f + 1.0f / (tanThetaIInv * tanThetaIInv)));

        // Sample x dimension (marginalized PDF - can be sampled directly via CDF^-1)
        float A = 2.0f * aSample.x / G1 - 1.0f; // TODO: dividing by G1, which can be zero?!?
        if (std::abs(A) == 1.0f)
            A -= SignNum(A) * 1e-4f; // avoid division by zero later
        const float B = tanThetaI;
        const float tmpFract = 1.0f / (A * A - 1.0f);
        const float D = SafeSqrt(B * B * tmpFract * tmpFract - (A * A - B * B) * tmpFract);
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
        if (aSample.y > 0.5f) // pick one positive/negative interval
        {
            ySign       = 1.0f;
            yHalfSample = 2.0f * (aSample.y - 0.5f);
        }
        else
        {
            ySign       = -1.0f;
            yHalfSample = 2.0f * (0.5f - aSample.y);
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
// FIXME: There must be some problem with memory alignment because when change the order 
// of parameters so that alpha is second and wol is first, it crashes on SSE movap instruction
// accessing address no aligned to 16 bytes.
Vec3f SampleGgxVisibleNormals(
    const float  aRoughnessAlpha,
    const Vec3f &aWol,
    const Vec2f &aSample)
{
    PG3_ASSERT_VEC3F_NORMALIZED(aWol);
    PG3_ASSERT_FLOAT_LARGER_THAN(aWol.z, -0.0001);

    // Stretch Wol to canonical, unit roughness space
    const Vec3f wolStretch =
        Normalize(Vec3f(
            aWol.x * aRoughnessAlpha,
            aWol.y * aRoughnessAlpha,
            aWol.z)); //std::max(aWol.z, 0.0f))); - causes problems related to the FIXME at this function's header

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
    SampleGgxP11(slopeStretch, thetaWolStretch, aSample);

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

Vec3f CrashFoo(
    const float  aFloat,
    const Vec3f &aVec3,
    const Vec2f &aVec2)
{
    const Vec3f wolStretch =
        Normalize(Vec3f(aVec3.x, aVec3.x, std::max(aVec3.x, 0.0f)));

    float thetaWolStretch = 0.0f;
    float phiWolStretch = 0.0f;
    if (wolStretch.z < 0.999f)
    {
        thetaWolStretch = std::acos(wolStretch.z);
        phiWolStretch = std::atan2(wolStretch.y, wolStretch.x);
    }
    const float sinPhi = std::sin(phiWolStretch);
    const float cosPhi = std::cos(phiWolStretch);

    Vec2f slopeStretch;
    SampleGgxP11(slopeStretch, thetaWolStretch, aVec2);

    slopeStretch = Vec2f(
        slopeStretch.x * cosPhi - slopeStretch.y * sinPhi,
        slopeStretch.x * sinPhi + slopeStretch.y * cosPhi);

    Vec2f slope(
        slopeStretch.x * aFloat,
        slopeStretch.y * aFloat);

    const float slopeLengthInv = 1.0f / Vec3f(slope.x, slope.y, 1.0f).Length();
    Vec3f microfacetDir(
        -slope.x * slopeLengthInv,
        -slope.y * slopeLengthInv,
        slopeLengthInv);

    return microfacetDir;
}

// Sampling density of GGX sampling based on [Heitz2014]
float GgxSamplingPdfVisibleNormals(
    const Vec3f &aWol,
    const Vec3f &aHalfwayVec,
    const float  aRoughnessAlpha)
{
    // TODO: Reuse computations when (if) combined with sampling routine??

    PG3_ASSERT_VEC3F_NORMALIZED(aWol);
    PG3_ASSERT_VEC3F_NORMALIZED(aHalfwayVec);
    PG3_ASSERT_FLOAT_NONNEGATIVE(aWol.z);

    if (aHalfwayVec.z <= 0.f)
        return 0.0f;

    const float distrVal    = MicrofacetDistributionGgx(aHalfwayVec, aRoughnessAlpha);
    const float masking     = MicrofacetSmithMaskingFunctionGgx(aWol, aHalfwayVec, aRoughnessAlpha);
    const float cosThetaOM  = Dot(aHalfwayVec, aWol);
    const float cosThetaO   = std::max(aWol.z, 0.00001f);

    const float microfacetPdf = (masking * std::abs(cosThetaOM) * distrVal) / cosThetaO;

    PG3_ASSERT_FLOAT_NONNEGATIVE(microfacetPdf);

    return microfacetPdf;
}

///////////////////////////////////////////////////////////////////////////////
// Others
//////////////////////////////////////////////////////////////////////////

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

