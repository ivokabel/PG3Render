#pragma once

#include "math.hxx"
#include "ray.hxx"
#include "scene_graph.hxx"
#include "camera.hxx"
#include "frame_buffer.hxx"
#include "scene.hxx"
#include "aux_dbg_params.hxx"
#include "debugging.hxx"
#include "utils.hxx"
#include "hard_config.hxx"
#include "types.hxx"

#include <omp.h>
#include <string>
#include <set>
#include <sstream>
#include <vector>
#include <cmath>
#include <time.h>

enum Algorithm
{
    kEyeLight,
    kDirectIllumBsdfSampling,
    kDirectIllumLightSamplingAll,
    kDirectIllumLightSamplingSingle,
    kDirectIllumMis,
    kPathTracingNaive,
    kPathTracing,
    kAlgorithmCount
};

// Hardwired scene configurations
#define GEOM_FULL_BOX           Scene::kWalls | Scene::kFloor | Scene::k2Spheres
#define GEOM_2SPHERES_ON_FLOOR                  Scene::kFloor | Scene::k2Spheres
#define GEOM_BOX_1SPHERE        Scene::kWalls | Scene::kFloor | Scene::k1Sphere
#define GEOM_1SPHERE            Scene::k1Sphere
#define GEOM_LAYERED_SPHERE     Scene::kLayeredSphere
#define GEOM_RECTANTGLES        Scene::kVerticalRectangle | Scene::kDiagonalRectangles
#define MATS_PHONG_DIFFUSE      Scene::kWallsPhongDiffuse | Scene::kSpheresPhongDiffuse
#define MATS_PHONG_GLOSSY       Scene::kWallsPhongGlossy | Scene::kSpheresPhongGlossy
const Scene::BoxMask g_SceneConfigs[] =
{
    // Point light, box: 0, 1
    Scene::BoxMask(Scene::kLightPoint   | GEOM_FULL_BOX | MATS_PHONG_DIFFUSE),
    Scene::BoxMask(Scene::kLightPoint   | GEOM_FULL_BOX | MATS_PHONG_DIFFUSE | MATS_PHONG_GLOSSY),

    // Ceiling light, box: 2, 3
    Scene::BoxMask(Scene::kLightCeiling | GEOM_FULL_BOX | MATS_PHONG_DIFFUSE),
    Scene::BoxMask(Scene::kLightCeiling | GEOM_FULL_BOX | MATS_PHONG_DIFFUSE | MATS_PHONG_GLOSSY),

    // Light box, box: 4, 5
    Scene::BoxMask(Scene::kLightBox     | GEOM_FULL_BOX | MATS_PHONG_DIFFUSE),
    Scene::BoxMask(Scene::kLightBox     | GEOM_FULL_BOX | MATS_PHONG_DIFFUSE | MATS_PHONG_GLOSSY),

    // Environment map, box: 6, 7
    Scene::BoxMask(Scene::kLightEnv     | GEOM_FULL_BOX | MATS_PHONG_DIFFUSE),
    Scene::BoxMask(Scene::kLightEnv     | GEOM_FULL_BOX | MATS_PHONG_DIFFUSE | MATS_PHONG_GLOSSY),

    // Environment map, no walls: 8, 9
    Scene::BoxMask(Scene::kLightEnv     | GEOM_2SPHERES_ON_FLOOR | MATS_PHONG_DIFFUSE),
    Scene::BoxMask(Scene::kLightEnv     | GEOM_2SPHERES_ON_FLOOR | MATS_PHONG_DIFFUSE | MATS_PHONG_GLOSSY),

    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Multiple lights, diffuse: 10-13
    Scene::BoxMask(                     Scene::kLightBox | Scene::kLightEnv | GEOM_FULL_BOX          | MATS_PHONG_DIFFUSE),
    Scene::BoxMask(Scene::kLightPoint | Scene::kLightBox |                    GEOM_FULL_BOX          | MATS_PHONG_DIFFUSE),
    Scene::BoxMask(Scene::kLightPoint | Scene::kLightBox | Scene::kLightEnv | GEOM_FULL_BOX          | MATS_PHONG_DIFFUSE),
    Scene::BoxMask(Scene::kLightPoint | Scene::kLightBox | Scene::kLightEnv | GEOM_2SPHERES_ON_FLOOR | MATS_PHONG_DIFFUSE),

    // Multiple lights, glossy: 14, 15
    Scene::BoxMask(Scene::kLightPoint | Scene::kLightBox | Scene::kLightEnv | GEOM_2SPHERES_ON_FLOOR | MATS_PHONG_DIFFUSE | MATS_PHONG_GLOSSY),
    Scene::BoxMask(Scene::kLightPoint | Scene::kLightBox | Scene::kLightEnv | GEOM_FULL_BOX          | MATS_PHONG_DIFFUSE | MATS_PHONG_GLOSSY),

    // Material testing, Full box: 16-19
    Scene::BoxMask(Scene::kLightPoint   | GEOM_FULL_BOX    | Scene::kSpheresFresnelConductor | Scene::kWallsPhongDiffuse),
    Scene::BoxMask(Scene::kLightBox     | GEOM_FULL_BOX    | Scene::kSpheresFresnelConductor | Scene::kWallsPhongDiffuse),
    Scene::BoxMask(Scene::kLightCeiling | GEOM_FULL_BOX    | Scene::kSpheresFresnelConductor | Scene::kWallsPhongDiffuse),
    Scene::BoxMask(Scene::kLightBox     | GEOM_FULL_BOX    | Scene::kSpheresFresnelConductor | Scene::kWallsPhongDiffuse | Scene::kWallsPhongGlossy),

    // Material testing, 1 sphere: 20-27
    Scene::BoxMask(Scene::kLightEnv    | GEOM_1SPHERE      | Scene::kSpheresPhongDiffuse),
    Scene::BoxMask(Scene::kLightEnv    | GEOM_1SPHERE      | Scene::kSpheresPhongGlossy),
    Scene::BoxMask(Scene::kLightEnv    | GEOM_1SPHERE      | Scene::kSpheresPhongDiffuse | Scene::kSpheresPhongGlossy),
    Scene::BoxMask(Scene::kLightEnv    | GEOM_1SPHERE      | Scene::kSpheresFresnelConductor),
    Scene::BoxMask(Scene::kLightEnv    | GEOM_1SPHERE      | Scene::kSpheresFresnelDielectric),
    Scene::BoxMask(Scene::kLightEnv    | GEOM_1SPHERE      | Scene::kSpheresMicrofacetGGXConductor),
    Scene::BoxMask(Scene::kLightEnv    | GEOM_1SPHERE      | Scene::kSpheresMicrofacetGGXDielectric),
    Scene::BoxMask(Scene::kLightEnv    | GEOM_1SPHERE      | Scene::kSpheresWeidlichWilkieLayers),

    // Material testing, rectangles: 28-29
    Scene::BoxMask(Scene::kLightEnv     | GEOM_RECTANTGLES | Scene::kVertRectFresnelDielectric),
    Scene::BoxMask(Scene::kLightEnv     | GEOM_RECTANTGLES | Scene::kVertRectMicrofacetGGXDielectric),

    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Box with sphere: 30-37
    Scene::BoxMask(Scene::kLightBox     | GEOM_BOX_1SPHERE | Scene::kSpheresFresnelConductor         | Scene::kWallsPhongDiffuse),
    Scene::BoxMask(Scene::kLightBox     | GEOM_BOX_1SPHERE | Scene::kSpheresFresnelDielectric        | Scene::kWallsPhongDiffuse),
    Scene::BoxMask(Scene::kLightBox     | GEOM_BOX_1SPHERE | Scene::kSpheresMicrofacetGGXConductor   | Scene::kWallsPhongDiffuse),
    Scene::BoxMask(Scene::kLightBox     | GEOM_BOX_1SPHERE | Scene::kSpheresMicrofacetGGXDielectric  | Scene::kWallsPhongDiffuse),
    Scene::BoxMask(Scene::kLightCeiling | GEOM_BOX_1SPHERE | Scene::kSpheresMicrofacetGGXConductor   | Scene::kWallsPhongDiffuse),
    Scene::BoxMask(Scene::kLightCeiling | GEOM_BOX_1SPHERE | Scene::kSpheresMicrofacetGGXDielectric  | Scene::kWallsPhongDiffuse),
    Scene::BoxMask(Scene::kLightPoint   | GEOM_BOX_1SPHERE | Scene::kSpheresMicrofacetGGXDielectric  | Scene::kWallsPhongDiffuse),
    Scene::BoxMask(Scene::kLightEnv     | GEOM_BOX_1SPHERE | Scene::kSpheresMicrofacetGGXDielectric  | Scene::kWallsPhongDiffuse),

    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Debug: Layered material reference: 38 
    Scene::BoxMask(Scene::kLightEnv | GEOM_LAYERED_SPHERE),
};

