#include "math.hxx"
#include "ray.hxx"
#include "geometry.hxx"
#include "camera.hxx"
#include "framebuffer.hxx"
#include "scene.hxx"
#include "eyelight.hxx"
#include "directillumination.hxx"
#include "pathtracer.hxx"
#include "config.hxx"
#include "process.hxx"

#include <omp.h>
#include <string>
#include <set>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <time.h>

// Renderer factory
AbstractRenderer* CreateRenderer(
    const Config&   aConfig,
    const int32_t   aSeed)
{
    switch (aConfig.mAlgorithm)
    {
    case kEyeLight:
        return new EyeLight(aConfig, aSeed);

    case kDirectIllumLightSamplingAll:
    case kDirectIllumLightSamplingSingle:
    case kDirectIllumBRDFSampling:
    case kDirectIllumMIS:
        return new DirectIllumination(aConfig, aSeed);

    case kPathTracingNaive:
    case kPathTracing:
        return new PathTracer(aConfig, aSeed);

    default:
        PG3_FATAL_ERROR("Unknown algorithm!!");
    }
}

//////////////////////////////////////////////////////////////////////////
// The main rendering function, renders what is in aConfig

float Render(
    const Config &aConfig,
    uint32_t     *oUsedIterations = NULL)
{
    SetProcessPriority();

    // Set number of used threads
    omp_set_num_threads(aConfig.mNumThreads);

    // Create 1 renderer per thread
    typedef AbstractRenderer* AbstractRendererPtr;
    AbstractRendererPtr *renderers;
    renderers = new AbstractRendererPtr[aConfig.mNumThreads];

    for (uint32_t i=0; i<aConfig.mNumThreads; i++)
        renderers[i] = CreateRenderer(aConfig, aConfig.mBaseSeed + i);

    const float startT = (float)clock();
    const float endT   = startT + aConfig.mMaxTime*CLOCKS_PER_SEC;
    int32_t iter = 0;

    // Rendering loop, when we have any time limit, use time-based loop,
    // otherwise go with required iterations
    if (aConfig.mMaxTime > 0)
    {
        // Time based loop
#pragma omp parallel
        while (clock() < endT)
        {
            int32_t threadId = omp_get_thread_num();
            renderers[threadId]->RunIteration(aConfig.mAlgorithm, iter);

            // Print progress bar
#pragma omp critical
            {
                if (!aConfig.mQuietMode)
                {
                    const float currentClock = (float)clock();
                    const float progress = (float)((currentClock - startT) / (endT - startT));
                    PrintProgressBar(20, progress);
                }
            }

#pragma omp atomic
            iter++; // counts number of iterations
        }
    }
    else
    {
        // Iterations based loop
        uint32_t globalCounter = 0;
#pragma omp parallel for
        for (iter=0; iter < aConfig.mIterations; iter++)
        {
            int32_t threadId = omp_get_thread_num();
            renderers[threadId]->RunIteration(aConfig.mAlgorithm, iter);

            // Print progress bar
#pragma omp critical
            {
                globalCounter++;
                if (!aConfig.mQuietMode)
                {
                    const float progress = (float)globalCounter / aConfig.mIterations;
                    PrintProgressBar(20, progress);
                }
            }
        }
    }

    clock_t realEndT = clock();

    if (oUsedIterations)
        *oUsedIterations = iter+1;

    // Accumulate from all renderers into a common framebuffer
    uint32_t usedRenderers = 0;

    // With very low number of iterations and high number of threads
    // not all created renderers had to have been used.
    // Those must not participate in accumulation.
    for (uint32_t i=0; i<aConfig.mNumThreads; i++)
    {
        if (!renderers[i]->WasUsed())
            continue;

        if (usedRenderers == 0)
        {
            renderers[i]->GetFramebuffer(*aConfig.mFramebuffer);
        }
        else
        {
            Framebuffer tmp;
            renderers[i]->GetFramebuffer(tmp);
            aConfig.mFramebuffer->Add(tmp);
        }

        usedRenderers++;
    }

    // Scale framebuffer by the number of used renderers
    aConfig.mFramebuffer->Scale(FramebufferFloat(1.) / usedRenderers);

    // Clean up renderers
    for (uint32_t i=0; i<aConfig.mNumThreads; i++)
        delete renderers[i];

    delete [] renderers;

    return float(realEndT - startT) / CLOCKS_PER_SEC;
}

//////////////////////////////////////////////////////////////////////////
// Main

int32_t main(int32_t argc, const char *argv[])
{
    // Warn when not using C++11 Mersenne Twister
    PrintRngWarning();

    // Setup config based on command line
    Config config;
    ParseCommandline(argc, argv, config);
    if (config.mScene == NULL)
    {
        // When some error has been encountered, exit
        //getchar(); // debug // Wait for pressing the enter key on the command line
        return 1;
    }

    // If number of threads is invalid, set 1 thread per processor
    if (config.mNumThreads <= 0)
        config.mNumThreads  = std::max(1, omp_get_num_procs());

    // Print what we are doing
    PrintConfiguration(config);

    // Set up framebuffer and number of threads
    Framebuffer fbuffer;
    config.mFramebuffer = &fbuffer;

    // Render the image
    float time = Render(config);
    std::string timeHumanReadable;
    SecondsToHumanReadable(time, timeHumanReadable);
    if (!config.mQuietMode)
        printf(" done in %s\n", timeHumanReadable.c_str());

    // Save the image
    std::string fullOutputPath;
    if (!config.mOutputDirectory.empty())
        fullOutputPath = config.mOutputDirectory + "\\" + config.mOutputName;
    else
        fullOutputPath = config.mOutputName;
    if (!config.mQuietMode)
        printf("Saving to: %s ... ", fullOutputPath.c_str());
    std::string extension = fullOutputPath.substr(fullOutputPath.length() - 3, 3);
    if (extension == "bmp")
    {
        fbuffer.SaveBMP(fullOutputPath.c_str(), 2.2f /*gamma*/);
        if (!config.mQuietMode)
            printf("done\n");
    }
    else if (extension == "hdr")
    {
        fbuffer.SaveHDR(fullOutputPath.c_str());
        if (!config.mQuietMode)
            printf("done\n");
    }
    else
        printf("Used unknown extension %s\n", extension.c_str());

    // Scene cleanup
    delete config.mScene;

    // debug
    //getchar(); // Wait for pressing the enter key on the command line
    return 0;
}
