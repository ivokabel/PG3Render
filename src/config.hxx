#pragma once

#include <vector>
#include <cmath>
#include <time.h>
#include <cstdlib>
#include "math.hxx"
#include "ray.hxx"
#include "geometry.hxx"
#include "camera.hxx"
#include "framebuffer.hxx"
#include "scene.hxx"
#include "eyelight.hxx"
#include "pathtracer.hxx"
#include "types.hxx"

#include <omp.h>
#include <string>
#include <set>
#include <sstream>

// Renderer configuration, holds algorithm, scene, and all other settings
struct Config
{
    enum Algorithm
    {
        kEyeLight,
        kPathTracing,
        kAlgorithmCount
    };

    static const char* GetName(Algorithm aAlgorithm)
    {
        static const char* algorithmNames[7] =
        {
            "eye light",
            "path tracing"
        };

        if (aAlgorithm < 0 || aAlgorithm > 1)
            return "unknown algorithm";

        return algorithmNames[aAlgorithm];
    }

    static const char* GetAcronym(Algorithm aAlgorithm)
    {
        static const char* algorithmNames[7] = { "el", "pt" };

        if (aAlgorithm < 0 || aAlgorithm > 1)
            return "unknown";
        return algorithmNames[aAlgorithm];
    }

    const Scene *mScene;
    Algorithm    mAlgorithm;
    int32_t      mIterations;
    float        mMaxTime;
    Framebuffer *mFramebuffer;
    uint32_t     mNumThreads;
    int32_t      mBaseSeed;
    uint         mMaxPathLength;
    uint         mMinPathLength;
    std::string  mDefOutputExtension;
    std::string  mOutputName;
    Vec2i        mResolution;
};

// Utility function, essentially a renderer factory
AbstractRenderer* CreateRenderer(
    const Config&   aConfig,
    const int32_t   aSeed)
{
    const Scene& scene = *aConfig.mScene;

    switch(aConfig.mAlgorithm)
    {
    case Config::kEyeLight:
        return new EyeLight(scene, aSeed);
    case Config::kPathTracing:
        return new PathTracer(scene, aSeed);
    default:
        PG3_FATAL_ERROR("Unknown algorithm!!");
    }
}

// Scene configurations
uint g_SceneConfigs[] = 
{
    // 0,1
    Scene::kLightPoint   | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightPoint   | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,

    // 2,3
    Scene::kLightCeiling | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightCeiling | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,

    // 4,5
    Scene::kLightBox     | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightBox     | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,

    // 6,7
    Scene::kLightEnv     | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightEnv     | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,

    // 8,9
    Scene::kLightEnv | Scene::kFloor | Scene::kWallsDiffuse | Scene::k2Spheres | Scene::kSpheresDiffuse,
    Scene::kLightEnv | Scene::kFloor | Scene::kWallsDiffuse | Scene::k2Spheres | Scene::kSpheresDiffuse | Scene::kSpheresGlossy,

    // 10, 11, 12
    Scene::kLightEnv | Scene::k1Sphere | Scene::kSpheresDiffuse,
    Scene::kLightEnv | Scene::k1Sphere | Scene::kSpheresGlossy,
    Scene::kLightEnv | Scene::k1Sphere | Scene::kSpheresDiffuse | Scene::kSpheresGlossy,

    // 13, 14, 15, 16
                         Scene::kLightBox | Scene::kLightEnv | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightPoint | Scene::kLightBox |                    Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightPoint | Scene::kLightBox | Scene::kLightEnv | Scene::kWalls | Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightPoint | Scene::kLightBox | Scene::kLightEnv |                 Scene::kFloor | Scene::k2Spheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
};

