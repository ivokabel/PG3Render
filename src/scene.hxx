#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include "math.hxx"
#include "spectrum.hxx"
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

        for (size_t i=0; i<mLights.size(); i++)
            delete mLights[i];
    }

    bool Intersect(
        const Ray &aRay,
        Isect     &oResult) const
    {
        bool hit = mGeometry->Intersect(aRay, oResult);

        if (hit)
        {
            oResult.lightID = -1;
            std::map<int, int>::const_iterator it =
                mMaterial2Light.find(oResult.matID);

            if (it != mMaterial2Light.end())
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
        kLightCeiling       = 0x0001,
        kLightBox           = 0x0002,
        kLightPoint         = 0x0004,
        kLightEnv           = 0x0008,

        // geometry flags
        kSpheres            = 0x0010,
        kWalls              = 0x0020,

        // material flags
        kSpheresDiffuse     = 0x0100,
        kSpheresGlossy      = 0x0200,
        kWallsDiffuse       = 0x0400,
        kWallsGlossy        = 0x0800,

        kDefault            = (kLightCeiling | kWalls | kSpheres | kSpheresDiffuse | kWallsDiffuse),
    };

    enum EnvironmentMapType
    {
        kEMInvalid              = -1,

        kEMConstantBluish       = 0,
        kEMConstantSRGBWhite    = 1,
        kEMDebugSinglePixel     = 2,
        kEMPisa                 = 3,
        kEMGlacier              = 4,
        kEMDoge2                = 5,
        kEMPlayaSunrise         = 6,

        kEMCount,
        kEMDefault              = kEMConstantBluish,
    };

    void LoadCornellBox(
        const Vec2i &aResolution,
        uint aBoxMask = kDefault,
        uint aEnvironmentMapType = kEMDefault
        )
    {
        mSceneName = GetSceneName(aBoxMask, aEnvironmentMapType, &mSceneAcronym);

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

        //////////////////////////////////////////////////////////////////////////
        // Materials

        Material mat;
        Spectrum diffuseReflectance, glossyReflectance;

        // 0) light1, will only emit
        mMaterials.push_back(mat);
        // 1) light2, will only emit
        mMaterials.push_back(mat);

        // 2) white floor (and possibly ceiling)
        diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.803922f, 0.803922f);
        glossyReflectance.SetGreyAttenuation(0.5f);
        mat.Set(diffuseReflectance, glossyReflectance, 90, aBoxMask & kWallsDiffuse, aBoxMask & kWallsGlossy);
        mMaterials.push_back(mat);

        // 3) green left wall
        diffuseReflectance.SetSRGBAttenuation(0.156863f, 0.803922f, 0.172549f);
        glossyReflectance.SetGreyAttenuation(0.5f);
        mat.Set(diffuseReflectance, glossyReflectance, 90, aBoxMask & kWallsDiffuse, aBoxMask & kWallsGlossy);
        mMaterials.push_back(mat);

        // 4) red right wall
        diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.152941f, 0.152941f);
        glossyReflectance.SetGreyAttenuation(0.5f);
        mat.Set(diffuseReflectance, glossyReflectance, 90, aBoxMask & kWallsDiffuse, aBoxMask & kWallsGlossy);
        mMaterials.push_back(mat);

        // 5) white back wall
        diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.803922f, 0.803922f);
        glossyReflectance.SetGreyAttenuation(0.5f);
        mat.Set(diffuseReflectance, glossyReflectance, 90, aBoxMask & kWallsDiffuse, aBoxMask & kWallsGlossy);
        mMaterials.push_back(mat);

        // 6) sphere1 (yellow)
        diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.803922f, 0.152941f);
        glossyReflectance.SetGreyAttenuation(0.7f);
        mat.Set(diffuseReflectance, glossyReflectance, 200, aBoxMask & kSpheresDiffuse, aBoxMask & kSpheresGlossy);
        mMaterials.push_back(mat);

        // 7) sphere2 (blue)
        diffuseReflectance.SetSRGBAttenuation(0.152941f, 0.152941f, 0.803922f);
        glossyReflectance.SetGreyAttenuation(0.7f);
        mat.Set(diffuseReflectance, glossyReflectance, 600, aBoxMask & kSpheresDiffuse, aBoxMask & kSpheresGlossy);
        mMaterials.push_back(mat);

        delete mGeometry;

        //////////////////////////////////////////////////////////////////////////
        // Cornell box - geometry

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

        if (aBoxMask & kWalls)
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
            if (light_ceiling && !light_box)
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
        if ( aBoxMask & kSpheres )
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

        if (light_box && !light_ceiling)
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
        
        if (light_ceiling && !light_box)
        {
            // The entire ceiling is a light source

            mLights.resize(2);

            // The whole ceiling emmits 25 Watts
            const float totalPower = 25.0f; // Flux, in Wats
            Spectrum lightPower;
            lightPower.SetSRGBGreyLight(totalPower / 2.0f); // Each triangle emmits half of the flux

            AreaLight *light;
            
            light = new AreaLight(cb[2], cb[6], cb[7]);
            light->SetPower(lightPower);
            mLights[0] = light;
            mMaterial2Light.insert(std::make_pair(0, 0));

            light = new AreaLight(cb[7], cb[3], cb[2]);
            light->SetPower(lightPower);
            mLights[1] = light;
            mMaterial2Light.insert(std::make_pair(1, 1));
        }

        if (light_box && !light_ceiling)
        {
            // With light box

            mLights.resize(2);

            // The whole lightbox emmits 25 Watts
            const float totalPower = 25.0f; // Flux, in Wats
            Spectrum lightPower;
            lightPower.SetSRGBGreyLight(totalPower / 2.0f); // Each triangle emmits half of the flux

            AreaLight *light;
            
            light = new AreaLight(lb[0], lb[5], lb[4]);
            light->SetPower(lightPower);
            mLights[0] = light;
            mMaterial2Light.insert(std::make_pair(0, 0));

            light = new AreaLight(lb[5], lb[0], lb[1]);
            light->SetPower(lightPower);
            mLights[1] = light;
            mMaterial2Light.insert(std::make_pair(1, 1));
        }

        if (light_point)
        {
            PointLight *light = new PointLight(Vec3f(0.0, -0.5, 1.0));

            Spectrum lightPower;
            lightPower.SetSRGBGreyLight(50.f/*Watts*/);
            light->SetPower(lightPower);

            mLights.push_back(light);
        }

        if (light_env)
        {
            BackgroundLight *light = new BackgroundLight;

            switch (aEnvironmentMapType)
            {
                case kEMConstantBluish:
                {
                    Spectrum radiance;
                    radiance.SetSRGBLight(135 / 255.f, 206 / 255.f, 250 / 255.f);
                    light->SetConstantRadiance(radiance);
                    break;
                }

                case kEMConstantSRGBWhite:
                {
                    Spectrum radiance;
                    radiance.SetSRGBGreyLight(1.0f);
                    light->SetConstantRadiance(radiance);
                    break;
                }

                case kEMDebugSinglePixel:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\Debugging\\Single pixel.exr",
                        0.15f, 10.0f);
                        //".\\Light Probes\\Debugging\\test_exr.exr",
                        //0.15f, 1.0f);
                    break;
                }

                case kEMPisa:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\pisa.exr", 
                        0.0f, 1.0f);
                    break;
                }

                case kEMGlacier:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\glacier.exr",
                        0.1f, 1.0f);
                    break;
                }

                case kEMDoge2:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\doge2.exr",
                        0.05f, 2.0f);
                    break;
                }

                case kEMPlayaSunrise:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\hdrlabs.com\\Playa_Sunrise\\Playa_Sunrise.exr",
                        0.25f, 3.0f);
                    break;
                }

                default:
                    // Error: Undefined env map type
                    assert(false);
            }

            if (light != NULL)
            {
                mLights.push_back(light);
                mBackground = light;
            }
        }
    }

    static std::string GetEnvMapName(
        uint        aEnvironmentMapType,
        std::string *oAcronym = NULL
        )
    {
        std::string oName;

        switch (aEnvironmentMapType)
        {
        case kEMConstantBluish:
            oName = "const. bluish";
            break;

        case kEMConstantSRGBWhite:
            oName = "const. sRGB white";
            break;

        case kEMDebugSinglePixel:
            oName = "debug, single pixel, 18x6";
            break;

        case kEMPisa:
            oName = "Pisa";
            break;

        case kEMGlacier:
            oName = "glacier";
            break;

        case kEMDoge2:
            oName = "doge2";
            break;

        case kEMPlayaSunrise:
            oName = "playa sunrise";
            break;

        default:
            break;
        }

        if (oAcronym)
            *oAcronym = std::to_string(aEnvironmentMapType);

        return oName;
    }

    static std::string GetSceneName(
        uint        aBoxMask,
        uint        aEnvironmentMapType = kEMInvalid,
        std::string *oAcronym = NULL)
    {
        std::string name;
        std::string acronym;

        // Geometry

        bool geometryUsed = false;
        #define GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED \
            if (geometryUsed)      \
                name += ", ";   \
            geometryUsed = true;
        #define GEOMETRY_ADD_SPACE_IF_NEEDED \
            if (geometryUsed)      \
                name += " ";    

        if ((aBoxMask & kWalls) == kWalls)
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name += "walls";
            acronym += "w";
        }

        if ((aBoxMask & kSpheres) == kSpheres)
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "spheres";
            acronym += "s";
        }

        if ((aBoxMask & kWalls|kSpheres) == 0)
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "empty";
            acronym += "e";
        }

        GEOMETRY_ADD_SPACE_IF_NEEDED

        name    += "+ ";
        acronym += "_";

        // Light sources

        bool lightUsed = false;
        #define LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED \
            if (lightUsed)      \
                name += ", ";   \
            lightUsed = true;
        #define LIGHTS_ADD_SPACE_IF_NEEDED \
            if (lightUsed)      \
                name += " ";    

        if ((aBoxMask & kLightCeiling) == kLightCeiling)
        {
            LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "ceiling light";
            acronym += "c";
        }

        if ((aBoxMask & kLightBox) == kLightBox)
        {
            LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "light box";
            acronym += "b";
        }

        if ((aBoxMask & kLightPoint) == kLightPoint)
        {
            LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "point light";
            acronym += "p";
        }
        else if ((aBoxMask & kLightEnv) == kLightEnv)
        {
            LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED

            name    += "env. light";
            acronym += "e";

            if (aEnvironmentMapType != kEMInvalid)
            {
                std::string envMapAcronym;
                name    += " (" + GetEnvMapName(aEnvironmentMapType, &envMapAcronym) + ")";
                acronym += envMapAcronym;
            }
        }

        LIGHTS_ADD_SPACE_IF_NEEDED

        name += "+ ";
        acronym += "_";

        // Material

        bool materialUsed = false;
        #define MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED \
            if (materialUsed)      \
                name += ", ";   \
            materialUsed = true;
        #define MATERIALS_ADD_SPACE_IF_NEEDED \
            if (materialUsed)      \
                name += " ";    

        if ((aBoxMask & kSpheresDiffuse) == kSpheresDiffuse)
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. diffuse";
            acronym += "sd";
        }

        if ((aBoxMask & kSpheresGlossy) == kSpheresGlossy)
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. glossy";
            acronym += "sg";
        }

        if ((aBoxMask & kWallsDiffuse) == kWallsDiffuse)
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "walls diffuse";
            acronym += "wd";
        }

        if ((aBoxMask & kWallsGlossy) == kWallsGlossy)
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "walls glossy";
            acronym += "wg";
        }

        MATERIALS_ADD_SPACE_IF_NEEDED

        if (oAcronym) *oAcronym = acronym;
        return name;
    }

public:

    AbstractGeometry            *mGeometry;
    Camera                      mCamera;
    std::vector<Material>       mMaterials;
    std::vector<AbstractLight*> mLights;
    std::map<int, int>          mMaterial2Light;
    BackgroundLight*            mBackground;
    // SceneSphere           mSceneSphere;

    std::string           mSceneName;
    std::string           mSceneAcronym;
};
