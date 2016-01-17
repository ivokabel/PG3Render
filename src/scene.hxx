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
        kLightCeiling                       = 0x00000001,
        kLightBox                           = 0x00000002,
        kLightPoint                         = 0x00000004,
        kLightEnv                           = 0x00000008,

        // geometry flags
        k2Spheres                           = 0x00000010,
        k1Sphere                            = 0x00000020,   // large sphere in the middle
        kVerticalRectangle                  = 0x00000040,   // large vertical rectangle in the front
        kDiagonalRectangles                 = 0x00000080,   // rectangle from front floort edge to back ceiling edge
        kWalls                              = 0x00000100,
        kFloor                              = 0x00000200,
        kAllGeometry                        = 0x00000ff0,

        // material flags
        kSpheresPhongDiffuse                = 0x00001000,
        kSpheresPhongGlossy                 = 0x00002000,
        kWallsPhongDiffuse                  = 0x00004000,
        kWallsPhongGlossy                   = 0x00008000,
        kSpheresFresnelConductor            = 0x00010000,
        kSpheresFresnelDielectric           = 0x00020000,
        kSpheresMicrofacetGGXConductor      = 0x00040000,
        kSpheresMicrofacetGGXDielectric     = 0x00080000,
        kVertRectFresnelDielectric          = 0x00100000,
        kVertRectMicrofacetGGXDielectric    = 0x00200000,

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
        kEMImgSynthThreePointLighting   = 12,

        kEMCount,
        kEMDefault                      = kEMConstBluish,
    };

    void LoadCornellBox(
        const Vec2i &aResolution,
        uint32_t aBoxMask = kDefault,
        uint32_t aEnvironmentMapType = kEMDefault,
        float aDbgAux1 = INFINITY_F,
        float aDbgAux2 = INFINITY_F
        )
    {
        aDbgAux1, aDbgAux2; // possibly unused parameters

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
            Vec2f(float(aResolution.x), float(aResolution.y)),
            45);
            //135); // debug

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
        if (IS_MASKED(aBoxMask, kSpheresFresnelConductor))
            mMaterials.push_back(
                new SmoothConductorMaterial(MAT_COPPER_IOR, MAT_AIR_IOR, MAT_COPPER_ABSORBANCE));
        else if (IS_MASKED(aBoxMask, kSpheresMicrofacetGGXConductor))
            mMaterials.push_back(
                new MicrofacetGGXConductorMaterial(0.20f, MAT_COPPER_IOR, MAT_AIR_IOR, MAT_COPPER_ABSORBANCE));
        else
        {
            diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.803922f, 0.152941f);
            glossyReflectance.SetGreyAttenuation(0.7f);
            mMaterials.push_back(
                new PhongMaterial(
                    diffuseReflectance, glossyReflectance, 200,
                    aBoxMask & kSpheresPhongDiffuse, aBoxMask & kSpheresPhongGlossy)
                );
        }

        // 7) sphere2 (blue)
        if (IS_MASKED(aBoxMask, kSpheresFresnelConductor))
            mMaterials.push_back(
                new SmoothConductorMaterial(MAT_SILVER_IOR, MAT_AIR_IOR, MAT_SILVER_ABSORBANCE));
        else if (IS_MASKED(aBoxMask, kSpheresMicrofacetGGXConductor))
            mMaterials.push_back(
                new MicrofacetGGXConductorMaterial(0.20f, MAT_COPPER_IOR, MAT_AIR_IOR, MAT_COPPER_ABSORBANCE));
        else
        {
            diffuseReflectance.SetSRGBAttenuation(0.152941f, 0.152941f, 0.803922f);
            glossyReflectance.SetGreyAttenuation(0.7f);
            mMaterials.push_back(
                new PhongMaterial(
                    diffuseReflectance, glossyReflectance, 600,
                    aBoxMask & kSpheresPhongDiffuse, aBoxMask & kSpheresPhongGlossy)
                );
        }

        // 8) large sphere (white)
        if (IS_MASKED(aBoxMask, kSpheresFresnelConductor))
            mMaterials.push_back(
                new SmoothConductorMaterial(MAT_COPPER_IOR, MAT_AIR_IOR, MAT_COPPER_ABSORBANCE));
                //new SmoothConductorMaterial(MAT_SILVER_IOR, MAT_AIR_IOR, MAT_SILVER_ABSORBANCE));
                //new SmoothConductorMaterial(MAT_GOLD_IOR, MAT_AIR_IOR, MAT_GOLD_ABSORBANCE));
        else if (IS_MASKED(aBoxMask, kSpheresFresnelDielectric))
        {
            const float innerIor = (aDbgAux1 != 1.0f) ? MAT_GLASS_CORNING_IOR : MAT_AIR_IOR;
            const float outerIor = (aDbgAux1 != 1.0f) ? MAT_AIR_IOR           : MAT_GLASS_CORNING_IOR;
            mMaterials.push_back(new SmoothDielectricMaterial(innerIor, outerIor));
        }
        else if (IS_MASKED(aBoxMask, kSpheresMicrofacetGGXConductor))
            mMaterials.push_back(
                new MicrofacetGGXConductorMaterial(0.100f, MAT_COPPER_IOR, MAT_AIR_IOR, MAT_COPPER_ABSORBANCE));
        else if (IS_MASKED(aBoxMask, kSpheresMicrofacetGGXDielectric))
        {
            const float innerIor = (aDbgAux1 != 1.0f) ? MAT_GLASS_CORNING_IOR : MAT_AIR_IOR;
            const float outerIor = (aDbgAux1 != 1.0f) ? MAT_AIR_IOR           : MAT_GLASS_CORNING_IOR;
            const float roughness =
                (aDbgAux2 != INFINITY_F) ? aDbgAux2 : /*0.001f*/ /*0.010f*/ 0.100f;
            mMaterials.push_back(new MicrofacetGGXDielectricMaterial(roughness, innerIor, outerIor));
        }
        else
        {
            diffuseReflectance.SetSRGBAttenuation(0.803922f, 0.803922f, 0.803922f);
            glossyReflectance.SetGreyAttenuation(0.5f);
            mMaterials.push_back(
                new PhongMaterial(
                    diffuseReflectance, glossyReflectance, 90,
                    aBoxMask & kSpheresPhongDiffuse, aBoxMask & kSpheresPhongGlossy)
                );
        }

        // 9) front vertical rectangle
        if (IS_MASKED(aBoxMask, kVertRectFresnelDielectric))
            mMaterials.push_back(
                new SmoothDielectricMaterial(MAT_GLASS_CORNING_IOR, MAT_AIR_IOR));
                //new SmoothDielectricMaterial(MAT_AIR_IOR, MAT_GLASS_CORNING_IOR));
        else if (IS_MASKED(aBoxMask, kVertRectMicrofacetGGXDielectric))
            mMaterials.push_back(
                //new MicrofacetGGXDielectricMaterial(0.001f, MAT_GLASS_CORNING_IOR, MAT_AIR_IOR, true));
                //new MicrofacetGGXDielectricMaterial(0.010f, MAT_GLASS_CORNING_IOR, MAT_AIR_IOR, true));
                  new MicrofacetGGXDielectricMaterial(0.100f, MAT_GLASS_CORNING_IOR, MAT_AIR_IOR, true));
        else
        {
            diffuseReflectance.SetGreyAttenuation(0.8f);
            glossyReflectance.SetGreyAttenuation(0.0f);
            mMaterials.push_back(
                new PhongMaterial(diffuseReflectance, glossyReflectance, 1, 1, 0));
        }

        // 10) diagonal rectangles
        diffuseReflectance.SetGreyAttenuation(0.8f);
        glossyReflectance.SetGreyAttenuation(0.0f);
        mMaterials.push_back(
            new PhongMaterial(diffuseReflectance, glossyReflectance, 1, 1, 0));

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
            Vec3f leftWallCenter  = (cb[0] + cb[4]) * 0.5f + Vec3f(0, 0, ballRadius);
            Vec3f rightWallCenter = (cb[1] + cb[5]) * 0.5f + Vec3f(0, 0, ballRadius);
            float sceneWidth = rightWallCenter.x - leftWallCenter.x;
            Vec3f leftBallCenter  = leftWallCenter  + Vec3f(2.f * sceneWidth / 7.f, 0, 0);
            Vec3f rightBallCenter = rightWallCenter - Vec3f(2.f * sceneWidth / 7.f, -sceneWidth/4, 0);

            geometryList->mGeometry.push_back(new Sphere(leftBallCenter,  ballRadius, 6));
            geometryList->mGeometry.push_back(new Sphere(rightBallCenter, ballRadius, 7));
        }
        if ( aBoxMask & k1Sphere )
        {
            float ballRadius = 1.0f;
            Vec3f floorCenter = (cb[0] + cb[5]) * 0.5f;
            Vec3f ballCenter = floorCenter + Vec3f(0.f, 0.f, ballRadius);

            geometryList->mGeometry.push_back(new Sphere(ballCenter, ballRadius, 8));
        }

        // Rectangles
        if ( aBoxMask & kDiagonalRectangles )
        {
            const Vec3f floorXCenterFront   = (cb[4] + cb[5]) * 0.5f;
            const Vec3f ceilingXCenterBack  = (cb[3] + cb[2]) * 0.5f;
            const float boxHeight           = std::abs(cb[0].z - cb[3].z);
            const float floorWidth          = std::abs(cb[0].x - cb[1].x);

            const float rectHalfWidth       = 0.15f * floorWidth * 0.5f;
            const float rectXOffset         = 0.6f * floorWidth * 0.5f;
            const float rectZOffset         = 0.1f * boxHeight;

            Vec3f rectLeft[4] = {
                floorXCenterFront   + Vec3f(-rectHalfWidth - rectXOffset, 0, rectZOffset),
                floorXCenterFront   + Vec3f( rectHalfWidth - rectXOffset, 0, rectZOffset),
                ceilingXCenterBack  + Vec3f( rectHalfWidth - rectXOffset, 0, rectZOffset),
                ceilingXCenterBack  + Vec3f(-rectHalfWidth - rectXOffset, 0, rectZOffset),
            };
            geometryList->mGeometry.push_back(new Triangle(rectLeft[3], rectLeft[0], rectLeft[1], 10));
            geometryList->mGeometry.push_back(new Triangle(rectLeft[1], rectLeft[2], rectLeft[3], 10));

            Vec3f rectRight[4] = {
                floorXCenterFront   + Vec3f(-rectHalfWidth + rectXOffset, 0, rectZOffset),
                floorXCenterFront   + Vec3f( rectHalfWidth + rectXOffset, 0, rectZOffset),
                ceilingXCenterBack  + Vec3f( rectHalfWidth + rectXOffset, 0, rectZOffset),
                ceilingXCenterBack  + Vec3f(-rectHalfWidth + rectXOffset, 0, rectZOffset),
            };
            geometryList->mGeometry.push_back(new Triangle(rectRight[3], rectRight[0], rectRight[1], 10));
            geometryList->mGeometry.push_back(new Triangle(rectRight[1], rectRight[2], rectRight[3], 10));
        }
        if ( aBoxMask & kVerticalRectangle )
        {
            const Vec3f floorCenter         = (cb[0] + cb[5]) * 0.5f;
            const float floorWidth          = std::abs(cb[0].x - cb[1].x);
            const float boxHeight           = std::abs(cb[0].z - cb[3].z);
            const float boxDepth            = std::abs(cb[0].y - cb[4].y);

            const float rectHalfWidth       =  8.0f * floorWidth * 0.5f;
            const float rectHeight          =  0.7f * boxHeight;
            const float rectYOffset         = -1.0f * boxDepth * 0.5f;
            const float rectZOffset         =  0.1f * boxHeight;

            Vec3f rect[4] = {
                floorCenter + Vec3f(-rectHalfWidth, rectYOffset, rectZOffset),
                floorCenter + Vec3f( rectHalfWidth, rectYOffset, rectZOffset),
                floorCenter + Vec3f( rectHalfWidth, rectYOffset, rectZOffset + rectHeight),
                floorCenter + Vec3f(-rectHalfWidth, rectYOffset, rectZOffset + rectHeight),
            };
            if (aDbgAux1 != 1.0f)
            {
                geometryList->mGeometry.push_back(new Triangle(rect[3], rect[0], rect[1], 9));
                geometryList->mGeometry.push_back(new Triangle(rect[1], rect[2], rect[3], 9));
            }
            else
            {
                geometryList->mGeometry.push_back(new Triangle(rect[1], rect[0], rect[3], 9)); // reverted order
                geometryList->mGeometry.push_back(new Triangle(rect[3], rect[2], rect[1], 9)); // reverted order
            }
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
                        //0.05f, 32.0f);// debug
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
                        -0.12f, 3.0f);
                    break;
                }

                case kEMImgSynthThreePointLighting:
                {
                    light->LoadEnvironmentMap(
                        ".\\Light Probes\\Debugging\\Three point lighting 1024x512.exr",
                        0.0f, 7500.0f);
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

        case kEMImgSynthThreePointLighting:
            oName = "synthetic three point lighting";
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

        if (IS_MASKED(aBoxMask, kVerticalRectangle))
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "vertical rectangle";
            acronym += "vr";
        }

        if (IS_MASKED(aBoxMask, kDiagonalRectangles))
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "diagonal rectangles";
            acronym += "dr";
        }

        if ((aBoxMask & kAllGeometry) == 0)
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
        else if (IS_MASKED(aBoxMask, kSpheresFresnelConductor))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. full fresnel conductor";
            acronym += "Sffc";
        }
        else if (IS_MASKED(aBoxMask, kSpheresFresnelDielectric))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. full fresnel dielectric";
            acronym += "Sffd";
        }
        else if (IS_MASKED(aBoxMask, kSpheresMicrofacetGGXConductor))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. microfacet ggx conductor";
            acronym += "Smgc";
        }
        else if (IS_MASKED(aBoxMask, kSpheresMicrofacetGGXDielectric))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. microfacet ggx dielectric";
            acronym += "Smgd";
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

        if (IS_MASKED(aBoxMask, kVerticalRectangle))
        {
            if (IS_MASKED(aBoxMask, kVertRectFresnelDielectric))
            {
                MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
                name    += "rectangle full fresnel dielectric";
                acronym += "Rffd";
            }
            else if (IS_MASKED(aBoxMask, kVertRectMicrofacetGGXDielectric))
            {
                MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
                name    += "rectangle microfacet ggx dielectric";
                acronym += "Rmgd";
            }
            else
            {
                MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
                name    += "rectangle Phong diffuse";
                acronym += "Rpd";
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
