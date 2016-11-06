#pragma once

#include "math.hxx"
#include "spectrum.hxx"
#include "geometry.hxx"
#include "camera.hxx"
#include "materials.hxx"
#include "lights.hxx"
#include "types.hxx"
#include "debugging.hxx"
#include "utils.hxx"

#include <memory>
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
        mGeometry(nullptr),
        mBackgroundLightId(-1)
    {}

    ~Scene()
    {
        if (mGeometry != nullptr)
            delete mGeometry;
        
        for (auto& material : mMaterials)
            if (material != nullptr)
                delete material;

        // Lights get released using the shared_ptr mechanism
    }

    bool Intersect(
        const Ray       &aRay,
        RayIntersection &oResult) const
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
        ray.org  = aPoint + aDir * Geom::kEpsRay;
        ray.dir  = aDir;
        ray.tmin = 0;
        RayIntersection isect;
        isect.dist = aTMax - 2 * Geom::kEpsRay;

        return mGeometry->IntersectP(ray, isect);
    }

    const AbstractMaterial& GetMaterial(int32_t aMaterialIdx) const
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aMaterialIdx, 0, (int32_t)(mMaterials.size() - 1));
        PG3_ASSERT(mMaterials[aMaterialIdx] != nullptr);

        return *mMaterials[aMaterialIdx];
    }

    int32_t GetMaterialCount() const
    {
        return (int32_t)mMaterials.size();
    }

    const AbstractLight* GetLightPtr(int32_t aLightIdx) const
    {
        PG3_ASSERT_INTEGER_IN_RANGE(aLightIdx, 0, (int32_t)(mLights.size() - 1));

        aLightIdx = std::min<int32_t>(aLightIdx, (int32_t)mLights.size() - 1);
        return mLights[aLightIdx].get();
    }

    size_t GetLightCount() const
    {
        return mLights.size();
    }

    const BackgroundLight* GetBackgroundLight() const
    {
        if (mBackgroundLight)
            return static_cast<BackgroundLight*>(mBackgroundLight.get());
        else
            return nullptr;
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
        const Vec2i         &aResolution,
        BoxMask              aBoxMask = kDefault,
        EnvironmentMapType   aEnvironmentMapType = kEMDefault,
        float                aDbgAux1 = Math::InfinityF(),
        float                aDbgAux2 = Math::InfinityF()
        )
    {
        aDbgAux1, aDbgAux2; // possibly unused parameters

        mSceneName = GetSceneName(aBoxMask, aEnvironmentMapType, &mSceneAcronym);

        bool useCeilingLight = (aBoxMask & kLightCeiling)    != 0;
        bool useLightBox     = (aBoxMask & kLightBox)        != 0;
        bool usePointLight   = (aBoxMask & kLightPoint)      != 0;
        bool useEnvMap       = (aBoxMask & kLightEnv)        != 0;

        // Camera
        mCamera.Setup(
            Vec3f(-0.0439815f, -4.12529f, 0.222539f),
            Vec3f(0.00688625f, 0.998505f, -0.0542161f),
            Vec3f(3.73896e-4f, 0.0542148f, 0.998529f),
            Vec2f(float(aResolution.x), float(aResolution.y)),
            45);

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

        // 6) sphere1 (left, yellow)
        if (Utils::IsMasked(aBoxMask, kSpheresFresnelConductor))
            mMaterials.push_back(
                new SmoothConductorMaterial(
                    SpectralData::kCopperIor, SpectralData::kAirIor, SpectralData::kCopperAbsorb)
                );
        else if (Utils::IsMasked(aBoxMask, kSpheresMicrofacetGGXConductor))
            mMaterials.push_back(
                new MicrofacetGGXConductorMaterial(
                    0.100f,
                    SpectralData::kCopperIor, SpectralData::kAirIor, SpectralData::kCopperAbsorb)
                );
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

        // 7) sphere2 (right, blue)
        if (Utils::IsMasked(aBoxMask, kSpheresFresnelConductor))
            mMaterials.push_back(
                new SmoothConductorMaterial(
                    SpectralData::kSilverIor, SpectralData::kAirIor, SpectralData::kSilverAbsorb)
                );
        else if (Utils::IsMasked(aBoxMask, kSpheresMicrofacetGGXConductor))
            mMaterials.push_back(
                new MicrofacetGGXConductorMaterial(
                    0.100f,
                    SpectralData::kCopperIor, SpectralData::kAirIor, SpectralData::kCopperAbsorb)
                );
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

        // 8) large sphere  
        if (Utils::IsMasked(aBoxMask, kSpheresFresnelConductor))
            mMaterials.push_back(
                new SmoothConductorMaterial(SpectralData::kCopperIor, SpectralData::kAirIor, SpectralData::kCopperAbsorb));
                //new SmoothConductorMaterial(SpectralData::kSilverIor, SpectralData::kAirIor, SpectralData::kSilverAbsorb));
                //new SmoothConductorMaterial(SpectralData::kGoldIor, SpectralData::kAirIor, SpectralData::kGoldAbsorb));
        else if (Utils::IsMasked(aBoxMask, kSpheresFresnelDielectric))
        {
            const float innerIor = (aDbgAux1 != 1.0f) ? SpectralData::kGlassCorningIor  : SpectralData::kAirIor;
            const float outerIor = (aDbgAux1 != 1.0f) ? SpectralData::kAirIor           : SpectralData::kGlassCorningIor;
            mMaterials.push_back(new SmoothDielectricMaterial(innerIor, outerIor));
        }
        else if (Utils::IsMasked(aBoxMask, kSpheresMicrofacetGGXConductor))
        {
            const float roughness =
                (aDbgAux2 != Math::InfinityF()) ? aDbgAux2 : /*0.001f*/ 0.010f /*0.100f*/;
            mMaterials.push_back(
                new MicrofacetGGXConductorMaterial(
                    roughness,
                    SpectralData::kCopperIor, SpectralData::kAirIor, SpectralData::kCopperAbsorb));
        }
        else if (Utils::IsMasked(aBoxMask, kSpheresMicrofacetGGXDielectric))
        {
            const float innerIor = (aDbgAux1 != 1.0f) ? SpectralData::kGlassCorningIor   : SpectralData::kAirIor;
            const float outerIor = (aDbgAux1 != 1.0f) ? SpectralData::kAirIor            : SpectralData::kGlassCorningIor;
            const float roughness =
                (aDbgAux2 != Math::InfinityF()) ? aDbgAux2 : /*0.001f*/ 0.010f /*0.100f*/;
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
        if (Utils::IsMasked(aBoxMask, kVertRectFresnelDielectric))
            mMaterials.push_back(
                new SmoothDielectricMaterial(SpectralData::kGlassCorningIor, SpectralData::kAirIor));
                //new SmoothDielectricMaterial(SpectralData::kAirIor, SpectralData::kGlassCorningIor));
        else if (Utils::IsMasked(aBoxMask, kVertRectMicrofacetGGXDielectric))
            mMaterials.push_back(
                //new MicrofacetGGXDielectricMaterial(0.001f, SpectralData::kGlassCorningIor, SpectralData::kAirIor, true));
                //new MicrofacetGGXDielectricMaterial(0.010f, SpectralData::kGlassCorningIor, SpectralData::kAirIor, true));
                  new MicrofacetGGXDielectricMaterial(0.100f, SpectralData::kGlassCorningIor, SpectralData::kAirIor, true));
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
        mGeometry = nullptr;

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
            if (useCeilingLight && !useLightBox)
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
        if (aBoxMask & k1Sphere)
        {
            float ballRadius = 1.0f;
            Vec3f floorCenter = (cb[0] + cb[5]) * 0.5f;
            Vec3f ballCenter = floorCenter + Vec3f(0.f, 0.f, ballRadius);

            // Original
            //geometryList->mGeometry.push_back(new Sphere(ballCenter, ballRadius, 8));

            // Visualize initial triangulation
            //Vec3f vertices[12];
            //Vec3ui faces[20];
            //Geom::UnitIcosahedron(vertices, faces);
            //for (uint32_t i = 0; i < Utils::ArrayLength(faces); i++)
            //{
            //    geometryList->mGeometry.push_back(new Triangle(
            //        ballCenter + ballRadius * vertices[faces[i].Get(0)],
            //        ballCenter + ballRadius * vertices[faces[i].Get(1)],
            //        ballCenter + ballRadius * vertices[faces[i].Get(2)], 8));
            //}

            // Visualize subdivision
            const uint32_t dbgEm = (uint32_t)aDbgAux1;
            const char *emPath = [dbgEm](){
                switch (dbgEm)
                {
                case 0:  return ".\\Light Probes\\Debugging\\Const white 8x4.exr";
                case 1:  return ".\\Light Probes\\Debugging\\Const white 16x8.exr";
                case 2:  return ".\\Light Probes\\Debugging\\Const white 32x16.exr";
                case 3:  return ".\\Light Probes\\Debugging\\Const white 64x32.exr";
                case 4:  return ".\\Light Probes\\Debugging\\Const white 128x64.exr";
                case 5:  return ".\\Light Probes\\Debugging\\Const white 256x128.exr";
                case 6:  return ".\\Light Probes\\Debugging\\Const white 512x256.exr";
                case 7:  return ".\\Light Probes\\Debugging\\Const white 1024x512.exr";
                case 8:  return ".\\Light Probes\\Debugging\\Const white 2048x1024.exr";
                case 9:  return ".\\Light Probes\\Debugging\\Const white 4096x2048.exr";
                //".\\Light Probes\\Debugging\\Single pixel.exr", 0.42f // 16x8
                default: return "";
                }
            }();
            const float oversamplingFactor = (aDbgAux2 != Math::InfinityF()) ? aDbgAux2 : 1.0f;
            printf("Testing EM \"%s\", oversampling %.2f:\n", emPath, oversamplingFactor);
            std::unique_ptr<EnvironmentMapImage> image(EnvironmentMapImage::LoadImage(emPath));
            if (image)
            {
                std::list<EnvironmentMapSteeringSampler::TreeNode*> triangles;
                if (EnvironmentMapSteeringSampler::TriangulateEm(triangles, 1, *image, false,
                                                                 oversamplingFactor))
                {
                    for (const auto &node : triangles)
                    {
                        const auto triangle = static_cast<EnvironmentMapSteeringSampler::TriangleNode*>(node);
                        geometryList->mGeometry.push_back(new Triangle(
                            ballCenter + ballRadius * triangle->sharedVertices[0]->dir,
                            ballCenter + ballRadius * triangle->sharedVertices[1]->dir,
                            ballCenter + ballRadius * triangle->sharedVertices[2]->dir,
                            8));
                    }
                    EnvironmentMapSteeringSampler::FreeNodesList(triangles);
                }
            }
            Debugging::Exit(); // debug

            // Visualize spherical triangle uniform sampling
            //const float countPerDimension = 40.f;   //30.f;   //20.f;   //
            //const float pointSize         = 0.005f; //0.005f; //0.008f; //
            //Rng rng;
            //const float binSize = 1.f / countPerDimension;
            //for (uint32_t i = 0; i < 2; i++)
            //{
            //    Vec3f vertexA;
            //    Vec3f vertexB;
            //    Vec3f vertexC;
            //    switch (i)
            //    {
            //    case 0:
            //        vertexA = Vec3f(1.f,  0.f, 0.f);
            //        vertexB = Vec3f(0.f, -1.f, 0.f);
            //        vertexC = Vec3f(0.f,  0.f, 1.f);
            //        break;
            //    case 1:
            //        vertexA = Vec3f( 0.f, 1.f,  0.f);
            //        vertexB = Vec3f( 0.f, 0.f, -1.f);
            //        vertexC = Vec3f(-1.f, 0.f,  0.f);
            //        break;
            //    default:
            //        break;
            //    }

            //    for (float u = 0.f; u < (1.f - 0.0001f); u += binSize)
            //        for (float v = 0.f; v < (1.f - 0.0001f); v += binSize)
            //        {
            //            //Vec2f sample(u, v);
            //            //Vec2f sample = rng.GetVec2f();
            //            Vec2f sample = Vec2f(u, v) + rng.GetVec2f() * binSize;

            //            const Vec3f triangleSample =
            //                Sampling::SampleUniformSphericalTriangle(
            //                    vertexA, vertexB, vertexC, sample);
            //            geometryList->mGeometry.push_back(
            //                new Sphere(ballCenter + ballRadius * triangleSample,
            //                           pointSize * ballRadius, 8));
            //        }
            //}

            //// Visualize planar triangle sampling
            //{
            //    Vec3f vertices[12];
            //    Vec3ui faces[20];
            //    Geom::UnitIcosahedron(vertices, faces);
            //    auto face0 = vertices[faces[13].Get(0)];
            //    auto face1 = vertices[faces[13].Get(1)];
            //    auto face2 = vertices[faces[13].Get(2)];
            //    const uint32_t countPerDimension = 20;
            //    const float pointSize = 0.005f; //0.008f; //
            //    //Rng rng;
            //    const float binSize = 1.f / countPerDimension;
            //    //for (float u = 0.f; u < (1.f - 0.0001f); u += binSize)
            //    //{
            //    //    for (float v = 0.f; v < (1.f - 0.0001f); v += binSize)
            //    //        Vec2f sample(u, v);
            //    //        Vec2f sample = rng.GetVec2f();
            //    //        Vec2f sample = Vec2f(u, v) + rng.GetVec2f() * binSize;
            //    for (uint32_t i = 0; i <= countPerDimension; ++i)
            //        for (uint32_t j = 0; j <= countPerDimension; ++j)
            //        {
            //            Vec2f sample = Vec2f(Math::Sqr(i * binSize), j * binSize);

            //            const Vec3f triangleSample =
            //                Sampling::SampleUniformTriangle(face0, face1, face2, sample);
            //            geometryList->mGeometry.push_back(
            //                new Sphere(ballCenter + ballRadius * triangleSample,
            //                           pointSize * ballRadius, 8));
            //        }
            //}
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

        if (useLightBox && !useCeilingLight)
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
        
        if (useCeilingLight && !useLightBox)
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
            mLights[0] = TLightSharedPtr(light);
            mMaterial2Light.insert(std::make_pair(0, 0));

            light = new AreaLight(cb[7], cb[3], cb[2]);
            light->SetPower(lightPower);
            mLights[1] = TLightSharedPtr(light);
            mMaterial2Light.insert(std::make_pair(1, 1));
        }

        if (useLightBox && !useCeilingLight)
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
            mLights[0] = TLightSharedPtr(light);
            mMaterial2Light.insert(std::make_pair(0, 0));

            light = new AreaLight(lb[5], lb[0], lb[1]);
            light->SetPower(lightPower);
            mLights[1] = TLightSharedPtr(light);
            mMaterial2Light.insert(std::make_pair(1, 1));
        }

        if (usePointLight)
        {
            PointLight *light = new PointLight(Vec3f(0.0, -0.5, 1.0));

            SpectrumF lightPower;
            lightPower.SetSRGBGreyLight(50.f/*Watts*/);
            light->SetPower(lightPower);

            mLights.push_back(TLightSharedPtr(light));
        }

        if (useEnvMap)
        {
            BackgroundLight *light = new BackgroundLight();

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
                    radiance.SetSRGBGreyLight(0.87f); // 2^{-0.2}
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

            if (light != nullptr)
            {
                mBackgroundLight = TLightSharedPtr(light);
                mLights.push_back(mBackgroundLight);
                mBackgroundLightId = (int32_t)(mLights.size() - 1);
            }
        }
    }

    static std::string GetEnvMapName(
        uint32_t     aEnvironmentMapType,
        std::string *oAcronym = nullptr
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
        BoxMask              aBoxMask = kDefault,
        EnvironmentMapType   aEnvironmentMapType = kEMInvalid,
        std::string         *oAcronym = nullptr)
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

        if (Utils::IsMasked(aBoxMask, kWalls))
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name += "walls";
            acronym += "w";
        }

        if (Utils::IsMasked(aBoxMask, k2Spheres))
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "2 spheres";
            acronym += "2s";
        }

        if (Utils::IsMasked(aBoxMask, k1Sphere))
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "1 sphere";
            acronym += "1s";
        }

        if (Utils::IsMasked(aBoxMask, kVerticalRectangle))
        {
            GEOMETRY_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "vertical rectangle";
            acronym += "vr";
        }

        if (Utils::IsMasked(aBoxMask, kDiagonalRectangles))
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

        if (Utils::IsMasked(aBoxMask, kLightCeiling))
        {
            LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "ceiling light";
            acronym += "c";
        }

        if (Utils::IsMasked(aBoxMask, kLightBox))
        {
            LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "light box";
            acronym += "b";
        }

        if (Utils::IsMasked(aBoxMask, kLightPoint))
        {
            LIGHTS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "point light";
            acronym += "p";
        }
        
        if (Utils::IsMasked(aBoxMask, kLightEnv))
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

        if (Utils::IsMasked(aBoxMask, kSpheresPhongDiffuse) ||
            Utils::IsMasked(aBoxMask, kSpheresPhongGlossy))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. Phong";
            acronym += "Sp";

            if (Utils::IsMasked(aBoxMask, kSpheresPhongDiffuse))
            {
                name    += " diffuse";
                acronym += "d";
            }

            if (Utils::IsMasked(aBoxMask, kSpheresPhongGlossy))
            {
                name    += " glossy";
                acronym += "g";
            }
        }
        else if (Utils::IsMasked(aBoxMask, kSpheresFresnelConductor))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. full fresnel conductor";
            acronym += "Sffc";
        }
        else if (Utils::IsMasked(aBoxMask, kSpheresFresnelDielectric))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. full fresnel dielectric";
            acronym += "Sffd";
        }
        else if (Utils::IsMasked(aBoxMask, kSpheresMicrofacetGGXConductor))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. microfacet ggx conductor";
            acronym += "Smgc";
        }
        else if (Utils::IsMasked(aBoxMask, kSpheresMicrofacetGGXDielectric))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "sph. microfacet ggx dielectric";
            acronym += "Smgd";
        }

        if (Utils::IsMasked(aBoxMask, kWallsPhongDiffuse) ||
            Utils::IsMasked(aBoxMask, kWallsPhongGlossy))
        {
            MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
            name    += "walls Phong";
            acronym += "Wp";

            if (Utils::IsMasked(aBoxMask, kWallsPhongDiffuse))
            {
                name    += " diffuse";
                acronym += "d";
            }

            if (Utils::IsMasked(aBoxMask, kWallsPhongGlossy))
            {
                name    += " glossy";
                acronym += "g";
            }
        }

        if (Utils::IsMasked(aBoxMask, kVerticalRectangle))
        {
            if (Utils::IsMasked(aBoxMask, kVertRectFresnelDielectric))
            {
                MATERIALS_ADD_COMMA_AND_SPACE_IF_NEEDED
                name    += "rectangle full fresnel dielectric";
                acronym += "Rffd";
            }
            else if (Utils::IsMasked(aBoxMask, kVertRectMicrofacetGGXDielectric))
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

    typedef std::shared_ptr<AbstractLight> TLightSharedPtr;

    AbstractGeometry                    *mGeometry;
    Camera                               mCamera;
    std::vector<AbstractMaterial*>       mMaterials;
    std::vector<TLightSharedPtr>         mLights;
    std::map<int32_t, int32_t>           mMaterial2Light;
    TLightSharedPtr                      mBackgroundLight;
    int32_t                              mBackgroundLightId;

    std::string                          mSceneName;
    std::string                          mSceneAcronym;
};
