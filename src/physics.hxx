#pragma once

namespace Physics
{
    float FresnelDielectric(
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


    float FresnelConductor(
        float aCosThetaI,
        float aEtaAbs,         // internal IOR / external IOR
        float aAbsorbance)
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


    // Bouguer-Lambert-Beer law of attenuation
    SpectrumF BeerLambert(
        const SpectrumF &aAttenuationCoeff,
        const float      aPathLength)
    {
        PG3_ASSERT_VEC3F_VALID(aAttenuationCoeff);
        PG3_ASSERT_FLOAT_VALID(aPathLength);

        const SpectrumF opticalDepth    = aAttenuationCoeff * aPathLength;
        const SpectrumF transmissivity  = Exp(-opticalDepth);

        PG3_ASSERT_FLOAT_VALID(transmissivity);

        return transmissivity;
    }
}