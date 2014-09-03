#pragma once

#include <vector>
#include <map>
#include <cmath>
#include "math.hxx"
#include "geometry.hxx"
#include "camera.hxx"
#include "materials.hxx"
#include "lights.hxx"

class Scene
{
public:
    Scene() :
        mGeometry(NULL),
        mBackground(NULL)
    {}

    ~Scene()
    {
        delete mGeometry;

        for(size_t i=0; i<mLights.size(); i++)
            delete mLights[i];
    }

    bool Intersect(
        const Ray &aRay,
        Isect     &oResult) const
    {
        bool hit = mGeometry->Intersect(aRay, oResult);

        if(hit)
        {
            oResult.lightID = -1;
            std::map<int, int>::const_iterator it =
                mMaterial2Light.find(oResult.matID);

            if(it != mMaterial2Light.end())
                oResult.lightID = it->second;
        }

        return hit;
    }

    bool Occluded(
        const Vec3f &aPoint,
        const Vec3f &aDir,
        float aTMax) const
    {
        Ray ray;
        ray.org  = aPoint + aDir * EPS_RAY;
        ray.dir  = aDir;
        ray.tmin = 0;
        Isect isect;
        isect.dist = aTMax - 2*EPS_RAY;

        return mGeometry->IntersectP(ray, isect);
    }

    const Material& GetMaterial(const int aMaterialIdx) const
    {
        return mMaterials[aMaterialIdx];
    }

    int GetMaterialCount() const
    {
        return (int)mMaterials.size();
    }


    const AbstractLight* GetLightPtr(int aLightIdx) const
    {
        aLightIdx = std::min<int>(aLightIdx, (int)mLights.size()-1);
        return mLights[aLightIdx];
    }

    int GetLightCount() const
    {
        return (int)mLights.size();
    }

    const BackgroundLight* GetBackground() const
    {
        return mBackground;
    }

    //////////////////////////////////////////////////////////////////////////
    // Loads a Cornell Box scene
    enum BoxMask
    {
		// light source flags
        kLightCeiling      = 1,
		kLightBox          = 2,
        kLightPoint        = 4,
        kLightEnv          = 8,
		// geometry flags
        kSpheres           = 64,
        kWalls             = 256,
		// material flags
		kSpheresDiffuse    = 512,
		kSpheresGlossy     = 1024,
		kWallsDiffuse      = 2048,
		kWallsGlossy       = 4096,
        kDefault           = (kLightCeiling | kWalls | kSpheres | kSpheresDiffuse | kWallsDiffuse ),
    };

	inline void SetMaterial(
		Material &aMat, 
		const Vec3f& aDiffuseReflectance, 
		const Vec3f& aGlossyReflectance, 
		float aPhongExponent, 
		uint aDiffuse, 
		uint aGlossy)
	{
		aMat.Reset();
        aMat.mDiffuseReflectance = aDiffuse ? aDiffuseReflectance : Vec3f(0);
        aMat.mPhongReflectance   = aGlossy  ? aGlossyReflectance  : Vec3f(0);
        aMat.mPhongExponent      = aPhongExponent;
		if( aGlossy ) 
			aMat.mDiffuseReflectance /= 2; // to make it energy conserving
	}

