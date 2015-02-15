#pragma once

#include "math.hxx"
#include "ray.hxx"
#include "types.hxx"

#include <vector>
#include <cmath>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
// Geometry

class AbstractGeometry
{
public:

    virtual ~AbstractGeometry(){};

    // Finds the closest intersection
    virtual bool Intersect (const Ray& aRay, Isect& oResult) const = 0;

    // Finds any intersection, default calls Intersect
    virtual bool IntersectP(const Ray& aRay, Isect& oResult) const
    {
        return Intersect(aRay, oResult);
    }

    // Grows given bounding box by this object
    virtual void GrowBBox(Vec3f &aoBBoxMin, Vec3f &aoBBoxMax) = 0;
};

class GeometryList : public AbstractGeometry
{
public:

    virtual ~GeometryList()
    {
        for (uint32_t i=0; i<mGeometry.size(); i++)
            delete mGeometry[i];
    };

    virtual bool Intersect(const Ray& aRay, Isect& oResult) const
    {
        bool anyIntersection = false;

        for (uint32_t i=0; i<mGeometry.size(); i++)
        {
            bool hit = mGeometry[i]->Intersect(aRay, oResult);

            if (hit)
                anyIntersection = true;
        }

        return anyIntersection;
    }

    virtual bool IntersectP(
        const Ray &aRay,
        Isect     &oResult) const
    {
        for (uint32_t i=0; i<mGeometry.size(); i++)
        {
            if (mGeometry[i]->IntersectP(aRay, oResult))
                return true;
        }

        return false;
    }

    virtual void GrowBBox(
        Vec3f &aoBBoxMin,
        Vec3f &aoBBoxMax)
    {
        for (uint32_t i=0; i<mGeometry.size(); i++)
            mGeometry[i]->GrowBBox(aoBBoxMin, aoBBoxMax);
    }

public:

    std::vector<AbstractGeometry*> mGeometry;
};

class Triangle : public AbstractGeometry
{
public:

    Triangle(){}

    Triangle(
        const Vec3f &p0,
        const Vec3f &p1,
        const Vec3f &p2,
        int32_t      aMatID)
    {
        p[0] = p0;
        p[1] = p1;
        p[2] = p2;
        matID = aMatID;
        mNormal = Normalize(Cross(p[1] - p[0], p[2] - p[0]));
    }

    virtual bool Intersect(
        const Ray &aRay,
        Isect     &oResult) const
    {
        const Vec3f ao = p[0] - aRay.org;
        const Vec3f bo = p[1] - aRay.org;
        const Vec3f co = p[2] - aRay.org;

        const Vec3f v0 = Cross(co, bo);
        const Vec3f v1 = Cross(bo, ao);
        const Vec3f v2 = Cross(ao, co);

        const float v0d = Dot(v0, aRay.dir);
        const float v1d = Dot(v1, aRay.dir);
        const float v2d = Dot(v2, aRay.dir);

        if (((v0d < 0.f)  && (v1d < 0.f)  && (v2d < 0.f)) ||
            ((v0d >= 0.f) && (v1d >= 0.f) && (v2d >= 0.f)))
        {
            const float distance = Dot(mNormal, ao) / Dot(mNormal, aRay.dir);

            if ((distance > aRay.tmin) & (distance < oResult.dist))
            {
                oResult.normal = mNormal;
                oResult.matID  = matID;
                oResult.dist   = distance;
                return true;
            }
        }

        return false;
    }

    virtual void GrowBBox(
        Vec3f &aoBBoxMin,
        Vec3f &aoBBoxMax)
    {
        for (uint32_t i=0; i<3; i++)
        {
            for (uint32_t j=0; j<3; j++)
            {
                aoBBoxMin.Get(j) = std::min(aoBBoxMin.Get(j), p[i].Get(j));
                aoBBoxMax.Get(j) = std::max(aoBBoxMax.Get(j), p[i].Get(j));
            }
        }
    }

public:

    Vec3f   p[3];
    int32_t matID;
    Vec3f   mNormal;
};

class Sphere : public AbstractGeometry
{
public:

    Sphere(){}
    
    Sphere(
        const Vec3f &aCenter,
        float        aRadius,
        int32_t      aMatID)
    {
        center = aCenter;
        radius = aRadius;
        matID  = aMatID;
    }

    // Taken from:
    // http://wiki.cgsociety.org/index.php/Ray_Sphere_Intersection

    virtual bool Intersect(
        const Ray &aRay,
        Isect     &oResult) const
    {
        // we transform ray origin into object space (center == origin)
        const Vec3f transformedOrigin = aRay.org - center;

        const float A = Dot(aRay.dir, aRay.dir);
        const float B = 2 * Dot(aRay.dir, transformedOrigin);
        const float C = Dot(transformedOrigin, transformedOrigin) - (radius * radius);

        // Must use doubles, because when B ~ sqrt(B*B - 4*A*C)
        // the resulting t is imprecise enough to get around ray epsilons
        const double disc = B*B - 4*A*C;

        if (disc < 0)
            return false;

        const double discSqrt = std::sqrt(disc);
        const double q = (B < 0) ? ((-B - discSqrt) / 2.f) : ((-B + discSqrt) / 2.f);

        double t0 = q / A;
        double t1 = C / q;

        if (t0 > t1) std::swap(t0, t1);

        float resT;

        if (t0 > aRay.tmin && t0 < oResult.dist)
            resT = float(t0);
        else if (t1 > aRay.tmin && t1 < oResult.dist)
            resT = float(t1);
        else
            return false;

        oResult.dist   = resT;
        oResult.matID  = matID;
        oResult.normal = Normalize(transformedOrigin + Vec3f(resT) * aRay.dir);
        return true;
    }

    virtual void GrowBBox(
        Vec3f &aoBBoxMin,
        Vec3f &aoBBoxMax)
    {
        for (uint32_t i=0; i<8; i++)
        {
            Vec3f p = center;
            Vec3f half(radius);

            for (uint32_t j=0; j<3; j++)
                if (i & (1 << j)) half.Get(j) = -half.Get(j);
            
            p += half;

            for (uint32_t j=0; j<3; j++)
            {
                aoBBoxMin.Get(j) = std::min(aoBBoxMin.Get(j), p.Get(j));
                aoBBoxMax.Get(j) = std::max(aoBBoxMax.Get(j), p.Get(j));
            }
        }
    }

public:

    Vec3f   center;
    float   radius;
    int32_t matID;
};
