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

//////////////////////////////////////////////////////////////////////////
// Fresnel-related routines
//////////////////////////////////////////////////////////////////////////

float FresnelDielectric(
    float aCosThetaI,
    float aEta)         // internal IOR / external IOR
{
    if (aEta < 0)
        return 1.f;

    if (aCosThetaI < 0.f)
    {
        aCosThetaI = -aCosThetaI;
        aEta = aEta;
    }
    else
        aEta = 1.f / aEta;

    const float sinThetaT2 = Sqr(aEta) * (1.f - Sqr(aCosThetaI));
    const float cosThetaT  = SafeSqrt(1.f - sinThetaT2);

    //// debug
    //const float thetaI = std::acos(aCosThetaI);
    //const float thetaT = std::acos(cosThetaT);

    // Perpendicular (senkrecht) polarization
    const float term2                   = aEta * aCosThetaI;
    const float reflPerpendicularSqrt   = (term2 - cosThetaT) / (term2 + cosThetaT);
    const float reflPerpendicular       = Sqr(reflPerpendicularSqrt);

    // Parallel polarization
    const float term1               = aEta * cosThetaT;
    const float reflParallelSqrt    = (aCosThetaI - term1) / (aCosThetaI + term1);
    const float reflParallel        = Sqr(reflParallelSqrt);

    const float reflectance = 0.5f * (reflParallel + reflPerpendicular);

    PG3_ASSERT_FLOAT_IN_RANGE(reflParallel, 0.0f, 1.0f);
    PG3_ASSERT_FLOAT_IN_RANGE(reflPerpendicular, 0.0f, 1.0f);
    PG3_ASSERT_FLOAT_IN_RANGE(reflectance, 0.0f, 1.0f);
    PG3_ASSERT_FLOAT_LARGER_THAN_OR_EQUAL_TO(reflPerpendicular + 0.00001f, reflParallel);

    return reflectance;
}