// Renderer configuration, holds algorithm, scene, and all other settings. Provides related routines.
class Config
{
public:

    static const char* GetName(Algorithm aAlgorithm)
    {
        static const char* algorithmNames[kAlgorithmCount] =
        {
            "eye light",
            "direct illumination - BSDF sampling",
            "direct illumination - light sampling (all)",
            "direct illumination - light sampling (single sample)",
            "direct illumination - multiple importance sampling",
            "naive path tracing",
            "path tracing",
        };

        if (aAlgorithm < 0 || aAlgorithm >= kAlgorithmCount)
            return "unknown algorithm";

        return algorithmNames[aAlgorithm];
    }

    static const char* GetAcronym(Algorithm aAlgorithm)
    {
        static const char* algorithmNames[kAlgorithmCount] = { "el", "dbs", "dlsa", "dlss", "dmis", "ptn", "pt" };

        if (aAlgorithm < 0 || aAlgorithm >= kAlgorithmCount)
            return "unknown";

        return algorithmNames[aAlgorithm];
    }

    std::string DefaultFilename(
        const uint32_t          aSceneConfig,
        std::string             aOutputNameTrail
        ) const
    {
        aSceneConfig; // unused parameter

        std::string filename;

        filename += mScene->mSceneAcronym;

        // Algorithm acronym
        filename += "_";
        filename += Config::GetAcronym(mAlgorithm);

        // Path length
        if (   (mAlgorithm == kPathTracingNaive)
            || (mAlgorithm == kPathTracing))
        {
            filename += "_";
            if (mMaxPathLength == 0)
                filename += "rr";
            else
                filename +=
                      "pl"
                    + std::to_string(mMinPathLength)
                    + "-"
                    + std::to_string(mMaxPathLength);
        }

        // Splitting settings
        if (mAlgorithm == kPathTracing)
        {
            filename += "_splt" + std::to_string(mSplittingBudget);

            // debug, temporary: indirect illum splitting level
            std::ostringstream outStream2;
            outStream2.precision(1);
            outStream2 << std::fixed << mDbgSplittingLevel;
            filename += "," + outStream2.str();

            // debug, temporary: light-to-bsdf samples ratio
            std::ostringstream outStream1;
            outStream1.precision(1);
            outStream1 << std::fixed << mDbgSplittingLightToBrdfSmplRatio;
            filename += "," + outStream1.str();
        }

        // Indirect illumination clipping
        if ((mAlgorithm == kPathTracing) && (mIndirectIllumClipping > 0.f))
        {
            filename += "_iic";
            std::ostringstream outStream;
            outStream.precision(1);
            outStream << std::fixed << mIndirectIllumClipping;
            filename += outStream.str();
        }

#if defined PG3_USE_ENVMAP_SIMPLE_SPHERICAL_SAMPLER
        // default: no code
#elif defined PG3_USE_ENVMAP_STEERABLE_SAMPLER
        filename += "_emss";
#else
        filename += "_emcs";
#endif

        // Sample count
        filename += "_";
        if (mMaxTime > 0)
        {
            //filename += std::to_string(mMaxTime);
            std::ostringstream outStream;
            outStream.width(2);
            outStream.fill('0');
            outStream << mMaxTime;
            filename += outStream.str();

            filename += "sec";
        }
        else
        {
            filename += std::to_string(mIterations);
            filename += "s";
        }

        // Custom trail text
        if (!aOutputNameTrail.empty())
        {
            filename += "_";
            filename += aOutputNameTrail;
        }

        // The chosen otuput format extension
        filename += "." + mDefOutputExtension;

        return filename;
    }

