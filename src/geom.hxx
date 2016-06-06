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

    // Vertices and faces of a unit length regular icosahedron with centre in the origin [0, 0, 0]
    // Based on: http://geometrictools.com/Documentation/PlatonicSolids.pdf
    // by David Eberly, Geometric Tools, LLC
    static void UnitIcosahedron(Vec3f (&vertices)[12], Vec3ui (&faces)[20])
    {
        // Preprocessing
        const float t    = (1.0f + std::sqrt(5.0f)) / 2.0f;
        const float s    = std::sqrt(1.0f + t * t);
        const float sInv = 1.0f / s;

        // Vertices

        vertices[0]  = Vec3f( t,  1.f, 0.f) * sInv;
        vertices[1]  = Vec3f(-t,  1.f, 0.f) * sInv;
        vertices[2]  = Vec3f( t, -1.f, 0.f) * sInv;
        vertices[3]  = Vec3f(-t, -1.f, 0.f) * sInv;

        vertices[4]  = Vec3f( 1.f, 0.f,  t) * sInv;
        vertices[5]  = Vec3f( 1.f, 0.f, -t) * sInv;
        vertices[6]  = Vec3f(-1.f, 0.f,  t) * sInv;
        vertices[7]  = Vec3f(-1.f, 0.f, -t) * sInv;

        vertices[8]  = Vec3f(0.f,  t,  1.f) * sInv;
        vertices[9]  = Vec3f(0.f, -t,  1.f) * sInv;
        vertices[10] = Vec3f(0.f,  t, -1.f) * sInv;
        vertices[11] = Vec3f(0.f, -t, -1.f) * sInv;

        // TODO: Assert normalization/unit length

        // Faces

        faces[ 0].Set( 0,  8,  3);
        faces[ 1].Set( 1, 10,  7);
        faces[ 2].Set( 2,  9, 11);
        faces[ 3].Set( 7,  3,  1);

        faces[ 4].Set( 0,  5, 10);
        faces[ 5].Set( 3,  9,  6);
        faces[ 6].Set( 3, 11,  9);
        faces[ 7].Set( 8,  6,  4);

        faces[ 8].Set( 2,  4,  9);
        faces[ 9].Set( 3,  7, 11);
        faces[10].Set( 4,  2,  0);
        faces[11].Set( 9,  4,  6);

        faces[12].Set( 2, 11,  5);
        faces[13].Set( 0, 10,  8);
        faces[14].Set( 5,  0,  2);
        faces[15].Set(10,  5,  7);

        faces[16].Set( 1,  6,  8);
        faces[17].Set( 1,  8, 10);
        faces[18].Set( 6,  1,  3);
        faces[19].Set(11,  7,  5);
    }

} // namespace Geom
