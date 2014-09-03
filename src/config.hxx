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
		kAlgorithmMax
    };

    static const char* GetName(Algorithm aAlgorithm)
    {
        static const char* algorithmNames[7] =
        {
            "eye light",
            "path tracing"
        };

        if(aAlgorithm < 0 || aAlgorithm > 1)
            return "unknown algorithm";

        return algorithmNames[aAlgorithm];
    }

    static const char* GetAcronym(Algorithm aAlgorithm)
    {
        static const char* algorithmNames[7] = { "el", "pt" };

        if(aAlgorithm < 0 || aAlgorithm > 1)
            return "unknown";
        return algorithmNames[aAlgorithm];
    }

    const Scene *mScene;
    Algorithm   mAlgorithm;
    int         mIterations;
    float       mMaxTime;
    Framebuffer *mFramebuffer;
    int         mNumThreads;
    int         mBaseSeed;
    uint        mMaxPathLength;
    uint        mMinPathLength;
    std::string mOutputName;
    Vec2i       mResolution;
};

// Utility function, essentially a renderer factory
AbstractRenderer* CreateRenderer(
    const Config& aConfig,
    const int     aSeed)
{
    const Scene& scene = *aConfig.mScene;

    switch(aConfig.mAlgorithm)
    {
    case Config::kEyeLight:
        return new EyeLight(scene, aSeed);
    case Config::kPathTracing:
        return new PathTracer(scene, aSeed);
    default:
        printf("Unknown algorithm!!\n");
        exit(2);
    }
}

// Scene configurations
uint g_SceneConfigs[] = {
    Scene::kLightPoint   | Scene::kWalls | Scene::kSpheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightPoint   | Scene::kWalls | Scene::kSpheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,
    Scene::kLightCeiling | Scene::kWalls | Scene::kSpheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightCeiling | Scene::kWalls | Scene::kSpheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,
    Scene::kLightBox     | Scene::kWalls | Scene::kSpheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightBox     | Scene::kWalls | Scene::kSpheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy,
	Scene::kLightEnv     | Scene::kWalls | Scene::kSpheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse,
    Scene::kLightEnv     | Scene::kWalls | Scene::kSpheres | Scene::kWallsDiffuse | Scene::kSpheresDiffuse | Scene::kWallsGlossy | Scene::kSpheresGlossy
};

std::string DefaultFilename(
    const uint              aSceneConfig,
    const Scene             &aScene,
    const Config::Algorithm aAlgorithm)
{
    std::string filename;

    // We use scene acronym
    filename += aScene.mSceneAcronym;

    // We add acronym of the used algorithm
    filename += "_";
    filename += Config::GetAcronym(aAlgorithm);

    // And it will be written as bmp
    filename += ".bmp";

    return filename;
}

// Utility function, gives length of array
template <typename T, size_t N>
inline int SizeOfArray( const T(&)[ N ] )
{
    return int(N);
}

void PrintRngWarning()
{
#if defined(LEGACY_RNG)
    printf("The code was not compiled for C++11.\n");
    printf("It will be using Tiny Encryption Algorithm-based"
        "random number generator.\n");
    printf("This is worse than the Mersenne Twister from C++11.\n");
    printf("Consider setting up for C++11.\n");
    printf("Visual Studio 2010, and g++ 4.6.3 and later work.\n\n");
#endif
}

void PrintHelp(const char *argv[])
{
    printf("\n");
    printf("Usage: %s [ -s <scene_id> | -a <algorithm> |\n", argv[0]);
    printf("           -t <time> | -i <iteration> | -o <output_name> | --report ]\n\n");
    printf("    -s  Selects the scene (default 0):\n");

    for(int i = 0; i < SizeOfArray(g_SceneConfigs); i++)
        printf("          %d    %s\n", i, Scene::GetSceneName(g_SceneConfigs[i]).c_str());

    printf("    -a  Selects the rendering algorithm (default pt):\n");

    for(int i = 0; i < (int)Config::kAlgorithmMax; i++)
        printf("          %-3s  %s\n",
            Config::GetAcronym(Config::Algorithm(i)),
            Config::GetName(Config::Algorithm(i)));

    printf("    -t  Number of seconds to run the algorithm\n");
    printf("    -i  Number of iterations to run the algorithm (default 1)\n");
    printf("    -o  User specified output name, with extension .bmp or .hdr (default .bmp)\n");
    printf("\n    Note: Time (-t) takes precedence over iterations (-i) if both are defined\n");
}