    static void PrintRngWarning()
    {
    #if defined(LEGACY_RNG)
        printf("The code was not compiled for C++11.\n");
        printf("It will be using Tiny Encryption Algorithm-based random number generator.\n");
        printf("This is worse than the Mersenne Twister from C++11.\n");
        printf("Consider setting up for C++11.\n");
        printf("Visual Studio 2010, and g++ 4.6.3 and later work.\n\n");
    #endif
    }

    void PrintConfiguration() const
    {
        if (mQuietMode)
            return;

        printf("========== [[ PG3Render ]] ==========\n");

        printf("Scene:     %s\n", mScene->mSceneName.c_str());

        printf("Algorithm: %s", GetName(mAlgorithm));
        if (   (mAlgorithm == kPathTracingNaive)
            || (mAlgorithm == kPathTracing))
        {
            if (mMaxPathLength == 0)
                printf(", Russian roulette path ending");
            else
                printf(", path lengths: %d-%d", mMinPathLength, mMaxPathLength);
        }
        if ((mAlgorithm == kPathTracing) && (mIndirectIllumClipping > 0.f))
            printf(", indirect illum. clipping: %.2f", mIndirectIllumClipping);
        if (mAlgorithm == kPathTracing)
        {
            // debug, temporary
            printf(
                ", splitting: %d, %.1f, %.1f",
                mSplittingBudget,
                mDbgSplittingLevel,
                mDbgSplittingLightToBrdfSmplRatio);
        }
        printf("\n");

        // Configuration
        if (mMaxTime > 0)
            printf("Config:    %g seconds render time", mMaxTime);
        else
            printf("Config:    %d iteration(s)", mIterations);
        printf(""
            ", %d threads, "
            #if !defined _DEBUG
                "release"
            #else
                "debug"
            #endif
            #ifdef PG3_ASSERT_ENABLED
                " with assertions"
            #endif
            "\n",
            mNumThreads
            );

        // Debugging options
        if (!mAuxDbgParams.IsEmpty())
            printf(
                "Debugging aux params: "

                "float1 %f, "
                "float2 %f, "
                "float3 %f, "
                "float4 %f, "
                "float5 %f, "

                "bool1 %s, "
                "bool2 %s, "
                "bool3 %s, "
                "bool4 %s, "
                "bool5 %s"

                "\n",

                mAuxDbgParams.float1,
                mAuxDbgParams.float2,
                mAuxDbgParams.float3,
                mAuxDbgParams.float4,
                mAuxDbgParams.float5,

                mAuxDbgParams.bool1 ? "true" : "false",
                mAuxDbgParams.bool2 ? "true" : "false",
                mAuxDbgParams.bool3 ? "true" : "false",
                mAuxDbgParams.bool4 ? "true" : "false",
                mAuxDbgParams.bool5 ? "true" : "false");

        // Output
        printf("Out file:  ");
        if (!mOutputDirectory.empty())
            printf("%s\\", mOutputDirectory.c_str());
        printf("%s\n", mOutputName.c_str());

        fflush(stdout);
    }

