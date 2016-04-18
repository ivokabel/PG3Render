#pragma once

//#include "math.hxx"

#include <sstream>
#include <iomanip>


///////////////////////////////////////////////////////////////////////////////
// Various utility functions
///////////////////////////////////////////////////////////////////////////////
namespace Utils
{
    template <typename T>
    inline bool IsMasked(const T &aVal, const T &aMask)
    {
        return ((aVal & aMask) == aMask);
    }

    void SecondsToHumanReadable(const float aSeconds, std::string &oResult)
    {
        std::ostringstream outStream;

        uint32_t tmp = (uint32_t)std::roundf(aSeconds);
        if (tmp > 0)
        {
            uint32_t seconds = tmp % 60;
            tmp /= 60;
            uint32_t minutes = tmp % 60;
            tmp /= 60;
            uint32_t hours = tmp % 24;
            tmp /= 24;
            uint32_t days = tmp;

            if (days > 0)
                outStream << days << " d "; // " day" << ((days > 1) ? "s " : " ");
            if (hours > 0)
                outStream << hours << " h "; // " hour" << ((hours > 1) ? "s " : " ");
            if (minutes > 0)
                outStream << minutes << " m "; // " minute" << ((minutes > 1) ? "s " : " ");
            if (seconds > 0)
                outStream << seconds << " s "; // " second" << ((seconds > 1) ? "s " : " ");

            outStream << "(" << std::fixed << std::setprecision(1) << aSeconds << " s)";
        }
        else
            outStream << std::fixed << std::setprecision(2) << aSeconds << " s"; // " seconds";

        oResult = outStream.str();
    }

    bool GetFileName(const char *aPath, std::string &oResult)
    {
        //char path_buffer[_MAX_PATH];
        //char drive[_MAX_DRIVE];
        //char dir[_MAX_DIR];
        char fname[_MAX_FNAME];
        char ext[_MAX_EXT];
        errno_t err;

        err = _splitpath_s(aPath, NULL, 0, NULL, 0, fname, _MAX_FNAME, ext, _MAX_EXT);
        if (err != 0)
        {
            //printf("Error splitting the path. Error code %d.\n", err);
            return false;
        }
        //printf("Path extracted with _splitpath_s:\n");
        //printf("  Drive: %s\n", drive);
        //printf("  Dir: %s\n", dir);
        //printf("  Filename: %s\n", fname);
        //printf("  Ext: %s\n", ext);

        oResult  = fname;
        oResult += ext;
        return true;
    }

    // Returns the length of an array
    template <typename T, size_t N>
    inline int32_t ArrayLength(const T(&)[N])
    {
        return int32_t(N);
    }

    //////////////////////////////////////////////////////////////////////////
    // Fresnel-related routines
    //////////////////////////////////////////////////////////////////////////
    namespace Fresnel
    {
        float Dielectric(
            float aCosThetaI,
            float aEtaAbs)         // internal IOR / external IOR
        {
            PG3_ASSERT_FLOAT_LARGER_THAN(aEtaAbs, 0.0f);

            if (aEtaAbs < 0)
                return 1.f;

            float workingEta;
            if (aCosThetaI < 0.f)
            {
                aCosThetaI = -aCosThetaI;
                workingEta = aEtaAbs;
            }
            else
                workingEta = 1.f / aEtaAbs;

            const float sinThetaTSqr    = Math::Sqr(workingEta) * (1.f - Math::Sqr(aCosThetaI));
            const float cosThetaT       = Math::SafeSqrt(1.f - sinThetaTSqr);

            if (sinThetaTSqr > 1.0f)
                return 1.0f; // TIR

            // Perpendicular (senkrecht) polarization
            const float term2                   = workingEta * aCosThetaI;
            const float reflPerpendicularSqrt   = (term2 - cosThetaT) / (term2 + cosThetaT);
            const float reflPerpendicular       = Math::Sqr(reflPerpendicularSqrt);

            // Parallel polarization
            const float term1               = workingEta * cosThetaT;
            const float reflParallelSqrt    = (aCosThetaI - term1) / (aCosThetaI + term1);
            const float reflParallel        = Math::Sqr(reflParallelSqrt);

            const float reflectance = 0.5f * (reflParallel + reflPerpendicular);

            PG3_ASSERT_FLOAT_IN_RANGE(reflParallel, 0.0f, 1.0f);
            PG3_ASSERT_FLOAT_IN_RANGE(reflPerpendicular, 0.0f, 1.0f);
            PG3_ASSERT_FLOAT_IN_RANGE(reflectance, 0.0f, 1.0f);
            PG3_ASSERT_FLOAT_LARGER_THAN_OR_EQUAL_TO(reflPerpendicular + 0.001f, reflParallel);

            return reflectance;
        }