    void LoadCornellBox(
        const Vec2i &aResolution,
        uint aBoxMask = kDefault)
    {
        mSceneName = GetSceneName(aBoxMask, &mSceneAcronym);

        bool light_ceiling = (aBoxMask & kLightCeiling)    != 0;
        bool light_box     = (aBoxMask & kLightBox)        != 0;
        bool light_point   = (aBoxMask & kLightPoint)      != 0;
        bool light_env     = (aBoxMask & kLightEnv)        != 0;

        // Camera
        mCamera.Setup(
            Vec3f(-0.0439815f, -4.12529f, 0.222539f),
            Vec3f(0.00688625f, 0.998505f, -0.0542161f),
            Vec3f(3.73896e-4f, 0.0542148f, 0.998529f),
            Vec2f(float(aResolution.x), float(aResolution.y)), 45);

        // Materials
        Material mat;
        // 0) light1, will only emit
        mMaterials.push_back(mat);
        // 1) light2, will only emit
        mMaterials.push_back(mat);

        // 2) white floor (and possibly ceiling)
		SetMaterial(mat, Vec3f(0.803922f, 0.803922f, 0.803922f), Vec3f(0.5f), 90, aBoxMask & kWallsDiffuse, aBoxMask & kWallsGlossy);
        mMaterials.push_back(mat);

        // 3) green left wall
		SetMaterial(mat, Vec3f(0.156863f, 0.803922f, 0.172549f), Vec3f(0.5f), 90, aBoxMask & kWallsDiffuse, aBoxMask & kWallsGlossy);
        mMaterials.push_back(mat);

        // 4) red right wall
		SetMaterial(mat, Vec3f(0.803922f, 0.152941f, 0.152941f), Vec3f(0.5f), 90, aBoxMask & kWallsDiffuse, aBoxMask & kWallsGlossy);
        mMaterials.push_back(mat);

        // 5) white back wall
		SetMaterial(mat, Vec3f(0.803922f, 0.803922f, 0.803922f), Vec3f(0.5f), 90, aBoxMask & kWallsDiffuse, aBoxMask & kWallsGlossy);
        mMaterials.push_back(mat);

        // 6) sphere1 (yellow)
		SetMaterial(mat, Vec3f(0.803922f, 0.803922f, 0.152941f), Vec3f(0.7f), 200, aBoxMask & kSpheresDiffuse, aBoxMask & kSpheresGlossy);
        mMaterials.push_back(mat);

		// 7) sphere2 (blue)
		SetMaterial(mat, Vec3f(0.152941f, 0.152941f, 0.803922f), Vec3f(0.7f), 600, aBoxMask & kSpheresDiffuse, aBoxMask & kSpheresGlossy);
        mMaterials.push_back(mat);

        delete mGeometry;

        //////////////////////////////////////////////////////////////////////////
        // Cornell box
        Vec3f cb[8] = {
            Vec3f(-1.27029f,  1.30455f, -1.28002f),
            Vec3f( 1.28975f,  1.30455f, -1.28002f),
            Vec3f( 1.28975f,  1.30455f,  1.28002f),
            Vec3f(-1.27029f,  1.30455f,  1.28002f),
            Vec3f(-1.27029f, -1.25549f, -1.28002f),
            Vec3f( 1.28975f, -1.25549f, -1.28002f),
            Vec3f( 1.28975f, -1.25549f,  1.28002f),
            Vec3f(-1.27029f, -1.25549f,  1.28002f)
        };

        GeometryList *geometryList = new GeometryList;
        mGeometry = geometryList;

		// Floor
		geometryList->mGeometry.push_back(new Triangle(cb[0], cb[4], cb[5], 2));
		geometryList->mGeometry.push_back(new Triangle(cb[5], cb[1], cb[0], 2));

		if(aBoxMask & kWalls)
		{
			// Left wall
			geometryList->mGeometry.push_back(new Triangle(cb[3], cb[7], cb[4], 3));
			geometryList->mGeometry.push_back(new Triangle(cb[4], cb[0], cb[3], 3));

			// Right wall
			geometryList->mGeometry.push_back(new Triangle(cb[1], cb[5], cb[6], 4));
			geometryList->mGeometry.push_back(new Triangle(cb[6], cb[2], cb[1], 4));

			// Back wall
			geometryList->mGeometry.push_back(new Triangle(cb[0], cb[1], cb[2], 5));
			geometryList->mGeometry.push_back(new Triangle(cb[2], cb[3], cb[0], 5));


			// Ceiling
			if(light_ceiling && !light_box)
			{
				geometryList->mGeometry.push_back(new Triangle(cb[2], cb[6], cb[7], 0));
				geometryList->mGeometry.push_back(new Triangle(cb[7], cb[3], cb[2], 1));
			}
			else
			{
				geometryList->mGeometry.push_back(new Triangle(cb[2], cb[6], cb[7], 2));
				geometryList->mGeometry.push_back(new Triangle(cb[7], cb[3], cb[2], 2));
			}
		}

		// Spheres
		if( aBoxMask & kSpheres )
		{

			float smallRadius = 0.5f;
			Vec3f leftWallCenter  = (cb[0] + cb[4]) * (1.f / 2.f) + Vec3f(0, 0, smallRadius);
			Vec3f rightWallCenter = (cb[1] + cb[5]) * (1.f / 2.f) + Vec3f(0, 0, smallRadius);
			float xlen = rightWallCenter.x - leftWallCenter.x;
			Vec3f leftBallCenter  = leftWallCenter  + Vec3f(2.f * xlen / 7.f, 0, 0);
			Vec3f rightBallCenter = rightWallCenter - Vec3f(2.f * xlen / 7.f, -xlen/4, 0);

			geometryList->mGeometry.push_back(new Sphere(leftBallCenter,  smallRadius, 6));
			geometryList->mGeometry.push_back(new Sphere(rightBallCenter, smallRadius, 7));
		}

        //////////////////////////////////////////////////////////////////////////
        // Light box at the ceiling
        Vec3f lb[8] = {
            Vec3f(-0.25f,  0.25f, 1.26002f),
            Vec3f( 0.25f,  0.25f, 1.26002f),
            Vec3f( 0.25f,  0.25f, 1.28002f),
            Vec3f(-0.25f,  0.25f, 1.28002f),
            Vec3f(-0.25f, -0.25f, 1.26002f),
            Vec3f( 0.25f, -0.25f, 1.26002f),
            Vec3f( 0.25f, -0.25f, 1.28002f),
            Vec3f(-0.25f, -0.25f, 1.28002f)
        };

        if(light_box && !light_ceiling)
        {
            // Back wall
            geometryList->mGeometry.push_back(new Triangle(lb[0], lb[2], lb[1], 5));
            geometryList->mGeometry.push_back(new Triangle(lb[2], lb[0], lb[3], 5));
            // Left wall
            geometryList->mGeometry.push_back(new Triangle(lb[3], lb[4], lb[7], 5));
            geometryList->mGeometry.push_back(new Triangle(lb[4], lb[3], lb[0], 5));
            // Right wall
            geometryList->mGeometry.push_back(new Triangle(lb[1], lb[6], lb[5], 5));
            geometryList->mGeometry.push_back(new Triangle(lb[6], lb[1], lb[2], 5));
            // Front wall
            geometryList->mGeometry.push_back(new Triangle(lb[4], lb[5], lb[6], 5));
            geometryList->mGeometry.push_back(new Triangle(lb[6], lb[7], lb[4], 5));
			// Floor
			geometryList->mGeometry.push_back(new Triangle(lb[0], lb[5], lb[4], 0));
			geometryList->mGeometry.push_back(new Triangle(lb[5], lb[0], lb[1], 1));
        }

        //////////////////////////////////////////////////////////////////////////
        // Lights
        
		if(light_ceiling && !light_box)
        {
            // entire ceiling is a light source
            mLights.resize(2);
            AreaLight *l = new AreaLight(cb[2], cb[6], cb[7]);
            l->mRadiance = Vec3f(1.21f);
            mLights[0] = l;
            mMaterial2Light.insert(std::make_pair(0, 0));

            l = new AreaLight(cb[7], cb[3], cb[2]);
            l->mRadiance = Vec3f(1.21f);
            mLights[1] = l;
            mMaterial2Light.insert(std::make_pair(1, 1));
        }

        if(light_box && !light_ceiling)
        {
            // With light box
            mLights.resize(2);
            AreaLight *l = new AreaLight(lb[0], lb[5], lb[4]);
            l->mRadiance = Vec3f(31.831f); // 25 Watts
            mLights[0] = l;
            mMaterial2Light.insert(std::make_pair(0, 0));

            l = new AreaLight(lb[5], lb[0], lb[1]);
            l->mRadiance = Vec3f(31.831f); // 25 Watts
            mLights[1] = l;
            mMaterial2Light.insert(std::make_pair(1, 1));
        }

        if(light_point)
        {
            PointLight *l = new PointLight(Vec3f(0.0, -0.5, 1.0));
            l->mIntensity = Vec3f( 50.f/*Watts*/ / (4*PI_F) );
            mLights.push_back(l);
        }

        if(light_env)
        {
            BackgroundLight *l = new BackgroundLight;
            mLights.push_back(l);
            mBackground = l;
        }
    }