    static void PrintHelp(const char *argv[])
    {
        std::string filename;
        if (!Utils::IO::GetFileName(argv[0], filename))
            filename = argv[0];

        printf("\n");
        printf(
            "Usage: %s "
            "[-s <scene_id>] [-a <algorithm>] [-t <time> | -i <iterations>] [-minpl <min_path_length>] "
            "[-maxpl <max_path_length>] [-iic <indirect_illum_clipping_value>] "
            "[-sb|--splitting-budget <splitting_budget>] "
            "[-slbr|--splitting-light-to-bsdf-ratio <splitting_light_to_bsdf_ratio>] "
            "[-em <env_map_type>] [-e <def_output_ext>] [-od <output_directory>] [-o <output_name>] "
            "[-ot <output_trail>] [-j <threads_count>] [-q] [-opop|--only-print-output-pathname] "
            "[-auxf1|--dbg_aux_float1 <value>] "
            "[-auxf2|--dbg_aux_float2 <value>] "
            "[-auxf3|--dbg_aux_float3 <value>] "
            "[-auxf4|--dbg_aux_float4 <value>] "
            "[-auxf4|--dbg_aux_float5 <value>] "
            "[-auxb1|--dbg_aux_bool1 <value>] "
            "[-auxb2|--dbg_aux_bool2 <value>] "
            "[-auxb3|--dbg_aux_bool3 <value>] "
            "[-auxb4|--dbg_aux_bool4 <value>] "
            "[-auxb4|--dbg_aux_bool5 <value>] "
            "\n\n",
            filename.c_str());

        printf("    -s     Selects the scene (default 0):\n");
        for (uint32_t i = 0; i < Utils::ArrayLength(g_SceneConfigs); i++)
            printf("          %2d    %s\n", i, Scene::GetSceneName(g_SceneConfigs[i]).c_str());

        printf("    -em    Selects the environment map type (default 0; ignored if the scene doesn't use an environment map):\n");
        for (int32_t i = 0; i < Scene::kEMCount; i++)
            printf("          %2d    %s\n", i, Scene::GetEnvMapName(i).c_str());

        printf("    -a     Selects the rendering algorithm (default pt):\n");
        for (int32_t i = 0; i < (int32_t)kAlgorithmCount; i++)
            printf("          %-4s  %s\n",
                Config::GetAcronym(Algorithm(i)),
                Config::GetName(Algorithm(i)));
        printf("    -maxpl Maximum path length. Only valid for path tracers.\n");
        printf("           0 means no hard limit - paths are ended using Russian roulette (default behaviour)\n");
        printf("    -minpl Minimum path length. Must be greater than 0 and not greater then maximum path length.\n");
        printf("           Must not be set if Russian roulette is used for ending paths. Only valid for path tracers.\n");
        printf("           Default is 1.\n");
        printf("    -iic   Maximal allowed value for indirect illumination estimates. 0 means no clipping (default).\n");
        printf("           Only valid for path tracer (pt).\n");
        printf("    -sb | --splitting-budget \n");
        printf("           Splitting budget: maximal total amount of splitted paths per one camera ray (default 4).\n");
        printf("    -slbr | --splitting-light-to-bsdf-ratio \n");
        printf("           Number of light samples per one bsdf sample (default 1.0)\n");

        printf("    -t     Number of seconds to run the algorithm\n");
        printf("    -i     Number of iterations to run the algorithm (default 1)\n");
        printf("    -e     Extension of the default output file: bmp or hdr (default bmp)\n");
        printf("    -od    User specified directory for the output, whose existence is not checked (default \"\")\n");
        printf("    -o     User specified output name, with extension .bmp or .hdr (default .bmp)\n");
        printf("    -ot    Trail text to be added at the end the output file name\n");
        printf("           (only used to alter a default filename; '_' is pasted automatically before the trail).\n");
        printf("    -j     Number of threads (\"jobs\") to be used\n");
        printf("    -q     Quiet mode - doesn't print anything except for warnings and errors\n");

        printf("\n");

        printf("    -auxf1 | --dbg_aux_float1 \n");
        printf("           Auxiliary float debugging parameter no. 1 (default: infinity=not set).\n");
        printf("    -auxf2 | --dbg_aux_float2 \n");
        printf("           Auxiliary float debugging parameter no. 2 (default: infinity=not set).\n");
        printf("    -auxf3 | --dbg_aux_float3 \n");
        printf("           Auxiliary float debugging parameter no. 3 (default: infinity=not set).\n");
        printf("    -auxf4 | --dbg_aux_float4 \n");
        printf("           Auxiliary float debugging parameter no. 4 (default: infinity=not set).\n");
        printf("    -auxf5 | --dbg_aux_float5 \n");
        printf("           Auxiliary float debugging parameter no. 5 (default: infinity=not set).\n");

        printf("\n");

        printf("    -auxb1 | --dbg_aux_bool1 \n");
        printf("           Auxiliary bool debugging parameter no. 1 (default: false).\n");
        printf("    -auxb2 | --dbg_aux_bool2 \n");
        printf("           Auxiliary bool debugging parameter no. 2 (default: false).\n");
        printf("    -auxb3 | --dbg_aux_bool3 \n");
        printf("           Auxiliary bool debugging parameter no. 3 (default: false).\n");
        printf("    -auxb4 | --dbg_aux_bool4 \n");
        printf("           Auxiliary bool debugging parameter no. 4 (default: false).\n");
        printf("    -auxb5 | --dbg_aux_bool5 \n");
        printf("           Auxiliary bool debugging parameter no. 5 (default: false).\n");

        printf("\n");

        printf("    -opop | --only-print-output-pathname \n");
        printf("           Do not render anything; just print the full path of the current output file.\n");

        printf("\n    Note: Time (-t) takes precedence over iterations (-i) if both are defined\n");
    }