        float Conductor(
            float aCosThetaI,
            float aEtaAbs,         // internal IOR / external IOR
            float aAbsorbance
            )
        {
            PG3_ASSERT_FLOAT_LARGER_THAN(aEtaAbs, 0.0f);
            PG3_ASSERT_FLOAT_LARGER_THAN(aAbsorbance, 0.0f);

            if (aCosThetaI < -0.00001f)
                // Hitting the surface from the inside - no reflectance. This can be caused for example by 
                // a numerical error in object intersection code.
                return 0.0f;

            aCosThetaI = Math::Clamp(aCosThetaI, 0.0f, 1.0f);

#ifdef PG3_USE_ART_FRESNEL

            const float cosThetaSqr = Math::Sqr(aCosThetaI);
            const float sinThetaSqr = std::max(1.0f - cosThetaSqr, 0.0f);
            const float sinTheta    = std::sqrt(sinThetaSqr);

            const float iorSqr    = Math::Sqr(aEtaAbs);
            const float absorbSqr = Math::Sqr(aAbsorbance);

            const float tmp1 = iorSqr - absorbSqr - sinThetaSqr;
            const float tmp2 = std::sqrt(Math::Sqr(tmp1) + 4 * iorSqr * absorbSqr);

            const float aSqr     = (tmp2 + tmp1) * 0.5f;
            const float bSqr     = (tmp2 - tmp1) * 0.5f;
            const float aSqrMul2 = 2 * std::sqrt(aSqr);

            float tanTheta, tanThetaSqr;

            if (!Math::IsTiny(aCosThetaI))
            {
                tanTheta    = sinTheta / aCosThetaI;
                tanThetaSqr = Math::Sqr(tanTheta);
            }
            else
            {
                tanTheta    = Math::kHugeF;
                tanThetaSqr = Math::kHugeF;
            }

            const float reflPerpendicular =
                  (aSqr + bSqr - aSqrMul2 * aCosThetaI + cosThetaSqr)
                / (aSqr + bSqr + aSqrMul2 * aCosThetaI + cosThetaSqr);

            const float reflParallel =
                  reflPerpendicular
                * (  (aSqr + bSqr - aSqrMul2 * sinTheta * tanTheta + sinThetaSqr * tanThetaSqr)
                   / (aSqr + bSqr + aSqrMul2 * sinTheta * tanTheta + sinThetaSqr * tanThetaSqr));

            const float reflectance = 0.5f * (reflParallel + reflPerpendicular);

            PG3_ASSERT_FLOAT_IN_RANGE(reflPerpendicular, 0.0f, 1.0f);
            PG3_ASSERT_FLOAT_IN_RANGE(reflParallel,      0.0f, 1.0f);
            PG3_ASSERT_FLOAT_IN_RANGE(reflectance,       0.0f, 1.0f);
            PG3_ASSERT_FLOAT_LARGER_THAN_OR_EQUAL_TO(reflPerpendicular, reflParallel);

#elif defined PG3_USE_MITSUBA_FRESNEL

            // Taken and modified from Mitsuba renderer, which states:
            //      "Modified from "Optics" by K.D. Moeller, University Science Books, 1988"

            const float cosThetaI2 = aCosThetaI * aCosThetaI;
            const float sinThetaI2 = 1 - cosThetaI2;
            const float sinThetaI4 = sinThetaI2 * sinThetaI2;

            const float aEta2        = aEtaAbs * aEtaAbs;
            const float aAbsorbance2 = aAbsorbance * aAbsorbance;

            const float temp1 = aEta2 - aAbsorbance2 - sinThetaI2;
            const float a2pb2 = Math::SafeSqrt(temp1 * temp1 + 4 * aAbsorbance2 * aEta2);
            const float a = Math::SafeSqrt(0.5f * (a2pb2 + temp1));

            const float term1 = a2pb2 + cosThetaI2;
            const float term2 = 2 * a * aCosThetaI;

            const float Rs2 = (term1 - term2) / (term1 + term2);

            const float term3 = a2pb2 * cosThetaI2 + sinThetaI4;
            const float term4 = term2 * sinThetaI2;

            const float Rp2 = Rs2 * (term3 - term4) / (term3 + term4);

            const float reflectance = 0.5f * (Rp2 + Rs2); // non-polarising reflectance

            PG3_ASSERT_FLOAT_IN_RANGE(Rs2,          0.0f, 1.0f);
            PG3_ASSERT_FLOAT_IN_RANGE(Rp2,          0.0f, 1.0f);
            PG3_ASSERT_FLOAT_IN_RANGE(reflectance,  0.0f, 1.0f);
            PG3_ASSERT_FLOAT_LARGER_THAN_OR_EQUAL_TO(Rs2 + 0.00001f, Rp2);

#else
#error Unspecified Fresnel version!
#endif

            return reflectance;
        }
    } // namespace Fresnel

    namespace ProgressBar
    {
        void PrintCommon(float aProgress)
        {
            PG3_ASSERT_FLOAT_LARGER_THAN_OR_EQUAL_TO(aProgress, 0.0f);

            const uint32_t aBarCount = 30;

            aProgress = Math::Clamp(aProgress, 0.f, 1.f);

            printf("\rProgress:  [");

            for (uint32_t bar = 1; bar <= aBarCount; bar++)
            {
                const float barProgress = (float)bar / aBarCount;
                if (barProgress <= aProgress)
                    printf("|");
                else
                    printf(".");
            }

            printf("] %.1f%%", std::floor(100.0 * aProgress * 10.0f) / 10.f);
        }

        void PrintIterations(float aProgress, uint32_t aIterations)
        {
            PrintCommon(aProgress);

            printf(" (%d iter%s)", aIterations, (aIterations != 1) ? "s" : "");

            fflush(stdout);
        }

        void PrintTime(float aProgress, float aTime)
        {
            PrintCommon(aProgress);

            printf(" (%.1f sec)", aTime);

            fflush(stdout);
        }
    } // namespace ProgressBar
} // namespace Utils
