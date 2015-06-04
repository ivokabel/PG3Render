#pragma once

#include "math.hxx"
#include "ray.hxx"
#include "geometry.hxx"
#include "camera.hxx"
#include "framebuffer.hxx"
#include "scene.hxx"
#include "types.hxx"
#include "hardsettings.hxx"

#include <omp.h>
#include <string>
#include <set>
#include <sstream>
#include <vector>
#include <cmath>
#include <time.h>
#include <cstdlib>

enum Algorithm
{
    kEyeLight,
    kDirectIllumBRDFSampling,
    kDirectIllumLightSamplingAll,
    kDirectIllumLightSamplingSingle,
    kDirectIllumMIS,
    kPathTracingNaive,
    kPathTracing,
    kAlgorithmCount
};

// Renderer configuration, holds algorithm, scene, and all other settings
struct Config
{
    static const char* GetName(Algorithm aAlgorithm)
    {
        static const char* algorithmNames[kAlgorithmCount] =
        {
            "eye light",
            "direct illumination - BRDF sampling",
            "direct illumination - light sampling (all)",
            "direct illumination - light sampling (single sample)",
            "direct illumination - multiple importance sampling",
            "naive path tracing",
            "path tracing (next event estimate, multiple importance sampling)",
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

    const Scene *mScene;

    bool         mOnlyPrintOutputPath;

    int32_t      mIterations;
    float        mMaxTime;
    Framebuffer *mFramebuffer;
    uint32_t     mNumThreads;
    bool         mQuietMode;
    int32_t      mBaseSeed;
    std::string  mDefOutputExtension;
    std::string  mOutputName;
    std::string  mOutputDirectory;
    Vec2i        mResolution;

    Algorithm    mAlgorithm;

    // Only used for path-based algorithms
    uint32_t     mMaxPathLength;
    uint32_t     mMinPathLength;

    // Only used in the NEE MIS path tracer
    float        mIndirectIllumClipping;
    uint32_t     mSplittingBudget;

    // debug, temporary
    float        mDbgSplittingLevel;
    float        mDbgSplittingLightToBrdfSmplRatio;    // Number of light samples per one brdf sample.
};

// Scene configurations
uint32_t g_SceneConfigs[] = 
{
    // Point light, box: 0,1
    Scene::kLightPoint   | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightPoint   | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,

    // Ceiling light, box: 2,3
    Scene::kLightCeiling | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightCeiling | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,

    // Light box, box: 4,5
    Scene::kLightBox     | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightBox     | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,

    // Environment map, box: 6,7
    Scene::kLightEnv     | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightEnv     | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,

    // Environment map, no walls: 8,9
    Scene::kLightEnv                     | Scene::kFloor | Scene::kWallsDiffuse | Scene::k2Spheres | Scene::kSpheresDiffuse,
    Scene::kLightEnv                     | Scene::kFloor | Scene::kWallsDiffuse | Scene::k2Spheres | Scene::kSpheresDiffuse | Scene::kSpheresGlossy,

    // EM testing: 10, 11, 12
    Scene::kLightEnv | Scene::k1Sphere | Scene::kSpheresDiffuse,
    Scene::kLightEnv | Scene::k1Sphere | Scene::kSpheresGlossy,
    Scene::kLightEnv | Scene::k1Sphere | Scene::kSpheresDiffuse | Scene::kSpheresGlossy,

    // Multiple lights, diffuse: 13, 14, 15, 16
                         Scene::kLightBox | Scene::kLightEnv | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightPoint | Scene::kLightBox |                    Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightPoint | Scene::kLightBox | Scene::kLightEnv | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightPoint | Scene::kLightBox | Scene::kLightEnv |                 Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,

    // Multiple lights, glossy: 17, 18
    Scene::kLightPoint | Scene::kLightBox | Scene::kLightEnv |                 Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,
    Scene::kLightPoint | Scene::kLightBox | Scene::kLightEnv | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,
};

std::string DefaultFilename(
    const uint32_t          aSceneConfig,
    const Config          & aConfig,
    std::string             aOutputNameTrail
    )
{
    aSceneConfig; // unused parameter

    std::string filename;

    filename += aConfig.mScene->mSceneAcronym;

    // Algorithm acronym
    filename += "_";
    filename += Config::GetAcronym(aConfig.mAlgorithm);

    // Path length
    if (   (aConfig.mAlgorithm == kPathTracingNaive)
        || (aConfig.mAlgorithm == kPathTracing))
    {
        filename += "_";
        if (aConfig.mMaxPathLength == 0)
            filename += "rr";
        else
            filename +=
                  "pl"
                + std::to_string(aConfig.mMinPathLength)
                + "-"
                + std::to_string(aConfig.mMaxPathLength);
    }

    // Splitting settings
    if (aConfig.mAlgorithm == kPathTracing)
    {
        filename += "_splt" + std::to_string(aConfig.mSplittingBudget);

        // debug, temporary: indirect illum splitting level
        std::ostringstream outStream2;
        outStream2.precision(1);
        outStream2 << std::fixed << aConfig.mDbgSplittingLevel;
        filename += "," + outStream2.str();

        // debug, temporary: light-to-brdf samples ratio
        std::ostringstream outStream1;
        outStream1.precision(1);
        outStream1 << std::fixed << aConfig.mDbgSplittingLightToBrdfSmplRatio;
        filename += "," + outStream1.str();
    }

    // Indirect illumination clipping
    if ((aConfig.mAlgorithm == kPathTracing) && (aConfig.mIndirectIllumClipping > 0.f))
    {
        filename += "_iic";
        std::ostringstream outStream;
        outStream.precision(1);
        outStream << std::fixed << aConfig.mIndirectIllumClipping;
        filename += outStream.str();
    }

#ifndef ENVMAP_USE_IMPORTANCE_SAMPLING
    // Debug info
    if (   (aConfig.mAlgorithm >= kDirectIllumLightSamplingAll)
        && (aConfig.mAlgorithm <= kDirectIllumMIS))
    {
        filename += "_emcw";
    }
#endif

    // Sample count
    filename += "_";
    if (aConfig.mMaxTime > 0)
    {
        //filename += std::to_string(aConfig.mMaxTime);
        std::ostringstream outStream;
        outStream.width(2);
        outStream.fill('0');
        outStream << aConfig.mMaxTime;
        filename += outStream.str();

        filename += "sec";
    }
    else
    {
        filename += std::to_string(aConfig.mIterations);
        filename += "s";
    }

    // Custom trail text
    if (!aOutputNameTrail.empty())
    {
        filename += "_";
        filename += aOutputNameTrail;
    }

    // The chosen otuput format extension
    filename += "." + aConfig.mDefOutputExtension;

    return filename;
}

// Utility function, gives length of array
template <typename T, size_t N>
inline int32_t SizeOfArray( const T(&)[ N ] )
{
    return int32_t(N);
}

void PrintRngWarning()
{
#if defined(LEGACY_RNG)
    printf("The code was not compiled for C++11.\n");
    printf("It will be using Tiny Encryption Algorithm-based random number generator.\n");
    printf("This is worse than the Mersenne Twister from C++11.\n");
    printf("Consider setting up for C++11.\n");
    printf("Visual Studio 2010, and g++ 4.6.3 and later work.\n\n");
#endif
}

void PrintConfiguration(const Config &config)
{
    if (config.mQuietMode)
        return;

    printf("PG3Render\n");

    printf("Scene:     %s\n", config.mScene->mSceneName.c_str());
    printf("Algorithm: %s", config.GetName(config.mAlgorithm));
    if (   (config.mAlgorithm == kPathTracingNaive)
        || (config.mAlgorithm == kPathTracing))
    {
        if (config.mMaxPathLength == 0)
            printf(", Russian roulette path ending");
        else
            printf(", path lengths: %d-%d", config.mMinPathLength, config.mMaxPathLength);
    }
    if ((config.mAlgorithm == kPathTracing) && (config.mIndirectIllumClipping > 0.f))
        printf(", indirect illum. clipping: %.2f", config.mIndirectIllumClipping);
    if (config.mAlgorithm == kPathTracing)
    {
        // debug, temporary
        printf(
            ", splitting: %d, %.1f, %.1f",
            config.mSplittingBudget,
            config.mDbgSplittingLevel,
            config.mDbgSplittingLightToBrdfSmplRatio);
    }
    printf("\n");

    if (config.mMaxTime > 0)
        printf("Target:    %g seconds render time\n", config.mMaxTime);
    else
        printf("Target:    %d iteration(s)\n", config.mIterations);

    if (!config.mOutputDirectory.empty())
        printf("Out dir:   %s\n", config.mOutputDirectory.c_str());
    printf("Out name:  %s\n", config.mOutputName.c_str());

    printf("Config:    "
        "%d threads, "
        #if !defined _DEBUG
            "release"
        #else
            "debug"
        #endif
        #ifdef PG3_ASSERT_ENABLED
            " with assertions"
        #endif
        "\n",
        config.mNumThreads
        );

    fflush(stdout);
}

void PrintHelp(const char *argv[])
{
    std::string filename;
    if (!GetFileName(argv[0], filename))
        filename = argv[0];

    printf("\n");
    printf(
        "Usage: %s "
        "[-s <scene_id>] [-a <algorithm>] [-t <time> | -i <iterations>] [-minpl <min_path_length>] [-maxpl <max_path_length>] [-iic <indirect_illum_clipping_value>] [-sb|--splitting-budget <splitting_budget>] [-slbr|--splitting-light-to-brdf-ratio <splitting_light_to_brdf_ratio>] [-em <env_map_type>] [-e <def_output_ext>] [-od <output_directory>] [-o <output_name>] [-ot <output_trail>] [-j <threads_count>] [-q] [-opop|--only-print-output-pathname] \n\n",
        filename.c_str());

    printf("    -s     Selects the scene (default 0):\n");
    for (int32_t i = 0; i < SizeOfArray(g_SceneConfigs); i++)
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
    printf("    -slbr | --splitting-light-to-brdf-ratio \n");
    printf("           Number of light samples per one brdf sample (default 1.0)\n");

    printf("    -t     Number of seconds to run the algorithm\n");
    printf("    -i     Number of iterations to run the algorithm (default 1)\n");
    printf("    -e     Extension of the default output file: bmp or hdr (default bmp)\n");
    printf("    -od    User specified directory for the output, whose existence is not checked (default \"\")\n");
    printf("    -o     User specified output name, with extension .bmp or .hdr (default .bmp)\n");
    printf("    -ot    Trail text to be added at the end the output file name\n");
    printf("           (only used to alter a default filename; '_' is pasted automatically before the trail).\n");
    printf("    -j     Number of threads (\"jobs\") to be used\n");
    printf("    -q     Quiet mode - doesn't print anything except for warnings and errors\n");

    printf("    -opop | --only-print-output-pathname \n");
    printf("           Do not render anything; just print the full path of the current output file.\n");

    printf("\n    Note: Time (-t) takes precedence over iterations (-i) if both are defined\n");
}

// Parses command line, setting up config
bool ParseCommandline(int32_t argc, const char *argv[], Config &oConfig)
{
    // Parameters marked with [cmd] can be changed from command line
    oConfig.mScene                          = NULL;                     // [cmd] When NULL, renderer will not run

    oConfig.mOnlyPrintOutputPath            = false;                    // [cmd]

    oConfig.mIterations                     = 1;                        // [cmd]
    oConfig.mMaxTime                        = -1.f;                     // [cmd]
    oConfig.mDefOutputExtension             = "bmp";                    // [cmd]
    oConfig.mOutputName                     = "";                       // [cmd]
    oConfig.mOutputDirectory                = "";                       // [cmd]
    oConfig.mNumThreads                     = 0;
    oConfig.mQuietMode                      = false;
    oConfig.mBaseSeed                       = 1234;
    oConfig.mResolution                     = Vec2i(512, 512);

    oConfig.mAlgorithm                      = kAlgorithmCount;          // [cmd]
    oConfig.mMinPathLength                  = 1;                        // [cmd]
    oConfig.mMaxPathLength                  = 0;                        // [cmd]
    oConfig.mIndirectIllumClipping          = 0.f;                      // [cmd]
    oConfig.mSplittingBudget                = 4;                        // [cmd]

    // debug, temporary
    oConfig.mDbgSplittingLevel = 1.f;                                   // [cmd]
    oConfig.mDbgSplittingLightToBrdfSmplRatio = 1.f;                    // [cmd]

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
            oConfig.mOnlyPrintOutputPath = true;
        }
        else if (arg == "-j") // jobs (number of threads)
        {
            if (++i == argc)
            {
                printf("Error: Missing <threads_count> argument, please see help (-h)\n");
                return false;
            }

            std::istringstream iss(argv[i]);
            iss >> oConfig.mNumThreads;

            if (iss.fail() || oConfig.mNumThreads <= 0)
            {
                printf("Error: Invalid <threads_count> argument, please see help (-h)\n");
                return false;
            }
        }
        else if (arg == "-q") // quiet mode
        {
            oConfig.mQuietMode = true;
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

            if (iss.fail() || sceneID < 0 || sceneID >= SizeOfArray(g_SceneConfigs))
            {
                printf("Error: Invalid <scene_id> argument, please see help (-h)\n");
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
                printf("Error: Invalid <environment_map_id> argument, please see help (-h)\n");
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
            for (int32_t i=0; i<kAlgorithmCount; i++)
                if (alg == Config::GetAcronym(Algorithm(i)))
                    oConfig.mAlgorithm = Algorithm(i);

            if (oConfig.mAlgorithm == kAlgorithmCount)
            {
                printf("Error: Invalid <algorithm> argument, please see help (-h)\n");
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

            if (   (oConfig.mAlgorithm != kPathTracingNaive)
                && (oConfig.mAlgorithm != kPathTracing))
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
                printf("Error: Invalid <max_path_length> argument, please see help (-h)\n");
                return false;
            }

            oConfig.mMaxPathLength = tmpPathLength;
        }
        else if (arg == "-minpl") // minimal path length
        {
            if (++i == argc)
            {
                printf("Error: Missing <min_path_length> argument, please see help (-h)\n");
                return false;
            }

            if (   (oConfig.mAlgorithm != kPathTracingNaive)
                && (oConfig.mAlgorithm != kPathTracing))
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

            oConfig.mMinPathLength = tmpPathLength;
        }
        else if (arg == "-iic") // indirect illumination clipping value
        {
            if (++i == argc)
            {
                printf("Error: Missing <indirect_illum_clipping_value> argument, please see help (-h)\n");
                return false;
            }

            if (oConfig.mAlgorithm != kPathTracing)
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

            oConfig.mIndirectIllumClipping = tmp;
        }
        else if ((arg == "-sb") || (arg == "--splitting-budget")) // splitting budget
        {
            if (++i == argc)
            {
                printf("Error: Missing <splitting_budget> argument, please see help (-h)\n");
                return false;
            }

            if (oConfig.mAlgorithm != kPathTracing)
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

            oConfig.mSplittingBudget = tmp;
        }
        else if ((arg == "-sl")) // debug, temporary: splitting level
        {
            if (++i == argc)
            {
                printf("Error: Missing <splitting_level> argument, please see help (-h)\n");
                return false;
            }

            if (oConfig.mAlgorithm != kPathTracing)
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

            oConfig.mDbgSplittingLevel = tmp;
        }
        else if ((arg == "-slbr") || (arg == "--splitting-light-to-brdf-ratio")) // debug, temporary: splitting light-to-brdf samples ratio
        {
            if (++i == argc)
            {
                printf("Error: Missing <splitting_light_to_brdf_ratio> argument, please see help (-h)\n");
                return false;
            }

            if (oConfig.mAlgorithm != kPathTracing)
            {
                printf(
                    "\n"
                    "Warning: You specified light-to-brdf samples ratio for splitting; however, "
                    "the rendering algorithm was either not set yet or it doesn't support this option.\n\n");
            }

            float tmp;
            std::istringstream iss(argv[i]);
            iss >> tmp;

            if (iss.fail() || tmp <= 0.f)
            {
                printf(
                    "Error: Invalid <splitting_light_to_brdf_ratio> argument \"%s\", please see help (-h)\n",
                    argv[i]);
                return false;
            }

            oConfig.mDbgSplittingLightToBrdfSmplRatio = tmp;
        }
        else if (arg == "-i") // number of iterations to run
        {
            if (++i == argc)
            {
                printf("Error: Missing <iterations> argument, please see help (-h)\n");
                return false;
            }

            std::istringstream iss(argv[i]);
            iss >> oConfig.mIterations;

            if (iss.fail() || oConfig.mIterations < 1)
            {
                printf("Error: Invalid <iterations> argument, please see help (-h)\n");
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
            iss >> oConfig.mMaxTime;

            if (iss.fail() || oConfig.mMaxTime < 0)
            {
                printf("Error: Invalid <time> argument, please see help (-h)\n");
                return false;
            }

            oConfig.mIterations = -1; // time has precedence
        }
        else if (arg == "-e") // extension of default output file name
        {
            if (++i == argc)
            {
                printf("Error: Missing <default_output_extension> argument, please see help (-h)\n");
                return false;
            }

            oConfig.mDefOutputExtension = argv[i];

            if (oConfig.mDefOutputExtension != "bmp" && oConfig.mDefOutputExtension != "hdr")
            {
                printf(
                    "\n"
                    "Warning: The <default_output_extension> argument \"%s\" is neither \"bmp\" nor \"hdr\". Using \"bmp\".\n\n",
                    oConfig.mDefOutputExtension.c_str());
                oConfig.mDefOutputExtension = "bmp";
            }
            else if (oConfig.mDefOutputExtension.length() == 0)
            {
                printf("Error: Invalid <default_output_extension> argument, please see help (-h)\n");
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

            oConfig.mOutputName = argv[i];

            if (oConfig.mOutputName.length() == 0)
            {
                printf("Error: Invalid <output_name> argument, please see help (-h)\n");
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

            oConfig.mOutputDirectory = argv[i];

            if (oConfig.mOutputDirectory.length() == 0)
            {
                printf("Error: Invalid <output_directory> argument, please see help (-h)\n");
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

            outputNameTrail = argv[i];

            if (outputNameTrail.length() == 0)
            {
                printf("Error: Invalid <output_trail> argument, please see help (-h)\n");
                return false;
            }
        }
    }

    // Check algorithm was selected
    if (oConfig.mAlgorithm == kAlgorithmCount)
        oConfig.mAlgorithm = kPathTracing;

    // Check path lengths settings
    if ((oConfig.mMaxPathLength == 0) && (oConfig.mMinPathLength != 1))
    {
        printf(
            "Error: Minimum path length different from 1 is set while Russian roulette was requested for ending paths, "
            "please see help (-h)\n");
        return false;
    }
    if ((oConfig.mMaxPathLength != 0) && (oConfig.mMinPathLength > oConfig.mMaxPathLength))
    {
        printf(
            "Error: Minimum path length (%d) is larger than maximum path length (%d), please see help (-h)\n",
            oConfig.mMinPathLength, oConfig.mMaxPathLength);
        return false;
    }

    // Load scene
    Scene *scene = new Scene;
    scene->LoadCornellBox(oConfig.mResolution, g_SceneConfigs[sceneID], envMapID);
    oConfig.mScene = scene;

    // If no output name is chosen, create a default one
    if (oConfig.mOutputName.length() == 0)
        oConfig.mOutputName = DefaultFilename(g_SceneConfigs[sceneID], oConfig, outputNameTrail);

    // If output name doesn't have a valid extension (.bmp or .hdr), use .bmp
    std::string extension = "";
    if (oConfig.mOutputName.length() > 4) // there must be at least 1 character before .bmp
        extension = oConfig.mOutputName.substr(oConfig.mOutputName.length() - 4, 4);
    if (extension != ".bmp" && extension != ".hdr")
        oConfig.mOutputName += ".bmp";

    return true;
}