// Parses command line, setting up config
void ParseCommandline(int argc, const char *argv[], Config &oConfig)
{
    // Parameters marked with [cmd] can be changed from command line
    oConfig.mScene         = NULL;                  // [cmd] When NULL, renderer will not run
    oConfig.mAlgorithm     = Config::kAlgorithmMax; // [cmd]
    oConfig.mIterations    = 1;                     // [cmd]
    oConfig.mMaxTime       = -1.f;                  // [cmd]
    oConfig.mOutputName    = "";                    // [cmd]
    oConfig.mNumThreads    = 0;
    oConfig.mBaseSeed      = 1234;
    oConfig.mMaxPathLength = 10;
    oConfig.mMinPathLength = 0;
    oConfig.mResolution    = Vec2i(512, 512);
    //oConfig.mFramebuffer   = NULL; // this is never set by any parameter

    int sceneID    = 0; // default 0

    // Load arguments
    for(int i=1; i<argc; i++)
    {
        std::string arg(argv[i]);

        // print help string (at any position)
        if(arg == "-h" || arg == "--help" || arg == "/?")
        {
            PrintHelp(argv);
            return;
        }

        if(arg[0] != '-') // all our commands start with -
        {
            continue;
        }
        else if(arg == "-s") // scene to load
        {
            if(++i == argc)
            {
                printf("Missing <sceneID> argument, please see help (-h)\n");
                return;
            }

            std::istringstream iss(argv[i]);
            iss >> sceneID;

            if(iss.fail() || sceneID < 0 || sceneID >= SizeOfArray(g_SceneConfigs))
            {
                printf("Invalid <sceneID> argument, please see help (-h)\n");
                return;
            }
        }
        else if(arg == "-a") // algorithm to use
        {
            if(++i == argc)
            {
                printf("Missing <algorithm> argument, please see help (-h)\n");
                return;
            }

            std::string alg(argv[i]);
            for(int i=0; i<Config::kAlgorithmMax; i++)
                if(alg == Config::GetAcronym(Config::Algorithm(i)))
                    oConfig.mAlgorithm = Config::Algorithm(i);

            if(oConfig.mAlgorithm == Config::kAlgorithmMax)
            {
                printf("Invalid <algorithm> argument, please see help (-h)\n");
                return;
            }
        }
        else if(arg == "-i") // number of iterations to run
        {
            if(++i == argc)
            {
                printf("Missing <iteration> argument, please see help (-h)\n");
                return;
            }

            std::istringstream iss(argv[i]);
            iss >> oConfig.mIterations;

            if(iss.fail() || oConfig.mIterations < 1)
            {
                printf("Invalid <iteration> argument, please see help (-h)\n");
                return;
            }
        }
        else if(arg == "-t") // number of seconds to run
        {
            if(++i == argc)
            {
                printf("Missing <time> argument, please see help (-h)\n");
                return;
            }

            std::istringstream iss(argv[i]);
            iss >> oConfig.mMaxTime;

            if(iss.fail() || oConfig.mMaxTime < 0)
            {
                printf("Invalid <time> argument, please see help (-h)\n");
                return;
            }

            oConfig.mIterations = -1; // time has precedence
        }
        else if(arg == "-o") // number of seconds to run
        {
            if(++i == argc)
            {
                printf("Missing <output_name> argument, please see help (-h)\n");
                return;
            }

            oConfig.mOutputName = argv[i];

            if(oConfig.mOutputName.length() == 0)
            {
                printf("Invalid <output_name> argument, please see help (-h)\n");
                return;
            }
        }
    }

    // Check algorithm was selected
    if(oConfig.mAlgorithm == Config::kAlgorithmMax)
    {
		oConfig.mAlgorithm = Config::kPathTracing;
    }

    // Load scene
    Scene *scene = new Scene;
    scene->LoadCornellBox(oConfig.mResolution, g_SceneConfigs[sceneID]);

    oConfig.mScene = scene;

    // If no output name is chosen, create a default one
    if(oConfig.mOutputName.length() == 0)
    {
        oConfig.mOutputName = DefaultFilename(g_SceneConfigs[sceneID],
            *oConfig.mScene, oConfig.mAlgorithm);
    }

    // Check if output name has valid extension (.bmp or .hdr) and if not add .bmp
    std::string extension = "";

    if(oConfig.mOutputName.length() > 4) // must be at least 1 character before .bmp
        extension = oConfig.mOutputName.substr(
            oConfig.mOutputName.length() - 4, 4);

    if(extension != ".bmp" && extension != ".hdr")
        oConfig.mOutputName += ".bmp";
}
