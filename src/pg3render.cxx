#include <vector>
#include <cmath>
#include <time.h>
#include <cstdlib>
#include <algorithm>
#include "math.hxx"
#include "ray.hxx"
#include "geometry.hxx"
#include "camera.hxx"
#include "framebuffer.hxx"
#include "scene.hxx"
#include "eyelight.hxx"
#include "pathtracer.hxx"
#include "config.hxx"

#include <omp.h>
#include <string>
#include <set>
#include <sstream>

//////////////////////////////////////////////////////////////////////////
// The main rendering function, renders what is in aConfig

float render(
    const Config &aConfig,
    int *oUsedIterations = NULL)
{
    // Set number of used threads
    omp_set_num_threads(aConfig.mNumThreads);

    // Create 1 renderer per thread
    typedef AbstractRenderer* AbstractRendererPtr;
    AbstractRendererPtr *renderers;
    renderers = new AbstractRendererPtr[aConfig.mNumThreads];

    for (int i=0; i<aConfig.mNumThreads; i++)
    {
        renderers[i] = CreateRenderer(aConfig, aConfig.mBaseSeed + i);

        renderers[i]->mMaxPathLength = aConfig.mMaxPathLength;
        renderers[i]->mMinPathLength = aConfig.mMinPathLength;
    }

    clock_t startT = clock();
    int iter = 0;

    // Rendering loop, when we have any time limit, use time-based loop,
    // otherwise go with required iterations
    if (aConfig.mMaxTime > 0)
    {
        // Time based loop
#pragma omp parallel
        while (clock() < startT + aConfig.mMaxTime*CLOCKS_PER_SEC)
        {
            int threadId = omp_get_thread_num();
            renderers[threadId]->RunIteration(iter);

#pragma omp atomic
            iter++; // counts number of iterations
        }
    }
    else
    {
        // Iterations based loop
        int globalCounter = 0;
#pragma omp parallel for
        for (iter=0; iter < aConfig.mIterations; iter++)
        {
            int threadId = omp_get_thread_num();
            renderers[threadId]->RunIteration(iter);

            // Print progress bar
#pragma omp critical
            {
                globalCounter++;
                const double progress   = (double)globalCounter / aConfig.mIterations;
                const int barCount      = 20;

                printf(
                    "\rProgress:  %6.2f%% [", 
                    100.0 * progress);
                for (int bar = 1; bar <= barCount; bar++)
                {
                    const double barProgress = (double)bar / barCount;
                    if (barProgress <= progress)
                        printf("|");
                    else
                        printf(".");
                }
                printf("]");
                fflush(stdout);
            }
        }
    }

    clock_t endT = clock();

    if (oUsedIterations)
        *oUsedIterations = iter+1;

    // Accumulate from all renderers into a common framebuffer
    int usedRenderers = 0;

    // With very low number of iterations and high number of threads
    // not all created renderers had to have been used.
    // Those must not participate in accumulation.
    for (int i=0; i<aConfig.mNumThreads; i++)
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
    aConfig.mFramebuffer->Scale(1.f / usedRenderers);

    // Clean up renderers
    for (int i=0; i<aConfig.mNumThreads; i++)
        delete renderers[i];

    delete [] renderers;

    return float(endT - startT) / CLOCKS_PER_SEC;
}

//////////////////////////////////////////////////////////////////////////
// Main

int main(int argc, const char *argv[])
{
    // Warns when not using C++11 Mersenne Twister
    PrintRngWarning();

    // Setups config based on command line
    Config config;
    ParseCommandline(argc, argv, config);

    // If number of threads is invalid, set 1 thread per processor
    if (config.mNumThreads <= 0)
        config.mNumThreads  = std::max(1, omp_get_num_procs());

    // When some error has been encountered, exit
    if (config.mScene == NULL)
        return 1;

    // Sets up framebuffer and number of threads
    Framebuffer fbuffer;
    config.mFramebuffer = &fbuffer;

    // Prints what we are doing
    printf("Scene:     %s\n", config.mScene->mSceneName.c_str());
    if (config.mMaxTime > 0)
        printf("Target:    %g seconds render time\n", config.mMaxTime);
    else
        printf("Target:    %d iteration(s)\n", config.mIterations);

    // Renders the image
    printf("Running:   %s%s", config.GetName(config.mAlgorithm), (config.mMaxTime > 0) ? "..." : "\n");
    fflush(stdout);
    float time = render(config);
    printf(" done in %.2f s\n", time);

    // Saves the image
    printf("Saving to: %s ... ", config.mOutputName.c_str());
    std::string extension = config.mOutputName.substr(config.mOutputName.length() - 3, 3);
    if (extension == "bmp")
    {
        fbuffer.SaveBMP(config.mOutputName.c_str(), 2.2f /*gamma*/);
        printf("done\n");
    }
    else if (extension == "hdr")
    {
        fbuffer.SaveHDR(config.mOutputName.c_str());
        printf("done\n");
    }
    else
        printf("Used unknown extension %s\n", extension.c_str());

    // Scene cleanup
    delete config.mScene;

    // debug
    getchar(); // Wait for pressing the enter key on the command line

    return 0;
}
