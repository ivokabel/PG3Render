#pragma once

#include <vector>
#include <cmath>
#include "math.hxx"
#include "types.hxx"

//////////////////////////////////////////////////////////////////////////
// Ray casting

struct Ray
{
    Ray()
    {}

    Ray(const Vec3f& aOrg,
        const Vec3f& aDir,
        float aTMin
    ) :
        org(aOrg),
        dir(aDir),
        tmin(aTMin)
    {}

    Vec3f PointAt(float aT) const
    {
        return org + dir * aT;
    }

    Vec3f org;  //!< Ray origin
    Vec3f dir;  //!< Ray direction
    float tmin; //!< Minimal distance to intersection
};

struct Isect
{
    Isect()
    {}

    Isect(float aMaxDist):dist(aMaxDist)
    {}

    float       dist;    //!< Distance to closest intersection (serves as ray.tmax)
    int32_t     matID;   //!< ID of intersected material
    int32_t     lightID; //!< ID of intersected light (if < 0, then none)
    Vec3f       normal;  //!< Normal at the intersection
};
