#pragma once 

#include "unit_testing.hxx"
#include "math.hxx"
#include "types.hxx"
#include <sstream>
#include <iomanip>

namespace Filter
{
    // Triangle (tent) filter, width = 2
    template <
        typename TCoeff,
        typename TVal,
        typename = typename
            std::enable_if<
                   std::is_same<double, TCoeff>::value
                || std::is_same<float, TCoeff>::value
                >::type
        >
    static TVal Triangle(
        const TCoeff &cx, const TCoeff &cy,
        const TVal &x0y0, const TVal &x1y0,
        const TVal &x0y1, const TVal &x1y1)
    {
        return Math::Bilerp(
            cx, cy,
            x0y0, x1y0,
            x0y1, x1y1);
    }


    // Integral of the triangle-reconstructed function over the middle pixel (starting at 
    // coordinates x1y1). Assumes pixel surface area to equal 1.
    template <typename TVal>
    static TVal TriangleIntegral(
        const TVal &x0y0, const TVal &x1y0, const TVal &x2y0,
        const TVal &x0y1, const TVal &x1y1, const TVal &x2y1,
        const TVal &x0y2, const TVal &x1y2, const TVal &x2y2)
    {
        static const float kWeights[3][3]{
            { 1/24.f, 1/24.f, 1/24.f },
            { 1/24.f, 2/3.f,  1/24.f },
            { 1/24.f, 1/24.f, 1/24.f }
        };

        const float integral = 
              x0y0 * kWeights[0][0]
            + x0y1 * kWeights[0][1]
            + x0y2 * kWeights[0][2]
            + x1y0 * kWeights[1][0]
            + x1y1 * kWeights[1][1]
            + x1y2 * kWeights[1][2]
            + x2y0 * kWeights[2][0]
            + x2y1 * kWeights[2][1]
            + x2y2 * kWeights[2][2];

        return integral;
    }


#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

    static bool _UT_TriangleIntegral_Single(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const float                  (&aSampleValues)[3][3],
        const float                  aExpectedResult,
        const float                  aErrorThreshold)
    {
        // Name

        std::ostringstream testNameStream;

        testNameStream << "X0(";
        testNameStream << std::fixed << std::setw(4) << std::setprecision(1) << aSampleValues[0][0] << ",";
        testNameStream << std::fixed << std::setw(4) << std::setprecision(1) << aSampleValues[0][1] << ",";
        testNameStream << std::fixed << std::setw(4) << std::setprecision(1) << aSampleValues[0][2] << "), ";

        testNameStream << "X1(";
        testNameStream << std::fixed << std::setw(4) << std::setprecision(1) << aSampleValues[1][0] << ",";
        testNameStream << std::fixed << std::setw(4) << std::setprecision(1) << aSampleValues[1][1] << ",";
        testNameStream << std::fixed << std::setw(4) << std::setprecision(1) << aSampleValues[1][2] << "), ";

        testNameStream << "X2(";
        testNameStream << std::fixed << std::setw(4) << std::setprecision(1) << aSampleValues[2][0] << ",";
        testNameStream << std::fixed << std::setw(4) << std::setprecision(1) << aSampleValues[2][1] << ",";
        testNameStream << std::fixed << std::setw(4) << std::setprecision(1) << aSampleValues[2][2] << ")";

        const auto testNameStr = testNameStream.str();
        const auto testName = testNameStr.c_str();

        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", testName);

        // Test

        const auto result = TriangleIntegral(
            aSampleValues[0][0], aSampleValues[1][0], aSampleValues[2][0],
            aSampleValues[0][1], aSampleValues[1][1], aSampleValues[2][1],
            aSampleValues[0][2], aSampleValues[1][2], aSampleValues[2][2]);

        if (!Math::EqualDelta(result, aExpectedResult, aErrorThreshold))
        {
            std::ostringstream ossError;
            ossError << "Reconstructed function integral ";
            ossError << std::fixed << std::setprecision(4) << result;
            ossError << " doesn't match the reference ";
            ossError << std::fixed << std::setprecision(4) << aExpectedResult;
            ossError << "!";

            PG3_UT_FAILED(
                aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s",
                ossError.str().c_str(), testName);

            return false;
        }

        PG3_UT_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", testName);
        return true;
    }


    static bool _UT_TriangleIntegral(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest,
            "Filter::TriangleIntegral");

        if (!_UT_TriangleIntegral_Single(
                aMaxUtBlockPrintLevel, 
                {{ 1.f, 1.f, 1.f },  // X0
                 { 1.f, 1.f, 1.f },  // X1
                 { 1.f, 1.f, 1.f }}, // X2
                1.f, 0.0001f))
            return false;

        if (!_UT_TriangleIntegral_Single(
                aMaxUtBlockPrintLevel, 
                {{ -1.f, -1.f, -1.f },  // X0
                 { -1.f, -1.f, -1.f },  // X1
                 { -1.f, -1.f, -1.f }}, // X2
                -1.f, 0.0001f))
            return false;

        if (!_UT_TriangleIntegral_Single(
                aMaxUtBlockPrintLevel, 
                {{ 5.f, 5.f, 5.f },  // X0
                 { 5.f, 5.f, 5.f },  // X1
                 { 5.f, 5.f, 5.f }}, // X2
                5.f, 0.0001f))
            return false;

        if (!_UT_TriangleIntegral_Single(
                aMaxUtBlockPrintLevel, 
                {{ 0.f, 0.f, 0.f },  // X0
                 { 0.f, 1.f, 0.f },  // X1
                 { 0.f, 0.f, 0.f }}, // X2
                2.f / 3.f, 0.0001f))
            return false;

        if (!_UT_TriangleIntegral_Single(
                aMaxUtBlockPrintLevel, 
                {{ 1.f, 1.f, 1.f },  // X0
                 { 1.f, 0.f, 1.f },  // X1
                 { 1.f, 1.f, 1.f }}, // X2
                1.f / 3.f, 0.0001f))
            return false;

        PG3_UT_PASSED(
            aMaxUtBlockPrintLevel, eutblWholeTest,
            "Filter::TriangleIntegral");
        return true;
    }


    static bool _UnitTests(const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        if (!_UT_TriangleIntegral(aMaxUtBlockPrintLevel))
            return false;

        return true;
    }

#endif

}
