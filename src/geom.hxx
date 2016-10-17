#pragma once

#include "debugging.hxx"
#include "unittesting.hxx"
#include "math.hxx"

#include <list>
#include <set>

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

    // Returns direction on unit sphere such that its longitude equals 2*PI*u and its latitude equals PI*v.
    PG3_PROFILING_NOINLINE
    inline Vec3f LatLong2Dir(const Vec2f &aUV)
    {
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.x, 0.f, 1.f);
        PG3_ASSERT_FLOAT_IN_RANGE(aUV.y, 0.f, 1.f);

        const float phi   = -(aUV.x - 0.5f) * 2 * Math::kPiF; // we rotate in the opposite direction
        const float theta = aUV.y * Math::kPiF;

        return Geom::CreateDirection(theta, phi);
    }

    // Returns vector [u,v] in [0,1]x[0,1]. The direction must be non-zero and normalized.
    PG3_PROFILING_NOINLINE
    inline Vec2f Dir2LatLong(const Vec3f &aDirection)
    {
        PG3_ASSERT(!aDirection.IsZero() /*(aDirection.Length() == 1.0f)*/);

        // minus sign because we rotate in the opposite direction
        // Math::FastAtan2 is 50 times faster than atan2f at the price of slightly horizontally distorted mapping
        const float phi     = -Math::FastAtan2(aDirection.y, aDirection.x);
        const float theta   = acosf(aDirection.z);

        // Convert from [-Pi,Pi] to [0,1]
        //const float uTemp = fmodf(phi * 0.5f * Math::kPiInvF, 1.0f);
        //const float u = Math::Clamp(uTemp, 0.f, 1.f); // TODO: maybe not necessary
        const float u = Math::Clamp(0.5f + phi * 0.5f * Math::kPiInvF, 0.f, 1.f);

        // Convert from [0,Pi] to [0,1]
        const float v = Math::Clamp(theta * Math::kPiInvF, 0.f, 1.0f);

        return Vec2f(u, v);
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

    void ComputeSphericalTriangleAngles(
        float       &oAlpha,
        float       &oBeta,
        float       &oGamma,
        const Vec3f &aVertexA,
        const Vec3f &aVertexB,
        const Vec3f &aVertexC)
    {
        const auto normalAB = Cross(aVertexA, aVertexB);
        const auto normalBC = Cross(aVertexB, aVertexC);
        const auto normalCA = Cross(aVertexC, aVertexA);

        const float cosAlpha = Dot(normalAB, -normalCA);
        const float cosBeta  = Dot(normalBC, -normalAB);
        const float cosGamma = Dot(normalCA, -normalBC);

        oAlpha = std::acos(cosAlpha);
        oBeta  = std::acos(cosBeta);
        oGamma = std::acos(cosGamma);

        PG3_ASSERT_FLOAT_NONNEGATIVE(oAlpha);
        PG3_ASSERT_FLOAT_NONNEGATIVE(oBeta);
        PG3_ASSERT_FLOAT_NONNEGATIVE(oGamma);
    }

    // Area of a spherical triangle on a unit sphere, a.k.a. solid angle
    float SphericalTriangleArea(
        const float       &aAlpha,
        const float       &aBeta,
        const float       &aGamma)
    {
        PG3_ASSERT_FLOAT_NONNEGATIVE(aAlpha);
        PG3_ASSERT_FLOAT_NONNEGATIVE(aBeta);
        PG3_ASSERT_FLOAT_NONNEGATIVE(aGamma);

        const float area = aAlpha + aBeta + aGamma - Math::kPiF;

        PG3_ASSERT_FLOAT_NONNEGATIVE(area);

        return area;
    }

    // Area of a spherical triangle on a unit sphere, a.k.a. solid angle
    float SphericalTriangleArea(
        const Vec3f &aVertexA,
        const Vec3f &aVertexB,
        const Vec3f &aVertexC)
    {
        float alpha, beta, gamma;
        ComputeSphericalTriangleAngles(alpha, beta, gamma, aVertexA, aVertexB, aVertexC);

        const float area = SphericalTriangleArea(alpha, beta, gamma);

        return area;
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

#ifdef PG3_ASSERT_ENABLED
        for (uint32_t i = 0; i < Utils::ArrayLength(vertices); i++)
            PG3_ASSERT_VEC3F_NORMALIZED(vertices[i]);
#endif

        // Faces

        faces[ 0].Set( 0,  8,  4);
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

#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

    bool _UnitTest_UnitIcosahedron_Vertices(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel,
        const Vec3f(&vertices)[12])
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "Vertices");

        for (uint32_t vertexId = 0; vertexId < Utils::ArrayLength(vertices); vertexId++)
        {
            auto &vertex = vertices[vertexId];

            for (uint32_t coord = 0; coord < 3; coord++)
            {
                // Coordinate numbers validity
                if (std::isnan(vertex.Get(coord)))
                {
                    std::ostringstream errorDescription;
                    errorDescription << "Coordinate ";
                    errorDescription << coord;
                    errorDescription << " of vertex ";
                    errorDescription << vertexId;
                    errorDescription << " is NaN!";
                    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "Vertices",
                        errorDescription.str().c_str());
                    return false;
                }
                if (!std::isfinite(vertex.Get(coord)))
                {
                    std::ostringstream errorDescription;
                    errorDescription << "Coordinate ";
                    errorDescription << coord;
                    errorDescription << " of vertex ";
                    errorDescription << vertexId;
                    errorDescription << " is NOT FINITE!";
                    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "Vertices",
                        errorDescription.str().c_str());
                    return false;
                }
            }

            // Unit vector lengths
            if (fabs(vertex.LenSqr() - 1.0f) > 0.001f)
            {
                std::ostringstream errorDescription;
                errorDescription << "The direction of vertex ";
                errorDescription << vertexId;
                errorDescription << " is NOT UNIT!";
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "Vertices",
                    errorDescription.str().c_str());
                return false;
            }
        }

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "Vertices");
        return true;
    }

    bool _UnitTest_UnitIcosahedron_Faces(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel,
        const Vec3f(&vertices)[12],
        const Vec3ui(&faces)[20])
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "Faces");

        // Valid vertex indices
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex indices validity");
        for (uint32_t faceId = 0; faceId < Utils::ArrayLength(faces); faceId++)
        {
            auto &face = faces[faceId];
            for (uint32_t vertexSeqNum = 0; vertexSeqNum < 3; vertexSeqNum++)
            {
                auto vertexId = face.Get(vertexSeqNum);

                if (vertexId >= 12)
                {
                    std::ostringstream errorDescription;
                    errorDescription << "The vertex ";
                    errorDescription << vertexSeqNum;
                    errorDescription << " of face ";
                    errorDescription << faceId;
                    errorDescription << " is out of range (";
                    errorDescription << vertexId;
                    errorDescription << " >= 12)!";
                    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex indices validity",
                        errorDescription.str().c_str());
                    return false;
                }
            }
        }
        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex indices validity");

        // Edge lengths
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Edges lengths");
        for (uint32_t faceId = 0; faceId < Utils::ArrayLength(faces); faceId++)
        {
            auto &face = faces[faceId];
            const float edgeReferenceLength = 4.f / std::sqrt(10.f + 2.f * std::sqrt(5.f));
            const float edgeReferenceLengthSqr = edgeReferenceLength * edgeReferenceLength;
            for (uint32_t vertexSeqNum = 0; vertexSeqNum < 3; vertexSeqNum++)
            {
                auto vertex1Id = face.Get(vertexSeqNum);
                auto vertex2Id = face.Get((vertexSeqNum + 1) % 3);

                auto &vertex1Coords = vertices[vertex1Id];
                auto &vertex2Coords = vertices[vertex2Id];
                auto edgeLengthSqr = (vertex1Coords - vertex2Coords).LenSqr();
                if (fabs(edgeLengthSqr - edgeReferenceLengthSqr) > 0.001f)
                {
                    std::ostringstream errorDescription;
                    errorDescription << "The edge between vertices ";
                    errorDescription << vertex1Id;
                    errorDescription << " and ";
                    errorDescription << vertex2Id;
                    errorDescription << " has incorrect length (sqrt(";
                    errorDescription << edgeLengthSqr;
                    errorDescription << ") instead of sqrt(";
                    errorDescription << edgeReferenceLengthSqr;
                    errorDescription << "))!";
                    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Edges lengths",
                        errorDescription.str().c_str());
                    return false;
                }
            }
        }
        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Edges lengths");

        // Face uniqueness
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Face uniqueness");
        std::list<std::set<uint32_t>> alreadyFoundFaceVertices;
        for (uint32_t faceId = 0; faceId < Utils::ArrayLength(faces); faceId++)
        {
            auto &face = faces[faceId];
            std::set<uint32_t> vertexSet = { face.Get(0), face.Get(1), face.Get(2) };
            auto it = std::find(alreadyFoundFaceVertices.begin(),
                                alreadyFoundFaceVertices.end(),
                                vertexSet);
            if (it != alreadyFoundFaceVertices.end())
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Face uniqueness",
                    "Found duplicate face!");
                return false;
            }
            alreadyFoundFaceVertices.push_back(vertexSet);
        }
        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Face uniqueness");

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "Faces");
        return true;
    }

    bool _UnitTest_UnitIcosahedron(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest, "Geom::UnitIcosahedron()");

        Vec3f  vertices[12];
        Vec3ui faces[20];
        UnitIcosahedron(vertices, faces);

        if (!_UnitTest_UnitIcosahedron_Vertices(aMaxUtBlockPrintLevel, vertices))
            return false;

        if (!_UnitTest_UnitIcosahedron_Faces(aMaxUtBlockPrintLevel, vertices, faces))
            return false;

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblWholeTest, "Geom::UnitIcosahedron()");
        return true;
    }

