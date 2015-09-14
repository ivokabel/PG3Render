#pragma once

#include "math.hxx"
#include "spectrum.hxx"
#include "geometry.hxx"
#include "camera.hxx"
#include "materials.hxx"
#include "lights.hxx"
#include "types.hxx"
#include "debugging.hxx"

#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <sstream>

class Scene
{
public:
    Scene() :
        mGeometry(NULL),
        mBackground(NULL),
        mBackgroundLightId(-1)
    {}

    ~Scene()
    {
        delete mGeometry;

        for (size_t i=0; i<mLights.size(); i++)
            if (mLights[i] != NULL)
                delete mLights[i];
        
         for (size_t i = 0; i<mMaterials.size(); i++)
            if (mMaterials[i] != NULL)
                delete mMaterials[i];
    }

    bool Intersect(
        const Ray &aRay,
        Isect     &oResult) const
    {
        bool hit = mGeometry->Intersect(aRay, oResult);

        if (hit)
        {
            oResult.lightID = -1;
            std::map<int32_t, int32_t>::const_iterator it =
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

    const AbstractMaterial& GetMaterial(int32_t aMaterialIdx) const
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aMaterialIdx, 0, (int32_t)(mMaterials.size() - 1));
        PG3_ASSERT(mMaterials[aMaterialIdx] != NULL);

        return *mMaterials[aMaterialIdx];
    }

    int32_t GetMaterialCount() const
    {
        return (int32_t)mMaterials.size();
    }