    bool ProcessCommandlineParamFloat(
        const int32_t    aArgc,
        const char      *aArgv[],
        int32_t         &aParamIdx,
        float           &aValue,
        const char      *aParamName)
    {
        if (++aParamIdx == aArgc)
        {
            printf(
                "Error: Missing <%s> argument, please see help (-h)\n",
                aParamName);
            return false;
        }

        float tmp;
        std::istringstream iss(aArgv[aParamIdx]);
        iss >> tmp;

        if (iss.fail())
        {
            printf(
                "Error: Invalid <%s> argument \"%s\" (should be a floating point value). Please see help (-h).\n",
                aParamName,aArgv[aParamIdx]);
            return false;
        }

        aValue = tmp;
        return true;
    }


    bool ProcessCommandlineParamBool(
        const int32_t    aArgc,
        const char      *aArgv[],
        int32_t         &aParamIdx,
        bool            &aValue,
        const char      *aParamName)
    {
        if (++aParamIdx == aArgc)
        {
            printf(
                "Error: Missing <%s> argument, please see help (-h)\n",
                aParamName);
            return false;
        }

        bool tmp;
        std::istringstream iss(aArgv[aParamIdx]);

        // parse "true" or "false"
        iss >> std::boolalpha >> tmp;

        if (iss.fail())
        {
            // try also "1" or "0"
            iss.clear();
            iss >> std::noboolalpha >> tmp;
        }

        if (iss.fail())
        {
            printf(
                "Error: Invalid <%s> argument \"%s\" (should be a boolean value: true/false or 1/0). Please see help (-h).\n",
                aParamName, aArgv[aParamIdx]);
            return false;
        }

        aValue = tmp;
        return true;
    }