#endif

    // Code adapted from:
    // http://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/barycentric-coordinates
    bool RayTriangleIntersect(
        const Vec3f &aRayOrig,
        const Vec3f &aRayDir,
        const Vec3f &aTriVertex0,
        const Vec3f &aTriVertex1,
        const Vec3f &aTriVertex2,
              float &oT,
              float &oU,
              float &oV,
        const float  aEdgeNumThreshold = 0.0f)
    {
        PG3_ASSERT_FLOAT_NONNEGATIVE(aEdgeNumThreshold);

        // compute plane's normal
        const Vec3f v0v1 = aTriVertex1 - aTriVertex0;
        const Vec3f v0v2 = aTriVertex2 - aTriVertex0;
        // no need to normalize
        const Vec3f N = Cross(v0v1, v0v2); // N 
        const float denom = Dot(N, N);

        //////////////////////
        // Step 1: finding P
        //////////////////////

        // check if ray and plane are parallel ?
        const float NdotRayDirection = Dot(N, aRayDir);
        if (fabs(NdotRayDirection) < 0.0001f) // almost 0 
            return false; // they are parallel so they don't intersect ! 

        // compute d parameter using equation 2
        const float d = Dot(N, aTriVertex0);

        // compute t (equation 3)
        oT = (Dot(N, aRayOrig) + d) / NdotRayDirection;
        // check if the triangle is in behind the ray
        if (oT < 0)
            return false; // the triangle is behind 

        // compute the intersection point using equation 1
        const Vec3f P = aRayOrig + oT * aRayDir;

        ////////////////////////////////
        // Step 2: inside-outside test
        ////////////////////////////////

        Vec3f C; // vector perpendicular to triangle's plane 

        // edge 0
        const Vec3f edge0 = aTriVertex1 - aTriVertex0;
        const Vec3f vp0 = P - aTriVertex0;
        C = Cross(edge0, vp0);
        const float nc = Dot(N, C);
        if (nc < -aEdgeNumThreshold)
            return false; // P is on the right side 

        // edge 1
        const Vec3f edge1 = aTriVertex2 - aTriVertex1;
        const Vec3f vp1 = P - aTriVertex1;
        C = Cross(edge1, vp1);
        oU = Dot(N, C);
        if (oU < -aEdgeNumThreshold)
            return false; // P is on the right side 

        // edge 2
        const Vec3f edge2 = aTriVertex0 - aTriVertex2;
        const Vec3f vp2 = P - aTriVertex2;
        C = Cross(edge2, vp2);
        oV = Dot(N, C);
        if (oV < -aEdgeNumThreshold)
            return false; // P is on the right side; 

        oU /= denom;
        oV /= denom;

        PG3_ASSERT_FLOAT_LESS_THAN_OR_EQUAL_TO(oU, 1.0f + aEdgeNumThreshold);
        PG3_ASSERT_FLOAT_LESS_THAN_OR_EQUAL_TO(oV, 1.0f + aEdgeNumThreshold);

        return true; // this ray hits the triangle 
    }

    Vec3f GetTrianglePoint(
        const Vec3f &aP0,
        const Vec3f &aP1,
        const Vec3f &aP2,
        const Vec2f &aBarycentricCoords)
    {
        const auto barycentricCoordZ = (1.0f - aBarycentricCoords.x - aBarycentricCoords.y);
        const Vec3f point =
              aBarycentricCoords.x * aP1
            + aBarycentricCoords.y * aP2
            + barycentricCoordZ    * aP0;
        
        return point;
    }

} // namespace Geom