std::string DefaultFilename(
    const uint              aSceneConfig,
    const Config          & aConfig
    )
{
    aSceneConfig; // unused parameter

    std::string filename;

    // We use scene acronym
    filename += aConfig.mScene->mSceneAcronym;

    // We add acronym of the used algorithm
    filename += "_";
    filename += Config::GetAcronym(aConfig.mAlgorithm);

    // And it will be written in the chosen format
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

void PrintInfo(const Config &config)
{
    printf("PG3Render\n");

    printf("Scene:     %s\n", config.mScene->mSceneName.c_str());
    printf("Config:    "
        "%d threads, "
#if !defined _DEBUG
        "release"
#elif defined PG3_DEBUG_ASSERT_ENABLED
        "debug with assertions"
#else
        "debug"
#endif
        "\n",
        config.mNumThreads
        );
    if (config.mMaxTime > 0)
        printf("Target:    %g seconds render time\n", config.mMaxTime);
    else
        printf("Target:    %d iteration(s)\n", config.mIterations);
}

void PrintHelp(const char *argv[])
{
    printf("\n");
    printf(
        "Usage: %s [ -s <scene_id> | -em <env_map_type> | -a <algorithm> | -t <time> | -i <iterations> | -e <def_output_ext> | -o <output_name> | -j <threads_count> ]\n\n", 
        argv[0]);

    printf("    -s  Selects the scene (default 0):\n");
    for (int32_t i = 0; i < SizeOfArray(g_SceneConfigs); i++)
        printf("          %2d   %s\n", i, Scene::GetSceneName(g_SceneConfigs[i]).c_str());

    printf("    -em Selects the environment map type (default 0; ignored if the scene doesn't use an environment map):\n");
    for (int32_t i = 0; i < Scene::kEMCount; i++)
        printf("          %2d   %s\n", i, Scene::GetEnvMapName(i).c_str());

    printf("    -a  Selects the rendering algorithm (default pt):\n");
    for (int32_t i = 0; i < (int32_t)Config::kAlgorithmCount; i++)
        printf("          %-3s  %s\n",
            Config::GetAcronym(Config::Algorithm(i)),
            Config::GetName(Config::Algorithm(i)));

    printf("    -t  Number of seconds to run the algorithm\n");
    printf("    -i  Number of iterations to run the algorithm (default 1)\n");
    printf("    -e  Extension of the default output file: bmp or hdr (default bmp)\n");
    printf("    -o  User specified output name, with extension .bmp or .hdr (default .bmp)\n");
    printf("    -j  Number of threads (\"jobs\") to be used\n");
    printf("\n    Note: Time (-t) takes precedence over iterations (-i) if both are defined\n");
}

// Parses command line, setting up config
void ParseCommandline(int32_t argc, const char *argv[], Config &oConfig)
{
    // Parameters marked with [cmd] can be changed from command line
    oConfig.mScene              = NULL;                     // [cmd] When NULL, renderer will not run
    oConfig.mAlgorithm          = Config::kAlgorithmCount;  // [cmd]
    oConfig.mIterations         = 1;                        // [cmd]
    oConfig.mMaxTime            = -1.f;                     // [cmd]
    oConfig.mDefOutputExtension = "bmp";                    // [cmd]
    oConfig.mOutputName         = "";                       // [cmd]
    oConfig.mNumThreads         = 0;
    oConfig.mBaseSeed           = 1234;
    oConfig.mMaxPathLength      = 10;
    oConfig.mMinPathLength      = 0;
    oConfig.mResolution         = Vec2i(512, 512);
    //oConfig.mFramebuffer      = NULL; // this is never set by any parameter

    int32_t sceneID     = 0; // default 0
    uint32_t envMapID   = Scene::kEMDefault;

    // Load arguments
    for (int32_t i=1; i<argc; i++)
    {
        std::string arg(argv[i]);

        // print help string (at any position)
        if (arg == "-h" || arg == "--help" || arg == "/?")
        {
            PrintHelp(argv);
            return;
        }

        if (arg[0] != '-') // all our commands start with -
        {
            continue;
        }
        else if (arg == "-j") // jobs (number of threads)
        {
            if (++i == argc)
            {
                printf("Missing <threads_count> argument, please see help (-h)\n");
                return;
            }

            std::istringstream iss(argv[i]);
            iss >> oConfig.mNumThreads;

            if (iss.fail() || oConfig.mNumThreads <= 0)
            {
                printf("Invalid <threads_count> argument, please see help (-h)\n");
                return;
            }
        }
        else if (arg == "-s") // scene to load
        {
            if (++i == argc)
            {
                printf("Missing <scene_id> argument, please see help (-h)\n");
                return;
            }

            std::istringstream iss(argv[i]);
            iss >> sceneID;

            if (iss.fail() || sceneID < 0 || sceneID >= SizeOfArray(g_SceneConfigs))
            {
                printf("Invalid <scene_id> argument, please see help (-h)\n");
                return;
            }
        }
        else if (arg == "-em") // environment map
        {
            if (++i == argc)
            {
                printf("Missing <environment_map_id> argument, please see help (-h)\n");
                return;
            }

            if ((g_SceneConfigs[sceneID] & Scene::kLightEnv) == 0)
            {
                printf(
                    "Warning: You specified an environment map; however, "
                    "the scene was either not set yet or it doesn't use an environment map.\n\n");
            }

            std::istringstream iss(argv[i]);
            iss >> envMapID;

            if (iss.fail() || envMapID < 0 || envMapID >= Scene::kEMCount)
            {
                printf("Invalid <environment_map_id> argument, please see help (-h)\n");
                return;
            }
        }
        else if (arg == "-a") // algorithm to use
        {
            if (++i == argc)
            {
                printf("Missing <algorithm> argument, please see help (-h)\n");
                return;
            }

            std::string alg(argv[i]);
            for (int32_t i=0; i<Config::kAlgorithmCount; i++)
                if (alg == Config::GetAcronym(Config::Algorithm(i)))
                    oConfig.mAlgorithm = Config::Algorithm(i);

            if (oConfig.mAlgorithm == Config::kAlgorithmCount)
            {
                printf("Invalid <algorithm> argument, please see help (-h)\n");
                return;
            }
        }
        else if (arg == "-i") // number of iterations to run
        {
            if (++i == argc)
            {
                printf("Missing <iterations> argument, please see help (-h)\n");
                return;
            }

            std::istringstream iss(argv[i]);
            iss >> oConfig.mIterations;

            if (iss.fail() || oConfig.mIterations < 1)
            {
                printf("Invalid <iterations> argument, please see help (-h)\n");
                return;
            }
        }
        else if (arg == "-t") // number of seconds to run
        {
            if (++i == argc)
            {
                printf("Missing <time> argument, please see help (-h)\n");
                return;
            }

            std::istringstream iss(argv[i]);
            iss >> oConfig.mMaxTime;

            if (iss.fail() || oConfig.mMaxTime < 0)
            {
                printf("Invalid <time> argument, please see help (-h)\n");
                return;
            }

            oConfig.mIterations = -1; // time has precedence
        }
        else if (arg == "-e") // extension of default output file name
        {
            if (++i == argc)
            {
                printf("Missing <default_output_extension> argument, please see help (-h)\n");
                return;
            }

            oConfig.mDefOutputExtension = argv[i];

            if (oConfig.mDefOutputExtension != "bmp" && oConfig.mDefOutputExtension != "hdr")
            {
                printf(
                    "The <default_output_extension> argument \"%s\" is neither \"bmp\" nor \"hdr\". Using \"bmp\".\n", 
                    oConfig.mDefOutputExtension.c_str());
                oConfig.mDefOutputExtension = "bmp";
            }
            else if (oConfig.mDefOutputExtension.length() == 0)
            {
                printf("Invalid <default_output_extension> argument, please see help (-h)\n");
                return;
            }
        }
        else if (arg == "-o") // custom output file name
        {
            if (++i == argc)
            {
                printf("Missing <output_name> argument, please see help (-h)\n");
                return;
            }

            oConfig.mOutputName = argv[i];

            if (oConfig.mOutputName.length() == 0)
            {
                printf("Invalid <output_name> argument, please see help (-h)\n");
                return;
            }
        }
    }

    // Check algorithm was selected
    if (oConfig.mAlgorithm == Config::kAlgorithmCount)
    {
        oConfig.mAlgorithm = Config::kPathTracing;
    }

    // Load scene
    Scene *scene = new Scene;
    scene->LoadCornellBox(oConfig.mResolution, g_SceneConfigs[sceneID], envMapID);

    oConfig.mScene = scene;

    // If no output name is chosen, create a default one
    if (oConfig.mOutputName.length() == 0)
    {
        oConfig.mOutputName = DefaultFilename(g_SceneConfigs[sceneID], oConfig);
    }

    // Check if output name has valid extension (.bmp or .hdr) and if not add .bmp
    std::string extension = "";

    if (oConfig.mOutputName.length() > 4) // must be at least 1 character before .bmp
        extension = oConfig.mOutputName.substr(
            oConfig.mOutputName.length() - 4, 4);

    if (extension != ".bmp" && extension != ".hdr")
        oConfig.mOutputName += ".bmp";
}