float FresnelConductor(
    float aCosThetaI,
    float aEta,         // internal IOR / external IOR
    float aAbsorbance
    )
{
    PG3_ASSERT_FLOAT_LARGER_THAN(aEta, 0.0f);
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

    const float iorSqr    = Sqr(aEta);
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

    const float aEta2        = aEta * aEta;
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

// Reflect vector through (0,0,1)
Vec3f ReflectLocal(const Vec3f& aVector)
{
    return Vec3f(-aVector.x, -aVector.y, aVector.z);
}

// Reflect vector through given normal.
// Both vectors are expected to be normalized.
// Returns whether the input/output direction is in the half-space defined by the normal.
void Reflect(
          Vec3f &oVectorOut,
          bool  &oIsAboveSurface,
    const Vec3f &aVectorIn,
    const Vec3f &aNormal)
{
    PG3_ASSERT_VEC3F_NORMALIZED(aVectorIn);
    PG3_ASSERT_VEC3F_NORMALIZED(aNormal);

    const float dot = Dot(aVectorIn, aNormal); // projection of aVectorIn on normal
    oVectorOut = (2.0f * dot) * aNormal - aVectorIn;

    PG3_ASSERT_VEC3F_NORMALIZED(oVectorOut);

    oIsAboveSurface = dot > 0.0f;
}

void Refract(
          Vec3f &oVectorOut,
          bool  &oIsAboveSurface,
    const Vec3f &aVectorIn,
    const Vec3f &aNormal,
          float  aEta           // internal IOR / external IOR
          )
{
    PG3_ASSERT_VEC3F_NORMALIZED(aVectorIn);
    PG3_ASSERT_VEC3F_NORMALIZED(aNormal);

    const float cosThetaI = Dot(aVectorIn, aNormal);

    oIsAboveSurface = (cosThetaI > 0.0f);

    if (oIsAboveSurface)
        aEta = 1.0f / aEta;

    const float cosThetaTSqr = 1 - (1 - cosThetaI * cosThetaI) * (aEta * aEta);

    if (cosThetaTSqr < 0.0f)
    {
        oVectorOut.Set(0.0f, 0.0f, 0.0f); // TODO
        return;
    }

    float cosThetaT = std::sqrt(cosThetaTSqr);
    cosThetaT = (cosThetaI > 0.0f) ? -cosThetaT : cosThetaT;

    //// debug
    //const float thetaI = std::acos(std::abs(cosThetaI));
    //const float thetaT = std::acos(std::abs(cosThetaT));

    oVectorOut = aNormal * (cosThetaI * aEta + cosThetaT) - aVectorIn * aEta;
}

// Jacobian of the reflection transform
float MicrofacetReflectionJacobian(const Vec3f &aWil, const Vec3f &aMicrofacetNormal)
{
    PG3_ASSERT_VEC3F_NORMALIZED(aWil);
    PG3_ASSERT_VEC3F_NORMALIZED(aMicrofacetNormal);

    const float cosThetaIM = Dot(aMicrofacetNormal, aWil);
    const float cosThetaIMClamped = std::max(cosThetaIM, 0.000001f);

    const float transfJacobian = 1.0f / (4.0f * cosThetaIMClamped);

    PG3_ASSERT_FLOAT_NONNEGATIVE(transfJacobian);

    return transfJacobian;
}

// Jacobian of the refraction transform
float MicrofacetRefractionJacobian(
    const Vec3f &aWil,
    const Vec3f &aWol,
    const Vec3f &aMicrofacetNormal,
    const float  aEta
    )
{
    PG3_ASSERT_VEC3F_NORMALIZED(aWil);
    PG3_ASSERT_VEC3F_NORMALIZED(aWol);
    PG3_ASSERT_VEC3F_NORMALIZED(aMicrofacetNormal);

    const float cosThetaIM = Dot(aMicrofacetNormal, aWil);
    const float cosThetaOM = Dot(aMicrofacetNormal, aWol);

    const float denom = Sqr(cosThetaIM + aEta * cosThetaOM);
    const float transfJacobian = (Sqr(aEta) * std::abs(cosThetaOM)) / std::max(denom, 0.000001f);

    PG3_ASSERT_FLOAT_NONNEGATIVE(transfJacobian);

    return transfJacobian;
}

// Halfway vector, microfacet normal
// TODO: Add comment about reflection/refraction
// Incoming/outgoing directions on different sides of the macro surface are allowed.
Vec3f HalfwayVectorReflectionLocal(
    const Vec3f& aWil,
    const Vec3f& aWol
    )
{
    Vec3f halfwayVec = aWil + aWol;
    
    const float length = halfwayVec.Length();
    if (IsTiny(length))
        halfwayVec = Vec3f(0.0f, 0.0f, 1.0f); // Geometrical normal
    else
        halfwayVec /= length; // Normalize using the already computed length

    // Must point into the positive half-space
    halfwayVec *= (halfwayVec.z >= 0.0f) ? 1.0f : -1.0f;

    PG3_ASSERT_VEC3F_NORMALIZED(halfwayVec);

    return halfwayVec;
}

// Halfway vector, microfacet normal
// TODO: Add comment about reflection/refraction
// TODO: If we arrive from below, then eta should probably switch
// Incoming/outgoing directions on different sides of the macro surface are allowed.
Vec3f HalfwayVectorRefractionLocal(
    const Vec3f &aWil,
    const Vec3f &aWol,
    const float  aEta
    )
{
    // TODO: Vector asserts:
    // - normalized vectors
    // - on different sides of surface
    // eta assert?

    Vec3f halfwayVec = aWil + aWol * aEta;
    
    const float length = halfwayVec.Length();
    if (IsTiny(length))
        halfwayVec = Vec3f(0.0f, 0.0f, 1.0f); // Geometrical normal
    else
        halfwayVec /= length; // Normalize using the already computed length

    // Must point into the positive half-space
    halfwayVec *= (halfwayVec.z >= 0.0f) ? 1.0f : -1.0f;

    PG3_ASSERT_VEC3F_NORMALIZED(halfwayVec);

    return halfwayVec;
}

float TanTheta2(const Vec3f & aVectorLocal)
{
    PG3_ASSERT_VEC3F_NORMALIZED(aVectorLocal);

    const float cosTheta2 = aVectorLocal.z * aVectorLocal.z;
    const float sinTheta2 = 1.f - cosTheta2;
    if (sinTheta2 <= 0.0f)
        return 0.0f;
    else
        return sinTheta2 / cosTheta2;
}

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

// Sample direction in the upper hemisphere with cosine-proportional pdf
// The returned PDF is with respect to solid angle measure
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

// Sample Triangle
// returns barycentric coordinates
Vec2f SampleUniformTriangle(const Vec2f &aSamples)
{
    const float xSqr = std::sqrt(aSamples.x);

    return Vec2f(1.f - xSqr, aSamples.y * xSqr);
}

//////////////////////////////////////////////////////////////////////////
// Sphere sampling
//////////////////////////////////////////////////////////////////////////

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

    const float roughnessAlpha2 = aRoughnessAlpha * aRoughnessAlpha;
    const float cosTheta2 = aMicrofacetNormal.z * aMicrofacetNormal.z;
    const float tanTheta2 = TanTheta2(aMicrofacetNormal);
    const float temp1 = roughnessAlpha2 + tanTheta2;
    const float temp2 = cosTheta2 * temp1;

    const float result = roughnessAlpha2 / (PI_F * temp2 * temp2);

    PG3_ASSERT_FLOAT_NONNEGATIVE(result);

    return result;
}

float MicrofacetSmithMaskingFunctionGgx(
    const Vec3f &aWvl,  // the direction to compute masking for (either incoming or outgoing)
    const Vec3f &aMicrofacetNormal,
    const float  aRoughnessAlpha)
{
    PG3_ASSERT_FLOAT_NONNEGATIVE(aRoughnessAlpha);

    if (/*(aWvl.z <= 0) ||*/ (aMicrofacetNormal.z <= 0))
        return 0.0f;

    const float roughnessAlpha2 = aRoughnessAlpha * aRoughnessAlpha;
    const float tanTheta2 = TanTheta2(aWvl);
    const float root = std::sqrt(1.0f + roughnessAlpha2 * tanTheta2); // TODO: Optimize sqrt

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

    const float transfJacobian = MicrofacetReflectionJacobian(aWol, halfwayVec);

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
Vec3f SampleGgxVisibleNormals(
    const Vec3f &aWol,
    const float  aRoughnessAlpha,
    const Vec2f &aSample)
{
    PG3_ASSERT_FLOAT_NONNEGATIVE(aWol.z);

    // Stretch Wol to canonical, unit roughness space
    Vec3f wolStretch =
        Normalize(Vec3f(aWol.x * aRoughnessAlpha, aWol.y * aRoughnessAlpha, aWol.z));

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