    // Parses command line, setting up config
    bool ProcessCommandline(int32_t argc, const char *argv[])
    {
        // Parameters marked with [cmd] can be changed from command line
        mScene                          = nullptr;                  // [cmd] When nullptr, renderer will not run

        mOnlyPrintOutputPath            = false;                    // [cmd]

        mIterations                     = 1;                        // [cmd]
        mMaxTime                        = -1.f;                     // [cmd]
        mDefOutputExtension             = "bmp";                    // [cmd]
        mOutputName                     = "";                       // [cmd]
        mOutputDirectory                = "";                       // [cmd]
        mNumThreads                     = 0;
        mQuietMode                      = false;
        mBaseSeed                       = 1234;
        mResolution                     = Vec2i(512, 512);

        mAlgorithm                      = kAlgorithmCount;          // [cmd]
        mMinPathLength                  = 1;                        // [cmd]
        mMaxPathLength                  = 0;                        // [cmd]
        mIndirectIllumClipping          = 0.f;                      // [cmd]
        mSplittingBudget                = 4;                        // [cmd]

        // debug, temporary
        mDbgSplittingLevel = 1.f;                                   // [cmd]
        mDbgSplittingLightToBrdfSmplRatio = 1.f;                    // [cmd]

        int32_t sceneID     = 0; // default 0
        uint32_t envMapID   = Scene::kEMDefault;
        std::string outputNameTrail;

        // Load arguments
        for (int32_t i=1; i<argc; i++)
        {
            std::string arg(argv[i]);

            // print help string (at any position)
            if (arg == "-h" || arg == "--help" || arg == "/?")
            {
                PrintHelp(argv);
                return false;
            }

            if (arg[0] != '-') // all our commands start with -
            {
                continue;
            }
            else if ((arg == "-opop") || (arg == "--only-print-output-pathname"))
            {
                mOnlyPrintOutputPath = true;
            }
            else if (arg == "-j") // jobs (number of threads)
            {
                if (++i == argc)
                {
                    printf("Error: Missing <threads_count> argument, please see help (-h)\n");
                    return false;
                }

                std::istringstream iss(argv[i]);
                iss >> mNumThreads;

                if (iss.fail() || mNumThreads <= 0)
                {
                    printf(
                        "Error: Invalid <threads_count> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }
            }
            else if (arg == "-q") // quiet mode
            {
                mQuietMode = true;
            }
            else if (arg == "-s") // scene to load
            {
                if (++i == argc)
                {
                    printf("Error: Missing <scene_id> argument, please see help (-h)\n");
                    return false;
                }

                std::istringstream iss(argv[i]);
                iss >> sceneID;

                if (iss.fail() || sceneID < 0 || sceneID >= (int32_t)Utils::ArrayLength(g_SceneConfigs))
                {
                    printf(
                        "Error: Invalid <scene_id> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }
            }
            else if (arg == "-em") // environment map
            {
                if (++i == argc)
                {
                    printf("Error: Missing <environment_map_id> argument, please see help (-h)\n");
                    return false;
                }

                if ((g_SceneConfigs[sceneID] & Scene::kLightEnv) == 0)
                {
                    printf(
                        "\n"
                        "Warning: You specified an environment map; however, "
                        "the scene was either not set yet or it doesn't use an environment map.\n\n");
                }

                std::istringstream iss(argv[i]);
                iss >> envMapID;

                if (iss.fail() || envMapID < 0 || envMapID >= Scene::kEMCount)
                {
                    printf(
                        "Error: Invalid <environment_map_id> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }
            }
            else if (arg == "-a") // algorithm to use
            {
                if (++i == argc)
                {
                    printf("Error: Missing <algorithm> argument, please see help (-h)\n");
                    return false;
                }

                std::string alg(argv[i]);
                for (int32_t algorithm=0; algorithm<kAlgorithmCount; algorithm++)
                    if (alg == Config::GetAcronym(Algorithm(algorithm)))
                        mAlgorithm = Algorithm(algorithm);

                if (mAlgorithm == kAlgorithmCount)
                {
                    printf(
                        "Error: Invalid <algorithm> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }
            }
            else if (arg == "-maxpl") // maximal path length
            {
                if (++i == argc)
                {
                    printf("Error: Missing <max_path_length> argument, please see help (-h)\n");
                    return false;
                }

                if (   (mAlgorithm != kPathTracingNaive)
                    && (mAlgorithm != kPathTracing))
                {
                    printf(
                        "\n"
                        "Warning: You specified maximal path length; however, "
                        "the rendering algorithm was either not set yet or it doesn't support this option.\n\n");
                }

                int32_t tmpPathLength;
                std::istringstream iss(argv[i]);
                iss >> tmpPathLength;

                if (iss.fail() || tmpPathLength < 0)
                {
                    printf(
                        "Error: Invalid <max_path_length> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }

                mMaxPathLength = tmpPathLength;
            }
            else if (arg == "-minpl") // minimal path length
            {
                if (++i == argc)
                {
                    printf("Error: Missing <min_path_length> argument, please see help (-h)\n");
                    return false;
                }

                if (   (mAlgorithm != kPathTracingNaive)
                    && (mAlgorithm != kPathTracing))
                {
                    printf(
                        "\n"
                        "Warning: You specified minimal path length; however, "
                        "the rendering algorithm was either not set yet or it doesn't support this option.\n\n");
                }

                int32_t tmpPathLength;
                std::istringstream iss(argv[i]);
                iss >> tmpPathLength;

                if (iss.fail() || tmpPathLength < 1)
                {
                    printf(
                        "Error: Invalid <min_path_length> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }

                mMinPathLength = tmpPathLength;
            }
            else if (arg == "-iic") // indirect illumination clipping value
            {
                if (++i == argc)
                {
                    printf("Error: Missing <indirect_illum_clipping_value> argument, please see help (-h)\n");
                    return false;
                }

                if (mAlgorithm != kPathTracing)
                {
                    printf(
                        "\n"
                        "Warning: You specified maximal allowed value for indirect illumination estimate; however, "
                        "the rendering algorithm was either not set yet or it doesn't support this option.\n\n");
                }

                float tmp;
                std::istringstream iss(argv[i]);
                iss >> tmp;

                if (iss.fail() || tmp < 0.f)
                {
                    printf(
                        "Error: Invalid <indirect_illum_clipping_value> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }

                mIndirectIllumClipping = tmp;
            }
            else if ((arg == "-sb") || (arg == "--splitting-budget")) // splitting budget
            {
                if (++i == argc)
                {
                    printf("Error: Missing <splitting_budget> argument, please see help (-h)\n");
                    return false;
                }

                if (mAlgorithm != kPathTracing)
                {
                    printf(
                        "\n"
                        "Warning: You specified maximal splitting; however, "
                        "the rendering algorithm was either not set yet or it doesn't support this option.\n\n");
                }

                int32_t tmp;
                std::istringstream iss(argv[i]);
                iss >> tmp;

                if (iss.fail() || tmp < 1)
                {
                    printf(
                        "Error: Invalid <splitting_budget> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }

                mSplittingBudget = tmp;
            }
            else if ((arg == "-sl")) // debug, temporary: splitting level
            {
                if (++i == argc)
                {
                    printf("Error: Missing <splitting_level> argument, please see help (-h)\n");
                    return false;
                }

                if (mAlgorithm != kPathTracing)
                {
                    printf(
                        "\n"
                        "Warning: You specified splitting ratio; however, "
                        "the rendering algorithm was either not set yet or it doesn't support this option.\n\n");
                }

                float tmp;
                std::istringstream iss(argv[i]);
                iss >> tmp;

                if (iss.fail() || tmp <= 0.f || tmp > 1.f)
                {
                    printf(
                        "Error: Invalid <splitting_level> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }

                mDbgSplittingLevel = tmp;
            }
            else if ((arg == "-slbr") || (arg == "--splitting-light-to-bsdf-ratio")) // debug, temporary: splitting light-to-bsdf samples ratio
            {
                if (++i == argc)
                {
                    printf("Error: Missing <splitting_light_to_bsdf_ratio> argument, please see help (-h)\n");
                    return false;
                }

                if (mAlgorithm != kPathTracing)
                {
                    printf(
                        "\n"
                        "Warning: You specified light-to-bsdf samples ratio for splitting; however, "
                        "the rendering algorithm was either not set yet or it doesn't support this option.\n\n");
                }

                float tmp;
                std::istringstream iss(argv[i]);
                iss >> tmp;

                if (iss.fail() || tmp <= 0.f)
                {
                    printf(
                        "Error: Invalid <splitting_light_to_bsdf_ratio> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }

                mDbgSplittingLightToBrdfSmplRatio = tmp;
            }
            else if (arg == "-i") // number of iterations to run
            {
                if (++i == argc)
                {
                    printf("Error: Missing <iterations> argument, please see help (-h)\n");
                    return false;
                }

                std::istringstream iss(argv[i]);
                iss >> mIterations;

                if (iss.fail() || mIterations < 1)
                {
                    printf(
                        "Error: Invalid <iterations> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }
            }
            else if (arg == "-t") // number of seconds to run
            {
                if (++i == argc)
                {
                    printf("Error: Missing <time> argument, please see help (-h)\n");
                    return false;
                }

                std::istringstream iss(argv[i]);
                iss >> mMaxTime;

                if (iss.fail() || mMaxTime < 0)
                {
                    printf(
                        "Error: Invalid <time> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }

                mIterations = -1; // time has precedence
            }
            else if (arg == "-e") // extension of default output file name
            {
                if (++i == argc)
                {
                    printf("Error: Missing <default_output_extension> argument, please see help (-h)\n");
                    return false;
                }

                mDefOutputExtension = argv[i];

                if (mDefOutputExtension != "bmp" && mDefOutputExtension != "hdr")
                {
                    printf(
                        "\n"
                        "Warning: The <default_output_extension> argument \"%s\" is neither \"bmp\" nor \"hdr\". Using \"bmp\".\n\n",
                        mDefOutputExtension.c_str());
                    mDefOutputExtension = "bmp";
                }
                else if (mDefOutputExtension.length() == 0)
                {
                    printf(
                        "Error: Invalid <default_output_extension> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }
            }
            else if (arg == "-o") // custom output file name
            {
                if (++i == argc)
                {
                    printf("Error: Missing <output_name> argument, please see help (-h)\n");
                    return false;
                }

                mOutputName = argv[i];

                if (mOutputName.length() == 0)
                {
                    printf(
                        "Error: Invalid <output_name> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }
            }
            else if (arg == "-od") // custom output directory
            {
                if (++i == argc)
                {
                    printf("Error: Missing <output_directory> argument, please see help (-h)\n");
                    return false;
                }

                mOutputDirectory = argv[i];

                if (mOutputDirectory.length() == 0)
                {
                    printf(
                        "Error: Invalid <output_directory> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }
            }
            else if (arg == "-ot") // output file name trail text
            {
                if (++i == argc)
                {
                    printf("Error: Missing <output_trail> argument, please see help (-h)\n");
                    return false;
                }

                std::string currentTrail = argv[i];
                if (currentTrail.length() == 0)
                {
                    printf(
                        "Error: Invalid <output_trail> argument \"%s\", please see help (-h)\n",
                        argv[i]);
                    return false;
                }

                outputNameTrail += currentTrail;
            }

            else if ((arg == "-auxf1") || (arg == "--dbg_aux_float1"))
            {
                if (!ProcessCommandlineParamFloat(argc, argv, i, mAuxDbgParams.float1, "dbg_aux_float1"))
                    return false;
            }
            else if ((arg == "-auxf2") || (arg == "--dbg_aux_float2"))
            {
                if (!ProcessCommandlineParamFloat(argc, argv, i, mAuxDbgParams.float2, "dbg_aux_float2"))
                    return false;
            }
            else if ((arg == "-auxf3") || (arg == "--dbg_aux_float3"))
            {
                if (!ProcessCommandlineParamFloat(argc, argv, i, mAuxDbgParams.float3, "dbg_aux_float3"))
                    return false;
            }
            else if ((arg == "-auxf4") || (arg == "--dbg_aux_float4"))
            {
                if (!ProcessCommandlineParamFloat(argc, argv, i, mAuxDbgParams.float4, "dbg_aux_float4"))
                    return false;
            }
            else if ((arg == "-auxf5") || (arg == "--dbg_aux_float5"))
            {
                if (!ProcessCommandlineParamFloat(argc, argv, i, mAuxDbgParams.float5, "dbg_aux_float5"))
                    return false;
            }

            else if ((arg == "-auxb1") || (arg == "--dbg_aux_bool1"))
            {
                if (!ProcessCommandlineParamBool(argc, argv, i, mAuxDbgParams.bool1, "dbg_aux_bool1"))
                    return false;
            }
            else if ((arg == "-auxb2") || (arg == "--dbg_aux_bool2"))
            {
                if (!ProcessCommandlineParamBool(argc, argv, i, mAuxDbgParams.bool2, "dbg_aux_bool2"))
                    return false;
            }
            else if ((arg == "-auxb3") || (arg == "--dbg_aux_bool3"))
            {
                if (!ProcessCommandlineParamBool(argc, argv, i, mAuxDbgParams.bool3, "dbg_aux_bool3"))
                    return false;
            }
            else if ((arg == "-auxb4") || (arg == "--dbg_aux_bool4"))
            {
                if (!ProcessCommandlineParamBool(argc, argv, i, mAuxDbgParams.bool4, "dbg_aux_bool4"))
                    return false;
            }
            else if ((arg == "-auxb5") || (arg == "--dbg_aux_bool5"))
            {
                if (!ProcessCommandlineParamBool(argc, argv, i, mAuxDbgParams.bool5, "dbg_aux_bool5"))
                    return false;
            }
        }

        // Check algorithm was selected
        if (mAlgorithm == kAlgorithmCount)
            mAlgorithm = kPathTracing;

        // Check path lengths settings
        if ((mMaxPathLength == 0) && (mMinPathLength != 1))
        {
            printf(
                "Error: Minimum path length different from 1 is set while Russian roulette was requested for ending paths, "
                "please see help (-h)\n");
            return false;
        }
        if ((mMaxPathLength != 0) && (mMinPathLength > mMaxPathLength))
        {
            printf(
                "Error: Minimum path length (%d) is larger than maximum path length (%d), please see help (-h)\n",
                mMinPathLength, mMaxPathLength);
            return false;
        }

        // Load scene
        Scene *scene = new Scene;
        scene->LoadCornellBox(
            mResolution, mAuxDbgParams, g_SceneConfigs[sceneID], Scene::EnvironmentMapType(envMapID));
        mScene = scene;

        // If no output name is chosen, create a default one
        if (mOutputName.length() == 0)
            mOutputName = DefaultFilename(g_SceneConfigs[sceneID], outputNameTrail);

        // If output name doesn't have a valid extension (.bmp or .hdr), use .bmp
        std::string extension = "";
        if (mOutputName.length() > 4) // there must be at least 1 character before .bmp
            extension = mOutputName.substr(mOutputName.length() - 4, 4);
        if (extension != ".bmp" && extension != ".hdr")
            mOutputName += ".bmp";

        return true;
    }

    const Scene             *mScene;

    bool                     mOnlyPrintOutputPath;

    int32_t                  mIterations;
    float                    mMaxTime;
    Framebuffer             *mFramebuffer;
    uint32_t                 mNumThreads;
    bool                     mQuietMode;
    int32_t                  mBaseSeed;
    std::string              mDefOutputExtension;
    std::string              mOutputName;
    std::string              mOutputDirectory;
    Vec2i                    mResolution;

    Algorithm                mAlgorithm;

    // Only used for path-based algorithms
    uint32_t                 mMaxPathLength;
    uint32_t                 mMinPathLength;

    // Only used in the NEE MIS path tracer
    float                    mIndirectIllumClipping;
    uint32_t                 mSplittingBudget;

    // debug, temporary
    float                    mDbgSplittingLevel;
    float                    mDbgSplittingLightToBrdfSmplRatio;    // Number of light samples per one BSDF sample.

    // Auxiliary debugging ad hoc parameters
    AuxDbgParams             mAuxDbgParams;
};
