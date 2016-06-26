#include "math.hxx"
#include "ray.hxx"
#include "geometry.hxx"
#include "camera.hxx"
#include "framebuffer.hxx"
#include "scene.hxx"
#include "eyelight.hxx"
#include "directillumination.hxx"
#include "pathtracer.hxx"
#include "microfacet.hxx"
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
    case kDirectIllumBsdfSampling:
    case kDirectIllumMis:
        return new DirectIllumination(aConfig, aSeed);

    case kPathTracingNaive:
    case kPathTracing:
        return new PathTracer(aConfig, aSeed);

    default:
        PG3_FATAL_ERROR("Unknown algorithm!!");
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
// The main rendering function, renders what is in aConfig
float Render(
    const Config                        &aConfig,
    RendererIntrospectionDataAggregator &aIntrospectionAggregator,
    uint32_t                            *oUsedIterations = nullptr)
{
    Process::SetPriority();

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
        if (!aConfig.mQuietMode)
            Utils::ProgressBar::PrintTime(0.f, 0.f);

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
                    Utils::ProgressBar::PrintTime(progress, (currentClock - startT) / CLOCKS_PER_SEC);
                }
            }

#pragma omp atomic
            iter++; // counts number of iterations
        }
    }
    else
    {
        if (!aConfig.mQuietMode)
            Utils::ProgressBar::PrintIterations(0.f, 0);

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
                    Utils::ProgressBar::PrintIterations(progress, globalCounter);
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

    // Aggregate introspection data (e.g. path statistics) from all renderers
    for (uint32_t i = 0; i<aConfig.mNumThreads; i++)
        aIntrospectionAggregator.AddRendererData(renderers[i]->GetRendererIntrospectionData());

    // Release renderers
    for (uint32_t i=0; i<aConfig.mNumThreads; i++)
        delete renderers[i];
    delete [] renderers;

    return float(realEndT - startT) / CLOCKS_PER_SEC;
}

//////////////////////////////////////////////////////////////////////////
// Unit testing
#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER
void RunUnitTests(UnitTestBlockLevel aMaxUtBlockPrintLevel)
{
    //Geom::_UnitTest_UnitIcosahedron(aMaxUtBlockPrintLevel);

    //Microfacet::_UnitTest_HalfwayVectorRefractionLocal(aMaxUtBlockPrintLevel);

    //EnvironmentMapSteeringSampler::_UnitTest_TriangulateEm(aMaxUtBlockPrintLevel);
    //EnvironmentMapSteeringSampler::_UnitTest_SteeringValues(aMaxUtBlockPrintLevel);
    EnvironmentMapSteeringSampler::SteeringBasisValue::_UnitTest_GenerateSphHarm(aMaxUtBlockPrintLevel);
}
#endif

//////////////////////////////////////////////////////////////////////////
// Main
int32_t main(int32_t argc, const char *argv[])
{
    Debugging::Init();

#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

    argc; argv; // unused params

    //for (uint32_t i = 0; i < 1000; i++)
        //RunUnitTests(eutblWholeTest);
        //RunUnitTests(eutblSubTestLevel1);
        RunUnitTests(eutblSubTestLevel2);

    exit(0);

#else

    Config::PrintRngWarning(); // Warn when not using C++11 Mersenne Twister

    // Setup config based on command line
    Config config;
    if (!config.ParseCommandline(argc, argv))
    {
        // When some error has been encountered, exit
        //getchar(); // debug
        return 1;
    }

    std::string fullOutputPath;
    if (!config.mOutputDirectory.empty())
        fullOutputPath = config.mOutputDirectory + "\\" + config.mOutputName;
    else
        fullOutputPath = config.mOutputName;

    if (config.mOnlyPrintOutputPath)
    {
        printf("%s", fullOutputPath.c_str());
        return 1;
    }

    if (config.mNumThreads <= 0)
        config.mNumThreads  = std::max(1, omp_get_num_procs());

    config.PrintConfiguration();

    Framebuffer fbuffer;
    config.mFramebuffer = &fbuffer;

    RendererIntrospectionDataAggregator introspectionAggregator;

    // Render the image
    float time = Render(config, introspectionAggregator);
    std::string timeHumanReadable;
    Utils::SecondsToHumanReadable(time, timeHumanReadable);
    if (!config.mQuietMode)
        printf(" done in %s\n", timeHumanReadable.c_str());

    // Save the image
    std::string extension = fullOutputPath.substr(fullOutputPath.length() - 3, 3);
    if (extension == "bmp")
        fbuffer.SaveBMP(fullOutputPath.c_str(), 2.2f /*gamma*/);
    else if (extension == "hdr")
        fbuffer.SaveHDR(fullOutputPath.c_str());
    else
        printf("Saving:    Used unknown extension %s!\n", extension.c_str());

    introspectionAggregator.PrintIntrospection();

    delete config.mScene;

    // debug
    //getchar(); // Wait for pressing the enter key on the command line
    return 0;

#endif
}
