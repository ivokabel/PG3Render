#pragma once

#include <vector>
#include <cmath>
#include "math.hxx"

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

    float dist;    //!< Distance to closest intersection (serves as ray.tmax)
    int   matID;   //!< ID of intersected material
    int   lightID; //!< ID of intersected light (if < 0, then none)
    Vec3f normal;  //!< Normal at the intersection
};
