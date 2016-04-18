#pragma once

#include "debugging.hxx"
#include "math.hxx"

///////////////////////////////////////////////////////////////////////////////
// Geometry routines
///////////////////////////////////////////////////////////////////////////////
namespace Geom
{
    const float kEpsRay = 1e-3f;

    // Dynamic version (chooses from [kEpsRay, N*kEpsRay])
    // eps + n * (1-cos) * eps = (1 + n * (1-cos)) * eps
    // The smaller the cosine is, the larger epsilon we use to avoid 
    // numerical problems, e.g. causing self-intersection when shooting rays from a surface,
    // while starting as close to the surface as possible to avoid light leaks
    float EpsRayCos(float aCos)
    {
        return (1.f + 2.f * (1.f - aCos)) * kEpsRay;
    }

    const float kEpsDist = 1e-6f;

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
} // namespace Geom