    static std::string GetSceneName(
        uint        aBoxMask,
        std::string *oAcronym = NULL)
    {
        std::string name;
        std::string acronym;

        // Geometry
        if((aBoxMask & kWalls) == kWalls)
        {
            name    += "walls ";
            acronym += "w";
        }

        if((aBoxMask & kSpheres) == kSpheres)
        {
            name    += "spheres ";
            acronym += "s";
        }

		if((aBoxMask & kWalls|kSpheres) == 0)
        {
            name    += "empty ";
            acronym += "e";
        }

		name    += "+ ";
        acronym += "_";

        // Light sources
        if((aBoxMask & kLightCeiling) == kLightCeiling)
        {
            name    += "ceiling light ";
            acronym += "c";
        }

        if((aBoxMask & kLightBox) == kLightBox)
        {
            name    += "light box ";
            acronym += "b";
        }

        if((aBoxMask & kLightPoint) == kLightPoint)
        {
            name    += "point light ";
            acronym += "p";
        }
        else if((aBoxMask & kLightEnv) == kLightEnv)
        {
            name    += "env. light ";
            acronym += "e";
        }

		name    += "+ ";
        acronym += "_";

		// Material
        if((aBoxMask & kSpheresDiffuse) == kSpheresDiffuse)
        {
            name    += "sph. diffuse ";
            acronym += "sd";
        }

		if((aBoxMask & kSpheresGlossy) == kSpheresGlossy)
        {
            name    += "sph. glossy ";
            acronym += "sg";
        }

        if((aBoxMask & kWallsDiffuse) == kWallsDiffuse)
        {
            name    += "walls diffuse ";
            acronym += "wd";
        }

		if((aBoxMask & kWallsGlossy) == kWallsGlossy)
        {
            name    += "walls glossy ";
            acronym += "wg";
        }

        if(oAcronym) *oAcronym = acronym;
        return name;
    }

public:

    AbstractGeometry      *mGeometry;
    Camera                mCamera;
    std::vector<Material> mMaterials;
    std::vector<AbstractLight*>   mLights;
    std::map<int, int>    mMaterial2Light;
    // SceneSphere           mSceneSphere;
    BackgroundLight*      mBackground;

    std::string           mSceneName;
    std::string           mSceneAcronym;
};