    const AbstractLight* GetLightPtr(int32_t aLightIdx) const
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aLightIdx, 0, (int32_t)(mLights.size() - 1));

        aLightIdx = std::min<int32_t>(aLightIdx, (int32_t)mLights.size()-1);
        return mLights[aLightIdx];
    }

    size_t GetLightCount() const
    {
        return mLights.size();
    }

    const BackgroundLight* GetBackground() const
    {
        return mBackground;
    }

    int32_t GetBackgroundLightId() const
    {
        return mBackgroundLightId;
    }

    

    //////////////////////////////////////////////////////////////////////////
    // Loads a Cornell Box scene

    enum BoxMask
    {
        // light source flags
        kLightCeiling           = 0x0001,
        kLightBox               = 0x0002,
        kLightPoint             = 0x0004,
        kLightEnv               = 0x0008,

        // geometry flags
        k2Spheres               = 0x0010,
        k1Sphere                = 0x0020,
        kWalls                  = 0x0040,
        kFloor                  = 0x0080,

        // material flags
        kSpheresPhongDiffuse    = 0x0100,
        kSpheresPhongGlossy     = 0x0200,
        kWallsPhongDiffuse      = 0x0400,
        kWallsPhongGlossy       = 0x0800,
        kSpheresFresnel         = 0x1000,

        kDefault                = (kLightCeiling | kWalls | k2Spheres | kSpheresPhongDiffuse | kWallsPhongDiffuse),
    };

    enum EnvironmentMapType
    {
        kEMInvalid                      = -1,

        kEMConstBluish                  = 0,
        kEMConstSRGBWhite               = 1,
        kEMImgConstSRGBWhite8x4         = 2,
        kEMImgConstSRGBWhite1024x512    = 3,
        kEMImgDebugSinglePixel          = 4,
        kEMImgPisa                      = 5,
        kEMImgGlacier                   = 6,
        kEMImgDoge2                     = 7,
        kEMImgPlayaSunrise              = 8,
        kEMImgEnnis                     = 9,
        kEMImgSatellite                 = 10,
        kEMImgPeaceGardensDusk          = 11,

        kEMCount,
        kEMDefault                      = kEMConstBluish,
    };

    void LoadCornellBox(
        const Vec2i &aResolution,
        uint32_t aBoxMask = kDefault,
        uint32_t aEnvironmentMapType = kEMDefault
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

        SpectrumF diffuseReflectance, glossyReflectance;

        // 0) light1, will only emit
        mMaterials.push_back(new PhongMaterial());
        // 1) light2, will only emit
        mMaterials.push_back(new PhongMaterial());

        // 2) white floor (and possibly the ceiling)
        diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.803922f, 0.803922f);
        glossyReflectance.SetGreyAttenuation(0.5f);
        mMaterials.push_back(
            new PhongMaterial(
                diffuseReflectance, glossyReflectance, 90,
                aBoxMask & kWallsPhongDiffuse, aBoxMask & kWallsPhongGlossy
                )
            );

        // 3) green left wall
        diffuseReflectance.SetSRGBAttenuation(0.156863f, 0.803922f, 0.172549f);
        glossyReflectance.SetGreyAttenuation(0.5f);
        mMaterials.push_back(
            new PhongMaterial(
                diffuseReflectance, glossyReflectance, 90,
                aBoxMask & kWallsPhongDiffuse, aBoxMask & kWallsPhongGlossy
                )
            );

        // 4) red right wall
        diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.152941f, 0.152941f);
        glossyReflectance.SetGreyAttenuation(0.5f);
        mMaterials.push_back(
            new PhongMaterial(
                diffuseReflectance, glossyReflectance, 90,
                aBoxMask & kWallsPhongDiffuse, aBoxMask & kWallsPhongGlossy
                )
            );

        // 5) white back wall
        diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.803922f, 0.803922f);
        glossyReflectance.SetGreyAttenuation(0.5f);
        mMaterials.push_back(
            new PhongMaterial(
                diffuseReflectance, glossyReflectance, 90,
                aBoxMask & kWallsPhongDiffuse, aBoxMask & kWallsPhongGlossy
                )
            );

        // 6) sphere1 (yellow)
        diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.803922f, 0.152941f);
        glossyReflectance.SetGreyAttenuation(0.7f);
        if (!IS_MASKED(aBoxMask, kSpheresFresnel))
            mMaterials.push_back(
                new PhongMaterial(
                    diffuseReflectance, glossyReflectance, 200,
                    aBoxMask & kSpheresPhongDiffuse, aBoxMask & kSpheresPhongGlossy)
                );
        else
            mMaterials.push_back(new FresnelMaterial());

        // 7) sphere2 (blue)
        diffuseReflectance.SetSRGBAttenuation(0.152941f, 0.152941f, 0.803922f);
        glossyReflectance.SetGreyAttenuation(0.7f);
        if (!IS_MASKED(aBoxMask, kSpheresFresnel))
            mMaterials.push_back(
                new PhongMaterial(
                    diffuseReflectance, glossyReflectance, 600,
                    aBoxMask & kSpheresPhongDiffuse, aBoxMask & kSpheresPhongGlossy)
                );
        else
            mMaterials.push_back(new FresnelMaterial());

        // 8) large sphere (white)
        diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.803922f, 0.803922f);
        glossyReflectance.SetGreyAttenuation(0.5f);
        if (!IS_MASKED(aBoxMask, kSpheresFresnel))
            mMaterials.push_back(
                new PhongMaterial(
                    diffuseReflectance, glossyReflectance, 90,
                    aBoxMask & kSpheresPhongDiffuse, aBoxMask & kSpheresPhongGlossy)
                );
        else
            mMaterials.push_back(new FresnelMaterial());

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
        if (aBoxMask & kFloor)
        {
            geometryList->mGeometry.push_back(new Triangle(cb[0], cb[4], cb[5], 2));
            geometryList->mGeometry.push_back(new Triangle(cb[5], cb[1], cb[0], 2));
        }

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
        if ( aBoxMask & k2Spheres )
        {
            float ballRadius = 0.5f;
            Vec3f leftWallCenter  = (cb[0] + cb[4]) * (1.f / 2.f) + Vec3f(0, 0, ballRadius);
            Vec3f rightWallCenter = (cb[1] + cb[5]) * (1.f / 2.f) + Vec3f(0, 0, ballRadius);
            float xlen = rightWallCenter.x - leftWallCenter.x;
            Vec3f leftBallCenter  = leftWallCenter  + Vec3f(2.f * xlen / 7.f, 0, 0);
            Vec3f rightBallCenter = rightWallCenter - Vec3f(2.f * xlen / 7.f, -xlen/4, 0);

            geometryList->mGeometry.push_back(new Sphere(leftBallCenter,  ballRadius, 6));
            geometryList->mGeometry.push_back(new Sphere(rightBallCenter, ballRadius, 7));
        }
        if ( aBoxMask & k1Sphere )
        {
            float ballRadius = 1.0f;
            Vec3f floorCenter = (cb[0] + cb[5]) * (1.f / 2.f);
            Vec3f ballCenter = floorCenter + Vec3f(0.f, 0.f, ballRadius);

            geometryList->mGeometry.push_back(new Sphere(ballCenter, ballRadius, 8));
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
            SpectrumF lightPower;
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
            SpectrumF lightPower;
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

            SpectrumF lightPower;
            lightPower.SetSRGBGreyLight(50.f/*Watts*/);
            light->SetPower(lightPower);

            mLights.push_back(light);
        }

        if (light_env)
        {
            BackgroundLight *light = new BackgroundLight;

            switch (aEnvironmentMapType)
            {
                case kEMConstBluish:
                {
                    SpectrumF radiance;
                    radiance.SetSRGBLight(135 / 255.f, 206 / 255.f, 250 / 255.f);
                    light->SetConstantRadiance(radiance);
                    break;
                }

                case kEMConstSRGBWhite:
                {
                    SpectrumF radiance;
                    radiance.SetSRGBGreyLight(1.0f);
                    light->SetConstantRadiance(radiance);
                    break;
                }

                case kEMImgConstSRGBWhite8x4:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\Debugging\\Const white 8x4.exr");
                    break;
                }

                case kEMImgConstSRGBWhite1024x512:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\Debugging\\Const white 1024x512.exr");
                    break;
                }

                case kEMImgDebugSinglePixel:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\Debugging\\Single pixel.exr",
                        0.35f, 10.0f);
                    break;
                }

                case kEMImgPisa:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\pisa.exr", 
                        0.05f, 1.0f);
                    break;
                }

                case kEMImgGlacier:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\glacier.exr",
                        0.05f, 1.0f);
                    break;
                }

                case kEMImgDoge2:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\doge2.exr",
                        0.83f, 1.5f);
                    break;
                }

                case kEMImgPlayaSunrise:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\hdrlabs.com\\Playa_Sunrise\\Playa_Sunrise.exr",
                        0.1f, 2.0f);
                    break;
                }

                case kEMImgEnnis:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\ennis.exr",
                        0.53f, 0.2f);
                    break;
                }

                case kEMImgSatellite:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\hdr-sets.com\\HDR_SETS_SATELLITE_01_FREE\\107_ENV_DOMELIGHT.exr",
                        -0.12f, 0.5f);
                    break;
                }

                case kEMImgPeaceGardensDusk:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\panocapture.com\\PeaceGardens_Dusk.exr",
                        -0.12f, 0.5f);
                    break;
                }

                default:
                    PG3_FATAL_ERROR("Unknown environment map type");
            }

            if (light != NULL)
            {
                mLights.push_back(light);
                mBackground = light;
                mBackgroundLightId = (int32_t)(mLights.size() - 1);
            }
        }
    }

    static std::string GetEnvMapName(
        uint32_t     aEnvironmentMapType,
        std::string *oAcronym = NULL
        )
    {
        std::string oName;

        switch (aEnvironmentMapType)
        {
        case kEMConstBluish:
            oName = "const. bluish";
            break;

        case kEMConstSRGBWhite:
            oName = "const. sRGB white";
            break;

        case kEMImgConstSRGBWhite8x4:
            oName = "debug, white, 8x4";
            break;

        case kEMImgConstSRGBWhite1024x512:
            oName = "debug, white, 1024x512";
            break;

        case kEMImgDebugSinglePixel:
            oName = "debug, single pixel, 18x6";
            break;

        case kEMImgPisa:
            oName = "Pisa";
            break;

        case kEMImgGlacier:
            oName = "glacier";
            break;

        case kEMImgDoge2:
            oName = "Doge2";
            break;

        case kEMImgPlayaSunrise:
            oName = "playa sunrise";
            break;

        case kEMImgEnnis:
            oName = "ennis";
            break;

        case kEMImgSatellite:
            oName = "sattelite";
            break;

        case kEMImgPeaceGardensDusk:
            oName = "Peace Gardens - Dusk";
            break;

        default:
            PG3_FATAL_ERROR("Unknown environment map type");
        }

        if (oAcronym)
        {
            *oAcronym = std::to_string(aEnvironmentMapType);
            //std::ostringstream outStream;
            //outStream.width(2);
            //outStream.fill('0');
            //outStream << aEnvironmentMapType;
            //*oAcronym = outStream.str();
        }

        return oName;
    }

    static std::string GetSceneName(
        uint32_t     aBoxMask,
        uint32_t     aEnvironmentMapType = kEMInvalid,
        std::string *oAcronym = NULL)
    {
        std::string name;
        std::string acronym;

        name += "[";

        ///////////////////////////////////////////////////////////////////////////////////////////
        // Geometry
        ///////////////////////////////////////////////////////////////////////////////////////////

        bool geometryUsed = false;
        #define GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED \
            if (geometryUsed)      \
                name += ", ";   \
            geometryUsed = true;
        #define GEOMETRY_ADD_SPACE_IF_NEEDED \
            if (geometryUsed)      \
                name += " ";    

        if (IS_MASKED(aBoxMask, kWalls))
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name += "walls";
            acronym += "w";
        }

        if (IS_MASKED(aBoxMask, k2Spheres))
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "2 spheres";
            acronym += "2s";
        }

        if (IS_MASKED(aBoxMask, k1Sphere))
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "1 sphere";
            acronym += "1s";
        }

        if ((aBoxMask & kWalls|k2Spheres|k1Sphere) == 0)
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "empty";
            acronym += "e";
        }

        //GEOMETRY_ADD_SPACE_IF_NEEDED
        name    += "] + [";
        acronym += "_";

        ///////////////////////////////////////////////////////////////////////////////////////////
        // Light sources
        ///////////////////////////////////////////////////////////////////////////////////////////

        bool lightUsed = false;
        #define LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED \
            if (lightUsed)      \
                name += ", ";   \
            lightUsed = true;
        #define LIGHTS_ADD_SPACE_IF_NEEDED \
            if (lightUsed)      \
                name += " ";    

        if (IS_MASKED(aBoxMask, kLightCeiling))
        {
            LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "ceiling light";
            acronym += "c";
        }

        if (IS_MASKED(aBoxMask, kLightBox))
        {
            LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "light box";
            acronym += "b";
        }

        if (IS_MASKED(aBoxMask, kLightPoint))
        {
            LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "point light";
            acronym += "p";
        }
        
        if (IS_MASKED(aBoxMask, kLightEnv))
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

        //LIGHTS_ADD_SPACE_IF_NEEDED
        name    += "] + [";
        acronym += "_";

        ///////////////////////////////////////////////////////////////////////////////////////////
        // Materials
        ///////////////////////////////////////////////////////////////////////////////////////////

        bool materialUsed = false;
        #define MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED \
            if (materialUsed)      \
                name += ", ";   \
            materialUsed = true;
        #define MATERIALS_ADD_SPACE_IF_NEEDED \
            if (materialUsed)      \
                name += " ";    

        if (IS_MASKED(aBoxMask, kSpheresPhongDiffuse) ||
            IS_MASKED(aBoxMask, kSpheresPhongGlossy))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. Phong";
            acronym += "Sp";

            if (IS_MASKED(aBoxMask, kSpheresPhongDiffuse))
            {
                name    += " diffuse";
                acronym += "d";
            }

            if (IS_MASKED(aBoxMask, kSpheresPhongGlossy))
            {
                name    += " glossy";
                acronym += "g";
            }
        }
        else if (IS_MASKED(aBoxMask, kSpheresFresnel))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. ideal mirror";
            acronym += "Sim";
        }

        if (IS_MASKED(aBoxMask, kWallsPhongDiffuse) ||
            IS_MASKED(aBoxMask, kWallsPhongGlossy))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "walls Phong";
            acronym += "Wp";

            if (IS_MASKED(aBoxMask, kWallsPhongDiffuse))
            {
                name    += " diffuse";
                acronym += "d";
            }

            if (IS_MASKED(aBoxMask, kWallsPhongGlossy))
            {
                name    += " glossy";
                acronym += "g";
            }
        }

        //MATERIALS_ADD_SPACE_IF_NEEDED
        name += "]";

        if (oAcronym) *oAcronym = acronym;
        return name;
    }

public:

    AbstractGeometry                *mGeometry;
    Camera                           mCamera;
    std::vector<AbstractMaterial*>   mMaterials;
    std::vector<AbstractLight*>      mLights;
    std::map<int32_t, int32_t>       mMaterial2Light;
    BackgroundLight*                 mBackground;
    int32_t                          mBackgroundLightId;

    std::string                      mSceneName;
    std::string                      mSceneAcronym;
};
