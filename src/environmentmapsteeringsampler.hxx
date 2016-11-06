#pragma once

#include "environmentmapimage.hxx"
#include "unittesting.hxx"
#include "debugging.hxx"
#include "spectrum.hxx"
#include "types.hxx"

#include <list>
#include <stack>
#include <array>
#include <memory>

// Environment map sampler based on the paper "Steerable Importance Sampling"
// from Kartic Subr and Jim Arvo, 2007
class EnvironmentMapSteeringSampler
{
public:

     EnvironmentMapSteeringSampler() {}
    ~EnvironmentMapSteeringSampler() {}

public:

    class SteeringValue
    {
    public:

        SteeringValue() {}

        SteeringValue(const std::array<float, 9> &aBasisValues) :
            mBasisValues(aBasisValues)
        {}

        float operator* (const SteeringValue &aValue)
        {
            PG3_ASSERT_INTEGER_EQUAL(mBasisValues.size(), aValue.mBasisValues.size());

            float retval = 0.0f;
            for (size_t i = 0; i < mBasisValues.size(); i++)
                retval += mBasisValues[i] * aValue.mBasisValues[i];
            return retval;
        }

        bool operator == (const SteeringValue &aBasis) const
        {
            return mBasisValues == aBasis.mBasisValues;
        }

        bool operator != (const SteeringValue &aBasis) const
        {
            return !(*this == aBasis);
        }

        bool EqualsDelta(const SteeringValue &aBasis, float aDelta) const
        {
            PG3_ASSERT_FLOAT_NONNEGATIVE(aDelta);

            for (size_t i = 0; i < mBasisValues.size(); i++)
                if (!Math::EqualDelta(mBasisValues[i], aBasis.mBasisValues[i], aDelta))
                    return false;
            return true;
        }

    protected:
        std::array<float, 9> mBasisValues;
    };


    class SteeringBasisValue : public SteeringValue
    {
    public:

        SteeringBasisValue() {}

        SteeringBasisValue(const std::array<float, 9> &aBasisValues) :
            SteeringValue(aBasisValues)
        {}

        // Sets the value of spherical harmonic base at given direction multiplied by the factor
        SteeringBasisValue& GenerateSphHarm(const Vec3f &aDir, float aMulFactor)
        {
            PG3_ASSERT_VEC3F_NORMALIZED(aDir);
            PG3_ASSERT_FLOAT_NONNEGATIVE(aMulFactor);

            // Taken from 
            // 2001 Ramamoorthi & Hanrahan - An Efficient Representation for Irradiance Environment Maps

            mBasisValues[0] = aMulFactor * 0.282095f;    // Y_{00}

            mBasisValues[1] = aMulFactor * 0.488603f * aDir.y;   // Y_{1-1}
            mBasisValues[2] = aMulFactor * 0.488603f * aDir.z;   // Y_{10}
            mBasisValues[3] = aMulFactor * 0.488603f * aDir.x;   // Y_{11}

            mBasisValues[4] = aMulFactor * 1.092548f * aDir.x * aDir.y;                      // Y_{2-2}
            mBasisValues[5] = aMulFactor * 1.092548f * aDir.y * aDir.z;                      // Y_{2-1}
            mBasisValues[6] = aMulFactor * 0.315392f * (3.f * aDir.z * aDir.z - 1.f);        // Y_{20}
            mBasisValues[7] = aMulFactor * 1.092548f * aDir.x * aDir.z;                      // Y_{21}
            mBasisValues[8] = aMulFactor * 0.546274f * (aDir.x * aDir.x - aDir.y * aDir.y);  // Y_{22}

            return *this;
        }

        SteeringBasisValue operator* (const SteeringBasisValue &aValue) const
        {
            SteeringBasisValue retVal;
            for (size_t i = 0; i < mBasisValues.size(); i++)
                retVal.mBasisValues[i] = mBasisValues[i] * aValue.mBasisValues[i];
            return retVal;
        }

        SteeringBasisValue operator+ (const SteeringBasisValue &aValue) const
        {
            SteeringBasisValue retVal;
            for (size_t i = 0; i < mBasisValues.size(); i++)
                retVal.mBasisValues[i] = mBasisValues[i] + aValue.mBasisValues[i];
            return retVal;
        }

        SteeringBasisValue operator/ (float aValue) const
        {
            SteeringBasisValue retVal;
            for (size_t i = 0; i < mBasisValues.size(); i++)
                retVal.mBasisValues[i] = mBasisValues[i] / aValue;
            return retVal;
        }

#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

    public:

        static bool _UnitTest_GenerateSphHarm_SingleDirection(
            const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
            const Vec3f                 &aDirection,
            //const float                  aTheta,
            //const float                  aPhi,
            const SteeringBasisValue    &aNormalizedReferenceBasisValue,
            const char                  *aTestName)
        {
            PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", aTestName);

            const SteeringBasisValue normalizationValues({
                0.282095f,          // Y_{0 0}
                0.488603f,          // Y_{1-1}
                0.488603f,          // Y_{1 0}
                0.488603f,          // Y_{1 1}
                1.092548f * 0.5f,   // Y_{2-2}
                1.092548f * 0.5f,   // Y_{2-1}
                0.315392f * 2.f,    // Y_{2 0}
                1.092548f * 0.5f,   // Y_{2 1}
                0.546274f           // Y_{2 2}
            });
            
            SteeringBasisValue referenceVal = aNormalizedReferenceBasisValue * normalizationValues;

            //Vec3f direction = Geom::CreateDirection(aTheta, aPhi);

            SteeringBasisValue generatedValue;
            generatedValue.GenerateSphHarm(aDirection, 1.0f);

            if (!generatedValue.EqualsDelta(referenceVal, 0.0001f))
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s",
                                  "The generated value doesn't match the reference value",
                                  aTestName);
                return false;
            }

            PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", aTestName);
            return true;
        }

        static bool _UnitTest_GenerateSphHarm_CanonicalDirections(
            const UnitTestBlockLevel aMaxUtBlockPrintLevel)
        {
            const char *testName = NULL;
            Vec3f direction;
            SteeringBasisValue referenceVal;

            // Positive X direction
            testName = "Positive X";
            direction = Geom::CreateDirection(0.5f * Math::kPiF, 0.f);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                0.f /*Y_{1-1}*/, 0.f /*Y_{1 0}*/, 1.f /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, -0.5f /*Y_{2 0}*/, 0.f /*Y_{2 1}*/, 1.f /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Negative X direction
            testName = "Negative X";
            direction = Geom::CreateDirection(1.5f * Math::kPiF, 0.f);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                0.f /*Y_{1-1}*/, 0.f /*Y_{1 0}*/, -1.f /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, -0.5f /*Y_{2 0}*/, 0.f /*Y_{2 1}*/, 1.f /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Positive Y direction
            testName = "Positive Y";
            direction = Geom::CreateDirection(0.5f * Math::kPiF, 0.5f * Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                1.f /*Y_{1-1}*/, 0.f /*Y_{1 0}*/, 0.f /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, -0.5f /*Y_{2 0}*/, 0.f /*Y_{2 1}*/, -1.f /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Negative Y direction
            testName = "Negative Y";
            direction = Geom::CreateDirection(0.5f * Math::kPiF, 1.5f * Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                -1.f /*Y_{1-1}*/, 0.f /*Y_{1 0}*/, 0.f /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, -0.5f /*Y_{2 0}*/, 0.f /*Y_{2 1}*/, -1.f /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Positive Z direction
            testName = "Positive Z";
            direction = Geom::CreateDirection(0.f, 0.f);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                0.f /*Y_{1-1}*/, 1.f /*Y_{1 0}*/, 0.f /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, 1.f /*Y_{2 0}*/, 0.f /*Y_{2 1}*/, 0.f /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Negative Z direction
            testName = "Negative Z";
            direction = Geom::CreateDirection(Math::kPiF, 0.f);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                0.f /*Y_{1-1}*/, -1.f /*Y_{1 0}*/, 0.f /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, 1.f /*Y_{2 0}*/, 0.f /*Y_{2 1}*/, 0.f /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            return true;
        }

        static bool _UnitTest_GenerateSphHarm_XYDiagonalDirections(
            const UnitTestBlockLevel aMaxUtBlockPrintLevel)
        {
            const char *testName = NULL;
            Vec3f direction;
            SteeringBasisValue referenceVal;

            // Positive X+Y direction
            testName = "Positive X+Y";
            direction = Geom::CreateDirection(0.5f * Math::kPiF, 0.25f * Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                Math::kCosPiDiv4F /*Y_{1-1}*/, 0.f /*Y_{1 0}*/, Math::kCosPiDiv4F /*Y_{1 1}*/,
                1.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, -0.5f /*Y_{2 0}*/, 0.f /*Y_{2 1}*/, 0.f /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Negative X+Y direction
            testName = "Negative X+Y";
            direction = Geom::CreateDirection(0.5f * Math::kPiF, 1.25f * Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                -Math::kCosPiDiv4F /*Y_{1-1}*/, 0.f /*Y_{1 0}*/, -Math::kCosPiDiv4F /*Y_{1 1}*/,
                1.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, -0.5f /*Y_{2 0}*/, 0.f /*Y_{2 1}*/, 0.f /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Positive X-Y direction
            testName = "Positive X-Y";
            direction = Geom::CreateDirection(0.5f * Math::kPiF, 0.75f * Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                Math::kCosPiDiv4F /*Y_{1-1}*/, 0.f /*Y_{1 0}*/, -Math::kCosPiDiv4F /*Y_{1 1}*/,
                -1.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, -0.5f /*Y_{2 0}*/, 0.f /*Y_{2 1}*/, 0.f /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Negative X-Y direction
            testName = "Negative X-Y";
            direction = Geom::CreateDirection(0.5f * Math::kPiF, 1.75f * Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                -Math::kCosPiDiv4F /*Y_{1-1}*/, 0.f /*Y_{1 0}*/, Math::kCosPiDiv4F /*Y_{1 1}*/,
                -1.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, -0.5f /*Y_{2 0}*/, 0.f /*Y_{2 1}*/, 0.f /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            return true;
        }

        static bool _UnitTest_GenerateSphHarm_YZDiagonalDirections(
            const UnitTestBlockLevel aMaxUtBlockPrintLevel)
        {
            const char *testName = NULL;
            Vec3f direction;
            SteeringBasisValue referenceVal;

            // Positive Y+Z direction
            testName = "Positive Y+Z";
            direction = Geom::CreateDirection(0.25f * Math::kPiF, 0.5f * Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                Math::kCosPiDiv4F /*Y_{1-1}*/, Math::kCosPiDiv4F /*Y_{1 0}*/, 0.f /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 1.f /*Y_{2-1}*/, 0.5f * (3.f * Math::Sqr(Math::kCosPiDiv4F) - 1.0f) /*Y_{2 0}*/,
                0.f /*Y_{2 1}*/, -Math::Sqr(Math::kCosPiDiv4F) /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Negative Y+Z direction
            testName = "Negative Y+Z";
            direction = Geom::CreateDirection(0.75f * Math::kPiF, 1.5f * Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                -Math::kCosPiDiv4F /*Y_{1-1}*/, -Math::kCosPiDiv4F /*Y_{1 0}*/, 0.f /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 1.f /*Y_{2-1}*/, 0.5f * (3.f * Math::Sqr(Math::kCosPiDiv4F) - 1.0f) /*Y_{2 0}*/,
                0.f /*Y_{2 1}*/, -Math::Sqr(Math::kCosPiDiv4F) /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Positive Y-Z direction
            testName = "Positive Y-Z";
            direction = Geom::CreateDirection(0.25f * Math::kPiF, 1.5f * Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                -Math::kCosPiDiv4F /*Y_{1-1}*/, Math::kCosPiDiv4F /*Y_{1 0}*/, 0.f /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, -1.f /*Y_{2-1}*/, 0.5f * (3.f * Math::Sqr(Math::kCosPiDiv4F) - 1.0f) /*Y_{2 0}*/,
                0.f /*Y_{2 1}*/, -Math::Sqr(Math::kCosPiDiv4F) /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Negative Y-Z direction
            testName = "Negative Y-Z";
            direction = Geom::CreateDirection(0.75f * Math::kPiF, 0.5f * Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                Math::kCosPiDiv4F /*Y_{1-1}*/, -Math::kCosPiDiv4F /*Y_{1 0}*/, 0.f /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, -1.f /*Y_{2-1}*/, 0.5f * (3.f * Math::Sqr(Math::kCosPiDiv4F) - 1.0f) /*Y_{2 0}*/,
                0.f /*Y_{2 1}*/, -Math::Sqr(Math::kCosPiDiv4F) /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            return true;
        }

        static bool _UnitTest_GenerateSphHarm_XZDiagonalDirections(
            const UnitTestBlockLevel aMaxUtBlockPrintLevel)
        {
            const char *testName = NULL;
            Vec3f direction;
            SteeringBasisValue referenceVal;

            // Positive X+Z direction
            testName = "Positive X+Z";
            direction = Geom::CreateDirection(0.25f * Math::kPiF, 0.f);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                0.f /*Y_{1-1}*/, Math::kCosPiDiv4F /*Y_{1 0}*/, Math::kCosPiDiv4F /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, 0.5f * (3.f * Math::Sqr(Math::kCosPiDiv4F) - 1.0f) /*Y_{2 0}*/,
                1.f /*Y_{2 1}*/, Math::Sqr(Math::kCosPiDiv4F) /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Negative X+Z direction
            testName = "Negative X+Z";
            direction = Geom::CreateDirection(0.75f * Math::kPiF, Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                0.f /*Y_{1-1}*/, -Math::kCosPiDiv4F /*Y_{1 0}*/, -Math::kCosPiDiv4F /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, 0.5f * (3.f * Math::Sqr(Math::kCosPiDiv4F) - 1.0f) /*Y_{2 0}*/,
                1.f /*Y_{2 1}*/, Math::Sqr(Math::kCosPiDiv4F) /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Positive X-Z direction
            testName = "Positive X-Z";
            direction = Geom::CreateDirection(0.25f * Math::kPiF, Math::kPiF);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                0.f /*Y_{1-1}*/, Math::kCosPiDiv4F /*Y_{1 0}*/, -Math::kCosPiDiv4F /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, 0.5f * (3.f * Math::Sqr(Math::kCosPiDiv4F) - 1.0f) /*Y_{2 0}*/,
                -1.f /*Y_{2 1}*/, Math::Sqr(Math::kCosPiDiv4F) /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            // Negative X-Z direction
            testName = "Negative X-Z";
            direction = Geom::CreateDirection(0.75f * Math::kPiF, 0.f);
            referenceVal = SteeringBasisValue({
                1.f /*Y_{0 0}*/,
                0.f /*Y_{1-1}*/, -Math::kCosPiDiv4F /*Y_{1 0}*/, Math::kCosPiDiv4F /*Y_{1 1}*/,
                0.f /*Y_{2-2}*/, 0.f /*Y_{2-1}*/, 0.5f * (3.f * Math::Sqr(Math::kCosPiDiv4F) - 1.0f) /*Y_{2 0}*/,
                -1.f /*Y_{2 1}*/, Math::Sqr(Math::kCosPiDiv4F) /*Y_{2 2}*/
            });
            if (!_UnitTest_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            return true;
        }

        static bool _UnitTest_GenerateSphHarm(
            const UnitTestBlockLevel aMaxUtBlockPrintLevel)
        {
            PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest,
                "SteeringBasisValue::GenerateSphHarm()");

            if (!_UnitTest_GenerateSphHarm_CanonicalDirections(aMaxUtBlockPrintLevel))
                return false;

            if (!_UnitTest_GenerateSphHarm_XYDiagonalDirections(aMaxUtBlockPrintLevel))
                return false;

            if (!_UnitTest_GenerateSphHarm_YZDiagonalDirections(aMaxUtBlockPrintLevel))
                return false;

            if (!_UnitTest_GenerateSphHarm_XZDiagonalDirections(aMaxUtBlockPrintLevel))
                return false;

            PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblWholeTest,
                "SteeringBasisValue::GenerateSphHarm()");

            return true;
        }

#endif
    };


    class SteeringCoefficients : public SteeringValue
    {
    public:
        // Generate clamped cosine spherical harmonic coefficients for the given normal
        SteeringCoefficients& GenerateForClampedCosSh(const Vec3f &aNormal)
        {
            aNormal;

            // TODO: Asserts:
            //  - normal: normalized

            // TODO: Get from paper...

            return *this;
        }
    };


#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

public:

    static bool _UnitTest_SteeringValues(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest,
            "Steering value structures");

        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "SteeringValue");

        // Equality operator

        if (SteeringValue({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }) !=
            SteeringValue({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "SteeringValue",
                "SteeringValue({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }) doesn't match itself!");
            return false;
        }

        if (SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f }) !=
            SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f }))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "SteeringValue",
                "SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f }) doesn't match itself!");
            return false;
        }

        if (SteeringValue({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }) ==
            SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f }))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "SteeringValue",
                "SteeringValue({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }) and "
                "SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f }) match!");
            return false;
        }

        // Delta equality operator

        if (!SteeringValue({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }).EqualsDelta(
             SteeringValue({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }), 0.001f))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "SteeringValue",
                "SteeringValue({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }) doesn't delta-match itself!");
            return false;
        }

        if (!SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f }).EqualsDelta(
             SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f }), 0.001f))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "SteeringValue",
                "SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f }) doesn't delta-match itself!");
            return false;
        }

        if (SteeringValue({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }).EqualsDelta(
            SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f }), 0.001f))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "SteeringValue",
                "SteeringValue({ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f }) and "
                "SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f }) delta-match!");
            return false;
        }

        if (!SteeringValue({ 0.f,    0.f,     0.f, 0.f, 0.f, 0.f, 0.f, 0.f,     0.f    }).EqualsDelta(
             SteeringValue({ 0.001f, 0.0001f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.0001f, 0.001f }), 0.001f))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "SteeringValue",
                "SteeringValue({ 0.f,    0.f,     0.f, 0.f, 0.f, 0.f, 0.f, 0.f,     0.f    }) and "
                "SteeringValue({ 0.001f, 0.0001f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.0001f, 0.001f }) don't delta-match!");
            return false;
        }

        if (SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.01f, 5.f, 6.f, 7.f, 8.f }).EqualsDelta(
            SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f,   5.f, 6.f, 7.f, 8.f }), 0.001f))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "SteeringValue",
                "SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.01f, 5.f, 6.f, 7.f, 8.f }) and "
                "SteeringValue({ 0.f, 1.f, 2.f, 3.f, 4.f,   5.f, 6.f, 7.f, 8.f }) delta-match!");
            return false;
        }

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "SteeringValue");

        // SteeringBasisValue
        // - initialization and operator==
        //   - different values
        //   - same values?

        // SteeringCoefficients
        // - initialization and operator==
        //   - different values
        //   - same values?


        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblWholeTest,
            "Steering value structures");

        return true;
    }

#endif

public:

    class Vertex
    {
    public:
        Vertex(const Vec3f &aDirection, const SteeringBasisValue &aWeight) : 
            dir(aDirection),
            weight(aWeight)
        {};

        bool operator == (const Vertex &aVertex) const
        {
            return (dir == aVertex.dir)
                && (weight == aVertex.weight);
        }

        Vec3f               dir; // TODO: Use (2D) spherical coordinates to save memory?
        SteeringBasisValue  weight;
    };


    class TreeNode
    {
    public:
        TreeNode(bool aIsTriangleNode) : mIsTriangleNode(aIsTriangleNode) {}

        bool IsTriangleNode() { return mIsTriangleNode; }

    protected:
        bool mIsTriangleNode; // node can be either triangle (leaf) or inner node
    };


    class InnerNode : public TreeNode
    {
    public:
        InnerNode() : TreeNode(false) {}

        ~InnerNode() {}

        // The node becomes the owner of the children and is responsible for releasing them
        void SetLeftChild( TreeNode* aLeftChild)  { mLeftChild.reset(aLeftChild); }
        void SetRightChild(TreeNode* aRightChild) { mLeftChild.reset(aRightChild); }

        const TreeNode* GetLeftChild()  const { return mLeftChild.get(); }
        const TreeNode* GetRightChild() const { return mRightChild.get(); }

    protected:
        // Children - owned by the node
        std::unique_ptr<TreeNode> mLeftChild;
        std::unique_ptr<TreeNode> mRightChild;
    };


    class TriangleNode : public TreeNode
    {
    public:

#ifdef _DEBUG
        // This class is not copyable because of a const member.
        // If we don't delete the assignment operator and copy constructor 
        // explicitly, the compiler may complain about not being able 
        // to create their default implementations.
        TriangleNode & operator=(const TriangleNode&) = delete;
        TriangleNode(const TriangleNode&) = delete;
#endif

        TriangleNode(
            std::shared_ptr<Vertex>  aVertex1,
            std::shared_ptr<Vertex>  aVertex2,
            std::shared_ptr<Vertex>  aVertex3,
            uint32_t                 aIndex,
            const TriangleNode      *aParentTriangle = nullptr
            ) :
            TreeNode(true),
            weight((aVertex1->weight + aVertex2->weight + aVertex3->weight) / 3.f),
            subdivLevel((aParentTriangle == nullptr) ? 0 : (aParentTriangle->subdivLevel + 1))
#ifdef _DEBUG
            ,index([aParentTriangle, aIndex](){
                std::ostringstream outStream;
                if (aParentTriangle != nullptr)
                    outStream << aParentTriangle->index << "-";
                outStream << aIndex;
                return outStream.str();
            }())
#endif
        {
            aIndex; // unused param (in debug)

            sharedVertices[0] = aVertex1;
            sharedVertices[1] = aVertex2;
            sharedVertices[2] = aVertex3;
        }

        bool operator == (const TriangleNode &aTriangle)
        {
            return (weight == aTriangle.weight)
                && (sharedVertices[0] == aTriangle.sharedVertices[0])
                && (sharedVertices[1] == aTriangle.sharedVertices[1])
                && (sharedVertices[2] == aTriangle.sharedVertices[2]);
        }

        Vec3f ComputeCrossProduct() const
        {
            const auto dir1 = (sharedVertices[1]->dir - sharedVertices[0]->dir);
            const auto dir2 = (sharedVertices[2]->dir - sharedVertices[1]->dir);

            auto crossProduct = Cross(Normalize(dir1), Normalize(dir2));

            PG3_ASSERT_VEC3F_VALID(crossProduct);

            return crossProduct;
        }

        Vec3f ComputeNormal() const
        {
            auto crossProduct = ComputeCrossProduct();

            auto lenSqr = crossProduct.LenSqr();
            if (lenSqr > 0.0001f)
                return crossProduct.Normalize();
            else
                return Vec3f(0.f);
        }

        float ComputeSurfaceArea() const
        {
            auto crossProduct = ComputeCrossProduct();
            auto surfaceArea  = crossProduct.Length();

            PG3_ASSERT_FLOAT_NONNEGATIVE(surfaceArea);

            return surfaceArea;
        }

        Vec3f ComputeCentroid() const
        {
            const Vec3f centroid = 
                (  sharedVertices[0]->dir
                 + sharedVertices[1]->dir
                 + sharedVertices[2]->dir) / 3.f;
            return centroid;
        }

        // Evaluates the linear approximation of the radiance function 
        // (without cosine multiplication) in the given direction. The direction is assumed to be 
        // pointing into the triangle.
        // TODO: Delete this?
        float EvaluateLuminanceApproxForDirection(
            const Vec3f                 &aDirection,
            const EnvironmentMapImage   &aEmImage,
            bool                         aUseBilinearFiltering
            ) const
        {
            PG3_ASSERT_VEC3F_NORMALIZED(aDirection);

            float t, u, v;
            bool isIntersection =
                Geom::RayTriangleIntersect(
                    Vec3f(0.f, 0.f, 0.f), aDirection,
                    sharedVertices[0]->dir,
                    sharedVertices[1]->dir,
                    sharedVertices[2]->dir,
                    t, u, v, 0.20f); //0.09f); //0.05f);
            u = Math::Clamp(u, 0.0f, 1.0f);
            v = Math::Clamp(v, 0.0f, 1.0f);
            const float w = Math::Clamp(1.0f - u - v, 0.0f, 1.0f);

            PG3_ASSERT(isIntersection);

            if (!isIntersection)
                return 0.0f;

            PG3_ASSERT_FLOAT_IN_RANGE(u, -0.0001f, 1.0001f);
            PG3_ASSERT_FLOAT_IN_RANGE(v, -0.0001f, 1.0001f);
            PG3_ASSERT_FLOAT_IN_RANGE(w, -0.0001f, 1.0001f);

            // TODO: Cache the luminances in the triangle
            const auto emVal0 = aEmImage.Evaluate(sharedVertices[0]->dir, aUseBilinearFiltering);
            const auto emVal1 = aEmImage.Evaluate(sharedVertices[1]->dir, aUseBilinearFiltering);
            const auto emVal2 = aEmImage.Evaluate(sharedVertices[2]->dir, aUseBilinearFiltering);
            const float luminance0 = emVal0.Luminance();
            const float luminance1 = emVal1.Luminance();
            const float luminance2 = emVal2.Luminance();

            const float approximation = 
                  u * luminance0
                + v * luminance1
                + w * luminance2;

            PG3_ASSERT_FLOAT_NONNEGATIVE(approximation);

            return approximation;
        }

        // Evaluates the linear approximation of the radiance function 
        // (without cosine multiplication) in the given barycentric coordinates.
        float EvaluateLuminanceApprox(
            const Vec2f                 &aBaryCoords,
            const EnvironmentMapImage   &aEmImage,
            bool                         aUseBilinearFiltering
            ) const
        {
            PG3_ASSERT_FLOAT_IN_RANGE(aBaryCoords.x, -0.0001f, 1.0001f);
            PG3_ASSERT_FLOAT_IN_RANGE(aBaryCoords.y, -0.0001f, 1.0001f);

            const float w = Math::Clamp(
                1.0f - aBaryCoords.x - aBaryCoords.y,
                0.0f, 1.0f);

            // TODO: Cache the luminances in the triangle
            const auto emVal0 = aEmImage.Evaluate(sharedVertices[0]->dir, aUseBilinearFiltering);
            const auto emVal1 = aEmImage.Evaluate(sharedVertices[1]->dir, aUseBilinearFiltering);
            const auto emVal2 = aEmImage.Evaluate(sharedVertices[2]->dir, aUseBilinearFiltering);
            const float luminance0 = emVal0.Luminance();
            const float luminance1 = emVal1.Luminance();
            const float luminance2 = emVal2.Luminance();

            const float approximation =
                  aBaryCoords.x * luminance0
                + aBaryCoords.y * luminance1
                + w             * luminance2;

            PG3_ASSERT_FLOAT_NONNEGATIVE(approximation);

            return approximation;
        }

    public:
        SteeringBasisValue      weight;

        uint32_t                subdivLevel;

#ifdef _DEBUG
        const std::string       index;
#endif

        // TODO: This is sub-optimal, both in terms of memory consumption and memory non-locality
        std::shared_ptr<Vertex> sharedVertices[3];
    };

public:

    // Builds the internal structures needed for sampling
    bool Build(
        uint32_t                     aMaxSubdivLevel,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        Cleanup();

        std::list<TreeNode*> tmpTriangles;

        if (!TriangulateEm(tmpTriangles, aMaxSubdivLevel, aEmImage, aUseBilinearFiltering))
            return false;

        if (!BuildTriangleTree(tmpTriangles))
            return false;

        return true;
    }

    // Generate a random direction on a sphere proportional to an adaptive piece-wise 
    // bilinear approximation of the environment map luminance.
    bool Sample(
        Vec3f       &oSampleDirection,
        float       &oSamplePdf,
        const Vec3f &aNormal,
        const Vec2f &aSample
        ) const
    {
        // TODO: Asserts:
        //  - normal: normalized
        //  - sample: [0,1]?

        // TODO: Spherical harmonics coefficients of clamped cosine for given normal
        SteeringCoefficients directionCoeffs;
        directionCoeffs.GenerateForClampedCosSh(aNormal);

        // TODO: Pick a triangle (descend the tree)
        const TriangleNode *triangle = nullptr;
        float triangleProbability = 0.0f;
        PickTriangle(triangle, triangleProbability, directionCoeffs, aSample/*may need some adjustments*/);
        if (triangle == nullptr)
            return false;

        // TODO: Sample triangle surface (bi-linear surface sampling)
        float triangleSamplePdf = 0.0f;
        SampleTriangleSurface(oSampleDirection, triangleSamplePdf, *triangle,
                              directionCoeffs, aSample/*may need some adjustments*/);
        oSamplePdf = triangleSamplePdf * triangleSamplePdf;

        return true;
    }

protected:

    // Releases the current data structures
    void Cleanup()
    {
        mTreeRoot.reset(nullptr);
    }

    static void FreeNode(TreeNode* aNode)
    {
        if (aNode == nullptr)
            return;

        if (aNode->IsTriangleNode())
            delete static_cast<TriangleNode*>(aNode);
        else
            delete static_cast<InnerNode*>(aNode);
    }

public:

    static void FreeNodesList(std::list<TreeNode*> &aNodes)
    {
        for (TreeNode* node : aNodes)
            FreeNode(node);
    }

    static void FreeNodesDeque(std::deque<TreeNode*> &aNodes)
    {
        while (!aNodes.empty())
        {
            FreeNode(aNodes.back());
            aNodes.pop_back();
        }
    }

    static void FreeTrianglesList(std::list<TriangleNode*> &aTriangles)
    {
        for (TriangleNode* triangle : aTriangles)
            delete triangle;
    }

    static void FreeTrianglesDeque(std::deque<TriangleNode*> &aTriangles)
    {
        while (!aTriangles.empty())
        {
            delete aTriangles.back();
            aTriangles.pop_back();
        }
    }

protected:

    class SingleLevelTriangulationStats
    {
    public:
        SingleLevelTriangulationStats() : mTriangleCount(0u), mSampleCount(0u) {}

        void AddTriangle()
        {
            mTriangleCount++;
        }

        void AddSample()
        {
            mSampleCount++;
        }

        uint32_t GetTriangleCount() const
        {
            return mTriangleCount;
        }

        uint32_t GetSampleCount() const
        {
            return mSampleCount;
        }

    protected:

        uint32_t mTriangleCount;
        uint32_t mSampleCount;
    };

    class TriangulationStats
    {
    public:

        TriangulationStats(const EnvironmentMapImage &aEmImage) :
            mEmWidth(aEmImage.Width()),
            mEmHeight(aEmImage.Height()),
            mEmSampleCounts(  aEmImage.Height(), std::vector<uint32_t>(aEmImage.Width(), 0))
#ifdef _DEBUG
            ,mEmHasSampleFlags(aEmImage.Height(), std::vector<std::string>(aEmImage.Width(), "*"))
#endif
        {}

        void AddTriangle(const TriangleNode &aTriangle)
        {
            if (mLevelStats.size() < (aTriangle.subdivLevel + 1))
                mLevelStats.resize(aTriangle.subdivLevel + 1);

            mLevelStats[aTriangle.subdivLevel].AddTriangle();
        }

        void AddSample(
            const TriangleNode  &aTriangle,
            const Vec3f         &aSampleDir)
        {
            if (mLevelStats.size() < (aTriangle.subdivLevel + 1))
                mLevelStats.resize(aTriangle.subdivLevel + 1);

            mLevelStats[aTriangle.subdivLevel].AddSample();

            const Vec2f uv = Geom::Dir2LatLong(aSampleDir);

            // UV to image coords
            const float x = uv.x * (float)mEmWidth;
            const float y = uv.y * (float)mEmHeight;
            const uint32_t x0 = Math::Clamp((uint32_t)x, 0u, mEmWidth  - 1u);
            const uint32_t y0 = Math::Clamp((uint32_t)y, 0u, mEmHeight - 1u);

            mEmSampleCounts[y0][x0]++;
#ifdef _DEBUG
            mEmHasSampleFlags[y0][x0] = aTriangle.index;
#endif
        }

        void Print() const
        {
#ifdef PG3_COMPUTE_AND_PRINT_EM_STEERING_STATISTICS

            printf("\nSteering Sampler - Triangulation Statistics:\n");
            if (mLevelStats.size() > 0)
            {
                uint32_t totalTriangleCount = 0u;
                uint32_t totalSampleCount = 0u;

                const size_t levels = mLevelStats.size();
                for (size_t i = 0; i < levels; ++i)
                {
                    const auto &level = mLevelStats[i];
                    printf(
                        "Level %d: triangles % 4d, samples % 6d (% 4.1f per triangle)\n",
                        i,
                        level.GetTriangleCount(),
                        level.GetSampleCount(),
                        (double)level.GetSampleCount() / level.GetTriangleCount());
                    totalTriangleCount += level.GetTriangleCount();
                    totalSampleCount   += level.GetSampleCount();
                }
                //printf("-----------------------------------------------------------\n");
                printf(
                    "Total  : triangles % 4d, samples % 6d (% 4.1f per triangle)\n",
                    totalTriangleCount,
                    totalSampleCount,
                    (double)totalSampleCount / totalTriangleCount);
            }
            else
                printf("no data!\n");

            printf("\nSteering Sampler - EM Sampling Empty Pixels:\n");
            if ((mEmWidth > 0) && (mEmHeight > 0) && !mEmSampleCounts.empty())
            {
                // Compute zero-sample pixels
                const auto maxBinCount  = 32u;
                const auto rowCount     = mEmSampleCounts.size();
                const auto binCount     = std::min((uint32_t)rowCount, maxBinCount);
                std::vector<std::pair<uint32_t, uint32_t>> zeroSampleVertHist(binCount, { 0, 0 });
                for (size_t row = 0; row < rowCount; ++row)
                {
                    const size_t binId =
                        (rowCount <= maxBinCount) ?
                        row : Math::RemapInterval<size_t>(row, rowCount - 1, binCount - 1);
                    auto &bin = zeroSampleVertHist[binId];
                    for (auto &pixelSampleCount : mEmSampleCounts[row])
                        bin.first += (pixelSampleCount == 0) ? 1 : 0; // zero count
                    bin.second += mEmWidth; // total count per row
                }

                // Print
                for (size_t row = 0; row < zeroSampleVertHist.size(); ++row)
                {
                    const auto &bin = zeroSampleVertHist[row];
                    const auto count = bin.first;
                    const auto total = bin.second;
                    const auto percent = ((count != 0) && (total != 0)) ? ((100.f * count) / total) : 0;
                    printf("bin % 4d: % 7d pixels (%5.1f%%): ", row, count, percent);
                    Utils::PrintHistogramTicks((uint32_t)std::round(percent), 100, 100);
                    printf("\n");
                }
            }
            else
                printf("no data!\n");

            //printf("\nSteering Sampler - EM Sampling Pixel Histogram:\n");
            //if ((mEmWidth > 0) && (mEmHeight > 0) && !mEmSampleCounts.empty())
            //{
            //    // Compute histogram
            //    std::vector<uint32_t> histogram;
            //    for (auto &rowCounts : mEmSampleCounts)
            //        for (auto &pixelSampleCount : rowCounts)
            //        {
            //            if (pixelSampleCount >= histogram.size())
            //                histogram.resize(pixelSampleCount + 1, 0u);
            //            histogram[pixelSampleCount]++;
            //        }

            //    // Print histogram
            //    const uint32_t maxCount = *std::max_element(histogram.begin(), histogram.end());
            //    for (size_t samples = 0; samples < histogram.size(); ++samples)
            //    {
            //        const auto &count = histogram[samples];
            //        printf("% 4d samples: % 7d pixels: ", samples, count);
            //        Utils::PrintHistogramTicks(count, maxCount, 30, (samples == 0) ? '*' : '.');
            //        printf("\n");
            //    }
            //}
            //else
            //    printf("no data!\n");

            printf("\n");

#endif
        }

        std::vector<SingleLevelTriangulationStats>  mLevelStats;
        uint32_t                                    mEmWidth;
        uint32_t                                    mEmHeight;
        std::vector<std::vector<uint32_t>>          mEmSampleCounts;

#ifdef _DEBUG
        std::vector<std::vector<std::string>>       mEmHasSampleFlags; // to be inspected within debugger
#endif
    };

public: // debug: public is for visualisation

    // Generates adaptive triangulation of the given environment map: fills the list of triangles
    static bool TriangulateEm(
        std::list<TreeNode*>        &oTriangles,
        uint32_t                     aMaxSubdivLevel,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        float                        aOversamplingFactor = 1.0f)
    {
        PG3_ASSERT(oTriangles.empty());        

        std::deque<TriangleNode*> toDoTriangles;

        TriangulationStats stats(aEmImage);

        if (!GenerateInitialEmTriangulation(toDoTriangles, aEmImage, aUseBilinearFiltering))
            return false;

        if (!RefineEmTriangulation(oTriangles, toDoTriangles, aMaxSubdivLevel,
                                   aEmImage, aUseBilinearFiltering,
                                   aOversamplingFactor, &stats))
            return false;

        stats.Print();

        PG3_ASSERT(toDoTriangles.empty());

        return true;
    }

protected:

    // Generates initial set of triangles and their vertices
    static bool GenerateInitialEmTriangulation(
        std::deque<TriangleNode*>   &oTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        aEmImage;  aUseBilinearFiltering;

        // Generate the geometrical data
        Vec3f vertices[12];
        Vec3ui faces[20];
        Geom::UnitIcosahedron(vertices, faces);

        std::vector<std::shared_ptr<Vertex>> sharedVertices(Utils::ArrayLength(vertices));

        // Allocate shared vertices for the triangles
        for (uint32_t i = 0; i < Utils::ArrayLength(vertices); i++)
            GenerateSharedVertex(sharedVertices[i], vertices[i], aEmImage, aUseBilinearFiltering);

        // Build triangle set
        for (uint32_t i = 0; i < Utils::ArrayLength(faces); i++)
        {
            const auto &faceVertices = faces[i];

            PG3_ASSERT_INTEGER_IN_RANGE(faceVertices.Get(0), 0, Utils::ArrayLength(vertices) - 1);
            PG3_ASSERT_INTEGER_IN_RANGE(faceVertices.Get(1), 0, Utils::ArrayLength(vertices) - 1);
            PG3_ASSERT_INTEGER_IN_RANGE(faceVertices.Get(2), 0, Utils::ArrayLength(vertices) - 1);

            oTriangles.push_back(new TriangleNode(
                sharedVertices[faceVertices.Get(0)],
                sharedVertices[faceVertices.Get(1)],
                sharedVertices[faceVertices.Get(2)],
                i));
        }

        return true;
    }

    static void GenerateSharedVertex(
        std::shared_ptr<Vertex>     &oSharedVertex,
        const Vec3f                 &aVertexDir,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        const auto radiance = aEmImage.Evaluate(aVertexDir, aUseBilinearFiltering);
        const auto luminance = radiance.Luminance();

        SteeringBasisValue weight;
        weight.GenerateSphHarm(aVertexDir, luminance);

        oSharedVertex = std::make_shared<Vertex>(aVertexDir, weight);
    }

    // Sub-divides the "to do" triangle set of triangles according to the refinement rule and
    // fills the output list of triangles. The refined triangles are released. The triangles 
    // are either moved from the "to do" set into the output list or deleted on error.
    // Although the "to do" triangle set is a TreeNode* container, it must contain 
    // TriangleNode* data only, otherwise an error will occur.
    static bool RefineEmTriangulation(
        std::list<TreeNode*>        &oRefinedTriangles,
        std::deque<TriangleNode*>   &aToDoTriangles,
        uint32_t                     aMaxSubdivLevel,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        float                        aOversamplingFactor = 1.0f,
        TriangulationStats          *aStats = nullptr)
    {
        PG3_ASSERT(!aToDoTriangles.empty());
        PG3_ASSERT(oRefinedTriangles.empty());

        while (!aToDoTriangles.empty())
        {
            auto currentTriangle = aToDoTriangles.front();
            aToDoTriangles.pop_front();

            PG3_ASSERT(currentTriangle != nullptr);
            PG3_ASSERT(currentTriangle->IsTriangleNode());

            if (currentTriangle == nullptr)
                continue;

            if (TriangleHasToBeSubdivided(*currentTriangle, aMaxSubdivLevel,
                                          aEmImage, aUseBilinearFiltering, aOversamplingFactor,
                                          aStats))
            {
                // Replace the triangle with sub-division triangles
                std::list<TriangleNode*> subdivisionTriangles;
                SubdivideTriangle(subdivisionTriangles, *currentTriangle, aEmImage, aUseBilinearFiltering);
                delete currentTriangle;
                for (auto triangle : subdivisionTriangles)
                    aToDoTriangles.push_front(triangle);
            }
            else
                // Move triangle to the final list
                oRefinedTriangles.push_front(currentTriangle);
        }

        PG3_ASSERT(aToDoTriangles.empty());

        return true;
    }

    static float SubdivTestSamplesPerDimension(
        const Vec3f         &aVertex0,
        const Vec3f         &aVertex1,
        const Vec3f         &aVertex2,
        const Vec2ui        &aEmSize,
        const Vec3f         &aPlanarTriangleCentroid,
        float                aMinSinClamped,
        float                aOversamplingFactor)
    {
        // Angular sample sizes based on the size of EM pixels on the equator.
        // We are ignoring the fact that there is higher angular pixel density as we go closer 
        // to poles.
        const Vec2f emPixelAngularSize{
            Math::kPiF / aEmSize.y,
            aMinSinClamped * Math::k2PiF / aEmSize.x };
        const float minPixelAngularSize = emPixelAngularSize.Min();
        const float maxAngularSampleSize =
            std::min(minPixelAngularSize / 2.0f/*Nyquist frequency*/, Math::kPiDiv2F - 0.1f);

        // The distance of the planar triangle centroid from the origin - a cheap estimate 
        // of the distance of the triangle from the origin; works well for regular triangles
        const float triangleDistEst = aPlanarTriangleCentroid.Length();

        // Planar sample size
        const float tanAngSample = std::tan(maxAngularSampleSize);
        const float maxPlanarSampleSize = tanAngSample * triangleDistEst;

        // Estimate triangle sampling density.
        // Based on the sampling frequency of a rectangular grid, but using average triangle 
        // edge length instead of rectangle size. A squared form is used to avoid unnecessary 
        // square roots.
        const auto edge0LenSqr = (aVertex0 - aVertex1).LenSqr();
        const auto edge1LenSqr = (aVertex1 - aVertex2).LenSqr();
        const auto edge2LenSqr = (aVertex2 - aVertex0).LenSqr();
        const float avgTriangleEdgeLengthSqr =
            (edge0LenSqr + edge1LenSqr + edge2LenSqr) / 3.0f;
        const float maxPlanarGridBinSizeSqr = Math::Sqr(maxPlanarSampleSize) / 2.0f;
        const float rectSamplesPerDimensionSqr = avgTriangleEdgeLengthSqr / maxPlanarGridBinSizeSqr;
        const float samplesPerDimensionSqr =
            rectSamplesPerDimensionSqr / 2.f/*triangle covers roughly half the rectangle*/;
        float samplesPerDimension = std::sqrt(samplesPerDimensionSqr);
        samplesPerDimension *= aOversamplingFactor;

        return samplesPerDimension;
    }

    static bool IsEstimationErrorTooLarge(
        const TriangleNode          &aTriangle,
        const Vec3f                 &aVertex0,
        const Vec3f                 &aVertex1,
        const Vec3f                 &aVertex2,
        uint32_t                     samplesPerDimension,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        TriangulationStats          *aStats
        )
    {
        const float binSize = 1.f / samplesPerDimension;
        for (uint32_t i = 0; i <= samplesPerDimension; ++i)
        {
            for (uint32_t j = 0; j <= samplesPerDimension; ++j)
            {
                Vec2f sample = Vec2f(Math::Sqr(i * binSize), j * binSize);

                // Sample planar triangle
                const auto triangleSampleBarycentric = Sampling::SampleUniformTriangle(sample);
                const auto approxVal = aTriangle.EvaluateLuminanceApprox(
                    triangleSampleBarycentric,
                    aEmImage, aUseBilinearFiltering);
                const auto trianglePoint = Geom::GetTrianglePoint(
                    aVertex0, aVertex1, aVertex2,
                    triangleSampleBarycentric);
                const auto sampleDir = Normalize(trianglePoint);

                const auto emRadiance = aEmImage.Evaluate(sampleDir, aUseBilinearFiltering);
                const auto emVal = emRadiance.Luminance();

                PG3_ASSERT_FLOAT_NONNEGATIVE(emVal);

                if (aStats != nullptr)
                    aStats->AddSample(aTriangle, sampleDir);

                const auto diff = std::abs(emVal - approxVal);
                const auto threshold = std::max(0.5f * emVal, 0.001f); // TODO: Parametrizable threshold
                if (diff > threshold)
                    // The approximation is too far from the original function
                    return true;
            }
        }

        return false;
    }

    static bool TriangleHasToBeSubdividedImpl(
        const TriangleNode          &aTriangle,
        const Vec3f                 &aVertex0,
        const float                  aVertex0Sin,
        const Vec3f                 &aVertex1,
        const float                  aVertex1Sin,
        const Vec3f                 &aVertex2,
        const float                  aVertex2Sin,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        float                        aOversamplingFactor = 1.0f,
        TriangulationStats          *aStats = nullptr)
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aVertex0);
        PG3_ASSERT_VEC3F_NORMALIZED(aVertex1);
        PG3_ASSERT_VEC3F_NORMALIZED(aVertex2);

        // Estimate the maximum and minimum sine(theta) value over the triangle.
        // Sine value directly affect the necessary sampling density in each EM pixel.

        const auto edgeCentre01Dir = ((aVertex0 + aVertex1) / 2.f).Normalize();
        const auto edgeCentre12Dir = ((aVertex1 + aVertex2) / 2.f).Normalize();
        const auto edgeCentre20Dir = ((aVertex2 + aVertex0) / 2.f).Normalize();
        const auto centroidDir     = aTriangle.ComputeCentroid().Normalize();

        const float edgeCentre01Sin = std::sqrt(1.f - Math::Sqr(edgeCentre01Dir.z));
        const float edgeCentre12Sin = std::sqrt(1.f - Math::Sqr(edgeCentre12Dir.z));
        const float edgeCentre20Sin = std::sqrt(1.f - Math::Sqr(edgeCentre20Dir.z));
        const float centroidSin     = std::sqrt(1.f - Math::Sqr(centroidDir.z));

        const float minSin = Math::MinN(
            aVertex0Sin, aVertex1Sin, aVertex2Sin,
            edgeCentre01Sin, edgeCentre12Sin, edgeCentre20Sin,
            centroidSin);
        const float maxSin = Math::MaxN(
            aVertex0Sin, aVertex1Sin, aVertex2Sin,
            edgeCentre01Sin, edgeCentre12Sin, edgeCentre20Sin,
            centroidSin);

        const float polePixelMidTheta = 0.5f * Math::kPiDiv2F / aEmImage.Height();
        const float polePixelSin = std::sin(polePixelMidTheta);
        const float minSinClamped = std::max(minSin, polePixelSin);
        const float maxSinClamped = std::max(maxSin, polePixelSin);

        //TODO: Sample sub-triangles independently if sines differ too much (to avoid unnecessary oversampling)
        //      Else check the whole triangle
        maxSinClamped; // unused so far

        // Determine sampling frequency
        const float samplesPerDimensionF = SubdivTestSamplesPerDimension(
            aVertex0, aVertex1, aVertex2,
            aEmImage.Size(), aTriangle.ComputeCentroid(),
            minSinClamped, aOversamplingFactor);

        // Sample
        return IsEstimationErrorTooLarge(
            aTriangle, aVertex0, aVertex1, aVertex2, (uint32_t)std::ceil(samplesPerDimensionF),
            aEmImage, aUseBilinearFiltering, aStats);
    }

    static bool TriangleHasToBeSubdivided(
        const TriangleNode          &aTriangle,
        uint32_t                     aMaxSubdivLevel,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        float                        aOversamplingFactor = 1.0f,
        TriangulationStats          *aStats = nullptr)
    {
        // TODO: Build triangle count/size limit into the sub-division criterion (if too small, stop)
        if (aTriangle.subdivLevel >= aMaxSubdivLevel)
            return false;

        if (aStats != nullptr)
            aStats->AddTriangle(aTriangle);

        const auto &vertices = aTriangle.sharedVertices;

        const float vertex0Sin = std::sqrt(1.f - Math::Sqr(vertices[0]->dir.z));
        const float vertex1Sin = std::sqrt(1.f - Math::Sqr(vertices[1]->dir.z));
        const float vertex2Sin = std::sqrt(1.f - Math::Sqr(vertices[2]->dir.z));

        bool result = TriangleHasToBeSubdividedImpl(
            aTriangle,
            vertices[0]->dir, vertex0Sin,
            vertices[1]->dir, vertex1Sin,
            vertices[2]->dir, vertex2Sin,
            aEmImage, aUseBilinearFiltering,
            aOversamplingFactor,
            aStats);

        return result;
    }

    static void SubdivideTriangle(
        std::list<TriangleNode*>    &oSubdivisionTriangles,
        const TriangleNode          &aTriangle,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        // For now just a full regular subdivision (each edge is subdivided by placing a new edge 
        // in the middle of the edge) resulting in 4 new triangles
        //    /\
        //   /__\
        //  /\  /\
        // /__\/__\

        // New vertex coordinates
        // We don't have to use slerp - normalization does the trick
        Vec3f newVertexCoords[3];
        const auto &verts = aTriangle.sharedVertices;
        newVertexCoords[0] = ((verts[0]->dir + verts[1]->dir) / 2.f).Normalize();
        newVertexCoords[1] = ((verts[1]->dir + verts[2]->dir) / 2.f).Normalize();
        newVertexCoords[2] = ((verts[2]->dir + verts[0]->dir) / 2.f).Normalize();

        // New shared vertices
        std::vector<std::shared_ptr<Vertex>> newVertices(3);
        GenerateSharedVertex(newVertices[0], newVertexCoords[0], aEmImage, aUseBilinearFiltering);
        GenerateSharedVertex(newVertices[1], newVertexCoords[1], aEmImage, aUseBilinearFiltering);
        GenerateSharedVertex(newVertices[2], newVertexCoords[2], aEmImage, aUseBilinearFiltering);

        // Central triangle
        oSubdivisionTriangles.push_back(
            new TriangleNode(newVertices[0], newVertices[1], newVertices[2], 0, &aTriangle));

        // 3 corner triangles
        const auto &oldVertices = aTriangle.sharedVertices;
        oSubdivisionTriangles.push_back(
            new TriangleNode(oldVertices[0], newVertices[0], newVertices[2], 1, &aTriangle));
        oSubdivisionTriangles.push_back(
            new TriangleNode(newVertices[0], oldVertices[1], newVertices[1], 2, &aTriangle));
        oSubdivisionTriangles.push_back(
            new TriangleNode(newVertices[1], oldVertices[2], newVertices[2], 3, &aTriangle));

        PG3_ASSERT_INTEGER_EQUAL(oSubdivisionTriangles.size(), 4);

        // LATER: Adaptive (more memory-efficient) sub-division?

    }

#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

public:

    static bool _UnitTest_SubdivideTriangle(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::SubdivideTriangle");

        // Dummy EM
        std::unique_ptr<EnvironmentMapImage>
            dummyImage(EnvironmentMapImage::LoadImage(
                ".\\Light Probes\\Debugging\\Const white 8x4.exr"));
        if (!dummyImage)
        {
            PG3_UT_FATAL_ERROR(aMaxUtBlockPrintLevel, eutblWholeTest,
                "EnvironmentMapSteeringSampler::SubdivideTriangle", "Unable to load image!");
            return false;
        }

        const float c45 = Math::kCosPiDiv4F;

        if (!_UnitTest_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant +X+Y+Z",
                { Vec3f(1.f, 0.f, 0.f), Vec3f(0.f, 1.f, 0.f), Vec3f(0.f, 0.f, 1.f) },
                { Vec3f(c45, c45, 0.f), Vec3f(0.f, c45, c45), Vec3f(c45, 0.f, c45) },
                *dummyImage, false))
            return false;

        if (!_UnitTest_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant -X-Y-Z",
                { Vec3f(-1.f,  0.f, 0.f), Vec3f(0.f, -1.f,  0.f), Vec3f( 0.f, 0.f, -1.f) },
                { Vec3f(-c45, -c45, 0.f), Vec3f(0.f, -c45, -c45), Vec3f(-c45, 0.f, -c45) },
                *dummyImage, false))
            return false;

        if (!_UnitTest_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant +X-Y+Z",
                { Vec3f(0.f, -1.f, 0.f), Vec3f(1.f, 0.f, 0.f), Vec3f(0.f,  0.f, 1.f) },
                { Vec3f(c45, -c45, 0.f), Vec3f(c45, 0.f, c45), Vec3f(0.f, -c45, c45) },
                *dummyImage, false))
            return false;

        if (!_UnitTest_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant -X+Y-Z",
                { Vec3f(0.f,  1.f, 0.f), Vec3f(-1.f, 0.f,  0.f), Vec3f(0.f, 0.f, -1.f) },
                { Vec3f(-c45, c45, 0.f), Vec3f(-c45, 0.f, -c45), Vec3f(0.f, c45, -c45) },
                *dummyImage, false))
            return false;

        if (!_UnitTest_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant +X-Y-Z",
                { Vec3f(0.f, -1.f, 0.f), Vec3f(0.f, 0.f, -1.f), Vec3f(1.f,  0.f, 0.f) },
                { Vec3f(0.f, -c45, -c45), Vec3f(c45, 0.f, -c45), Vec3f(c45, -c45, 0.f) },
                *dummyImage, false))
            return false;

        if (!_UnitTest_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant -X+Y+Z",
                { Vec3f(0.f, 1.f, 0.f), Vec3f(0.f,  0.f, 1.f), Vec3f(-1.f, 0.f, 0.f) },
                { Vec3f(0.f, c45, c45), Vec3f(-c45, 0.f, c45), Vec3f(-c45, c45, 0.f) },
                *dummyImage, false))
            return false;

        if (!_UnitTest_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant +X+Y-Z",
                { Vec3f(1.f, 0.f,  0.f), Vec3f(0.f, 0.f, -1.f), Vec3f(0.f, 1.f, 0.f) },
                { Vec3f(c45, 0.f, -c45), Vec3f(0.f, c45, -c45), Vec3f(c45, c45, 0.f) },
                *dummyImage, false))
            return false;

        if (!_UnitTest_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant -X-Y+Z",
                { Vec3f(-1.f, 0.f, 0.f), Vec3f(0.f,  0.f, 1.f), Vec3f(0.f,  -1.f, 0.f) },
                { Vec3f(-c45, 0.f, c45), Vec3f(0.f, -c45, c45), Vec3f(-c45, -c45, 0.f) },
                *dummyImage, false))
            return false;

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::SubdivideTriangle");
        return true;
    }

protected:

    static bool _UnitTest_SubdivideTriangle_SingleConfiguration(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const char                  *aTestName,
        const std::array<Vec3f, 3>  &aTriangleCoords,
        const std::array<Vec3f, 3>  &aSubdivisionPoints,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", aTestName);

        // Generate triangle with vertices
        std::vector<std::shared_ptr<Vertex>> vertices(3);
        GenerateSharedVertex(vertices[0], aTriangleCoords[0], aEmImage, aUseBilinearFiltering);
        GenerateSharedVertex(vertices[1], aTriangleCoords[1], aEmImage, aUseBilinearFiltering);
        GenerateSharedVertex(vertices[2], aTriangleCoords[2], aEmImage, aUseBilinearFiltering);
        TriangleNode triangle(vertices[0], vertices[1], vertices[2], 0);

        // Subdivide
        std::list<TriangleNode*> subdivisionTriangles;
        SubdivideTriangle(subdivisionTriangles, triangle, aEmImage, aUseBilinearFiltering);

        // Check subdivision count
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Sub-divisions count");
        if (subdivisionTriangles.size() != 4)
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Sub-divisions count",
                "Subdivision triangle count is not 4");
            return false;
        }
        for (auto subdividedTriange : subdivisionTriangles)
        {
            if (subdividedTriange == nullptr)
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Sub-divisions count",
                                  "Subdivision triangle is null");
                return false;
            }
        }
        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Sub-divisions count");

        // Check orientation
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Faces orientation");
        const auto triangleNormal = triangle.ComputeNormal();
        for (auto subdividedTriange : subdivisionTriangles)
        {
            const auto subdivNormal = subdividedTriange->ComputeNormal();
            const auto dot = Dot(subdivNormal, triangleNormal);
            if (dot < 0.90f)
            {
                PG3_UT_END_FAILED(
                    aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Faces orientation",
                    "Subdivision triangle has orientation which differs too much from the original triangle");
                return false;
            }
        }
        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Faces orientation");

        // Check vertex positions
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex positions");

        auto itSubdivs = subdivisionTriangles.begin();

        // Central triangle
        const auto &centralSubdivVertices = (**itSubdivs).sharedVertices;
        if (   !centralSubdivVertices[0]->dir.EqualsDelta(aSubdivisionPoints[0], 0.0001f)
            || !centralSubdivVertices[1]->dir.EqualsDelta(aSubdivisionPoints[1], 0.0001f)
            || !centralSubdivVertices[2]->dir.EqualsDelta(aSubdivisionPoints[2], 0.0001f))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex positions",
                "Central subdivision triangle has at least one incorrectly positioned vertex");
            return false;
        }
        itSubdivs++;

        // Corner triangle 1
        const auto &corner1SubdivVertices = (**itSubdivs).sharedVertices;
        if (   !corner1SubdivVertices[0]->dir.EqualsDelta(aTriangleCoords[0],    0.0001f)
            || !corner1SubdivVertices[1]->dir.EqualsDelta(aSubdivisionPoints[0], 0.0001f)
            || !corner1SubdivVertices[2]->dir.EqualsDelta(aSubdivisionPoints[2], 0.0001f))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex positions",
                "Corner 1 subdivision triangle has at least one incorrectly positioned vertex");
            return false;
        }
        itSubdivs++;

        // Corner triangle 2
        const auto &corner2SubdivVertices = (**itSubdivs).sharedVertices;
        if (   !corner2SubdivVertices[0]->dir.EqualsDelta(aSubdivisionPoints[0], 0.0001f)
            || !corner2SubdivVertices[1]->dir.EqualsDelta(aTriangleCoords[1],    0.0001f)
            || !corner2SubdivVertices[2]->dir.EqualsDelta(aSubdivisionPoints[1], 0.0001f))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex positions",
                "Corner 2 subdivision triangle has at least one incorrectly positioned vertex");
            return false;
        }
        itSubdivs++;

        // Corner triangle 3
        const auto &corner3SubdivVertices = (**itSubdivs).sharedVertices;
        if (   !corner3SubdivVertices[0]->dir.EqualsDelta(aSubdivisionPoints[1], 0.0001f)
            || !corner3SubdivVertices[1]->dir.EqualsDelta(aTriangleCoords[2],    0.0001f)
            || !corner3SubdivVertices[2]->dir.EqualsDelta(aSubdivisionPoints[2], 0.0001f))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex positions",
                "Corner 3 subdivision triangle has at least one incorrectly positioned vertex");
            return false;
        }
        itSubdivs++;

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex positions");
        
        // TODO: Weights??

        FreeTrianglesList(subdivisionTriangles);

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", aTestName);
        return true;
    }

#endif

protected:

    // Build a uniformly balanced tree from the provided list of nodes (typically triangles).
    // The tree is built from bottom to top, accumulating the children data into their parents.
    // The triangles are either moved from the list into the tree or deleted on error.
    bool BuildTriangleTree(std::list<TreeNode*> &aNodes)
    {
        aNodes;

        // TODO: While there is more than one node, do:
        //  - Replace pairs of nodes with nodes containing the pair as its children.
        //    Un-paired noded (if any) stays in the list for the next iteration.

        // TODO: Move the resulting node (if any) to the tree root

        // TODO: Assert: triangles list is empty
    }

    // Randomly pick a triangle with probability proportional to the integral of 
    // the piece-wise bilinear EM approximation over the triangle surface.
    TriangleNode* PickTriangle(
        const TriangleNode          *&oTriangle,
        float                        &oProbability,
        const SteeringCoefficients   &aDirectionCoeffs,
        const Vec2f                  &aSample) const
    {
        oTriangle; oProbability; aDirectionCoeffs; aSample;

        return nullptr;
    }

    // Randomly sample the surface of the triangle with probability density proportional
    // to the the piece-wise bilinear EM approximation
    void SampleTriangleSurface(
        Vec3f                       &oSampleDirection,
        float                       &oSamplePdf,
        const TriangleNode          &aTriangle,
        const SteeringCoefficients  &aDirectionCoeffs,
        const Vec2f                 &aSample) const
    {
        oSampleDirection; oSamplePdf; aTriangle; aDirectionCoeffs; aSample;

        // ...
    }

protected:

    std::unique_ptr<TreeNode>   mTreeRoot;

public:

#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

    static bool _UnitTest_TriangulateEm_SingleEm_InitialTriangulation(
        std::deque<TriangleNode*>   &oTriangles,
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation");

        if (!GenerateInitialEmTriangulation(oTriangles, aEmImage, aUseBilinearFiltering))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation",
                "GenerateInitialEmTriangulation() failed!");
            return false;
        }

        // Triangles count
        if (oTriangles.size() != 20)
        {
            std::ostringstream errorDescription;
            errorDescription << "Initial triangle count is ";
            errorDescription << oTriangles.size();
            errorDescription << " instead of 20!";
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation",
                                errorDescription.str().c_str());
            return false;
        }

        // Check each triangle
        std::list<std::set<Vertex*>> alreadyFoundFaceVertices;
        for (const auto &triangle : oTriangles)
        {
            const auto &sharedVertices = triangle->sharedVertices;

            // Each triangle is unique
            {
                std::set<Vertex*> vertexSet = {sharedVertices[0].get(),
                                               sharedVertices[1].get(),
                                               sharedVertices[2].get()};
                auto it = std::find(alreadyFoundFaceVertices.begin(),
                                    alreadyFoundFaceVertices.end(),
                                    vertexSet);
                if (it != alreadyFoundFaceVertices.end())
                {
                    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation",
                        "Found duplicate face!");
                    return false;
                }
                alreadyFoundFaceVertices.push_back(vertexSet);
            }

            // Vertices are not equal
            {
                const auto vertex0 = sharedVertices[0].get();
                const auto vertex1 = sharedVertices[1].get();
                const auto vertex2 = sharedVertices[2].get();
                if (   (vertex0 == vertex1) || (*vertex0 == *vertex1)
                    || (vertex1 == vertex2) || (*vertex1 == *vertex2)
                    || (vertex2 == vertex0) || (*vertex2 == *vertex0)
                    )
                {
                    std::ostringstream errorDescription;
                    errorDescription << "A triangle with two or more identical vertices is present. Triangles: ";
                    errorDescription << vertex0;
                    errorDescription << ", ";
                    errorDescription << vertex1;
                    errorDescription << ", ";
                    errorDescription << vertex2;
                    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation",
                        errorDescription.str().c_str());
                    return false;
                }
            }

            // Vertices and edges
            {
                const float edgeReferenceLength = 4.f / std::sqrt(10.f + 2.f * std::sqrt(5.f));
                const float edgeReferenceLengthSqr = edgeReferenceLength * edgeReferenceLength;
                for (uint32_t vertexSeqNum = 0; vertexSeqNum < 3; vertexSeqNum++)
                {
                    const auto vertex     = sharedVertices[vertexSeqNum].get();
                    const auto vertexNext = sharedVertices[(vertexSeqNum + 1) % 3].get();

                    if ((vertex == nullptr) || (vertexNext == nullptr))
                    {
                        PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation",
                                            "A triangle contains a null pointer to vertex");
                        return false;
                    }

                    // Edge length
                    const auto edgeLengthSqr = (vertex->dir - vertexNext->dir).LenSqr();
                    if (fabs(edgeLengthSqr - edgeReferenceLengthSqr) > 0.001f)
                    {
                        std::ostringstream errorDescription;
                        errorDescription << "The edge between vertices ";
                        errorDescription << vertexSeqNum;
                        errorDescription << " and ";
                        errorDescription << vertexSeqNum + 1;
                        errorDescription << " has incorrect length (sqrt(";
                        errorDescription << edgeLengthSqr;
                        errorDescription << ") instead of sqrt(";
                        errorDescription << edgeReferenceLengthSqr;
                        errorDescription << "))!";
                        PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation",
                            errorDescription.str().c_str());
                        return false;
                    }

                    // Each vertex must be shared by exactly 5 owning triangles
                    auto useCount = sharedVertices[vertexSeqNum].use_count();
                    if (useCount != 5)
                    {
                        std::ostringstream errorDescription;
                        errorDescription << "The vertex ";
                        errorDescription << vertexSeqNum;
                        errorDescription << " is shared by ";
                        errorDescription << useCount;
                        errorDescription << " owners instead of 5!";
                        PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation",
                            errorDescription.str().c_str());
                        return false;
                    }

                    // Vertex weights
                    const auto radiance  = aEmImage.Evaluate(vertex->dir, aUseBilinearFiltering);
                    const auto luminance = radiance.Luminance();
                    auto referenceWeight = 
                        SteeringBasisValue().GenerateSphHarm(vertex->dir, luminance);
                    if (vertex->weight != referenceWeight)
                    {
                        std::ostringstream errorDescription;
                        errorDescription << "Incorect weight at vertex ";
                        errorDescription << vertexSeqNum;
                        errorDescription << "!";
                        PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation",
                            errorDescription.str().c_str());
                        return false;
                    }
                }
            }
        }

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation");

        return true;
    }

    static bool _UnitTest_TriangulateEm_SingleEm_RefineTriangulation(
        std::deque<TriangleNode*>   &aInitialTriangles,
        uint32_t                     aMaxSubdivLevel,
        uint32_t                     aExpectedRefinedCount,
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement");

        std::list<TreeNode*> refinedTriangles;
        if (!RefineEmTriangulation(refinedTriangles, aInitialTriangles, aMaxSubdivLevel, aEmImage, aUseBilinearFiltering))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                "RefineEmTriangulation() failed!");
            FreeNodesList(refinedTriangles);
            return false;
        }

        // Triangles count (optional)
        if ((aExpectedRefinedCount > 0) && (refinedTriangles.size() != aExpectedRefinedCount))
        {
            std::ostringstream errorDescription;
            errorDescription << "Initial triangle count is ";
            errorDescription << refinedTriangles.size();
            errorDescription << " instead of ";
            errorDescription << aExpectedRefinedCount;
            errorDescription << "!";
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                errorDescription.str().c_str());
            FreeNodesList(refinedTriangles);
            return false;
        }

        // All vertices lie on unit sphere
        for (auto node : refinedTriangles)
        {
            auto triangle = static_cast<EnvironmentMapSteeringSampler::TriangleNode*>(node);
            auto &vertices = triangle->sharedVertices;
            if (   !Math::EqualDelta(vertices[0]->dir.LenSqr(), 1.0f, 0.001f)
                || !Math::EqualDelta(vertices[1]->dir.LenSqr(), 1.0f, 0.001f)
                || !Math::EqualDelta(vertices[2]->dir.LenSqr(), 1.0f, 0.001f))
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                    "Triangulation contains a vertex not lying on the unit sphere");
                FreeNodesList(refinedTriangles);
                return false;
            }
        }

        // Non-zero triangle size
        for (auto node : refinedTriangles)
        {
            auto triangle = static_cast<EnvironmentMapSteeringSampler::TriangleNode*>(node);
            auto surfaceArea = triangle->ComputeSurfaceArea();
            if (surfaceArea < 0.0001f)
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                    "Triangulation contains a degenerated triangle");
                FreeNodesList(refinedTriangles);
                return false;
            }
        }

        // Sanity check for normals
        for (auto node : refinedTriangles)
        {
            const auto triangle = static_cast<EnvironmentMapSteeringSampler::TriangleNode*>(node);
            const auto centroid = triangle->ComputeCentroid();
            const auto centroidDirection = Normalize(centroid);
            const auto normal = triangle->ComputeNormal();
            const auto dot = Dot(centroidDirection, normal);
            if (dot < 0.0f)
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                    "A triangle normal is oriented inside the sphere");
                FreeNodesList(refinedTriangles);
                return false;
            }
        }

        // Weights
        for (auto node : refinedTriangles)
        {
            const auto triangle = static_cast<EnvironmentMapSteeringSampler::TriangleNode*>(node);
            const auto &sharedVertices = triangle->sharedVertices;

            // Vertex weights
            for (const auto &vertex : sharedVertices)
            {
                const auto radiance = aEmImage.Evaluate(vertex->dir, aUseBilinearFiltering);
                const auto luminance = radiance.Luminance();
                const auto referenceWeight =
                    SteeringBasisValue().GenerateSphHarm(vertex->dir, luminance);
                if (vertex->weight != referenceWeight)
                {
                    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                        "Incorect triangle vertex weight");
                    FreeNodesList(refinedTriangles);
                    return false;
                }
            }

            // Triangle weight
            SteeringBasisValue referenceWeight =
                (  sharedVertices[0]->weight
                 + sharedVertices[1]->weight
                 + sharedVertices[2]->weight) / 3.0f;
            if (!referenceWeight.EqualsDelta(triangle->weight, 0.0001f))
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                    "Incorect triangle weight");
                FreeNodesList(refinedTriangles);
                return false;
            }
        }

        FreeNodesList(refinedTriangles);
        
        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement");

        return true;
    }

    static bool _UnitTest_TriangulateEm_SingleEm(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        char                        *aTestName,
        uint32_t                     aMaxSubdivLevel,
        uint32_t                     aExpectedRefinedCount,
        char                        *aImagePath,
        bool                         aUseBilinearFiltering)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", aTestName);

        std::unique_ptr<EnvironmentMapImage> image(EnvironmentMapImage::LoadImage(aImagePath));
        if (!image)
        {
            PG3_UT_FATAL_ERROR(aMaxUtBlockPrintLevel, eutblSubTestLevel1,
                               "%s", "Unable to load image!", aTestName);
            return false;
        }

        std::deque<TriangleNode*> initialTriangles;

        if (!_UnitTest_TriangulateEm_SingleEm_InitialTriangulation(initialTriangles,
                                                                   aMaxUtBlockPrintLevel,
                                                                   *image.get(),
                                                                   aUseBilinearFiltering))
        {
            FreeTrianglesDeque(initialTriangles);
            return false;
        }

        if (!_UnitTest_TriangulateEm_SingleEm_RefineTriangulation(initialTriangles,
                                                                  aMaxSubdivLevel,
                                                                  aExpectedRefinedCount,
                                                                  aMaxUtBlockPrintLevel,
                                                                  *image.get(),
                                                                  aUseBilinearFiltering))
        {
            FreeTrianglesDeque(initialTriangles);
            return false;
        }

        FreeTrianglesDeque(initialTriangles);

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", aTestName);

        return true;
    }

    static bool _UnitTest_TriangulateEm(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::TriangulateEm()");

        // TODO: Empty EM
        // TODO: Black constant EM (Luminance 0)
        // TODO: ?

        if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
                                              "Small white EM", 5, 20,
                                              ".\\Light Probes\\Debugging\\Const white 8x4.exr",
                                              false))
            return false;

        //if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
        //                                      "Large white EM",
        //                                      ".\\Light Probes\\Debugging\\Const white 1024x512.exr",
        //                                      false))
        //    return false;

        if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
                                              "Single pixel EM", 5, 0,
                                              ".\\Light Probes\\Debugging\\Single pixel.exr",
                                              false))
            return false;

        //if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
        //                                      "Satellite EM",
        //                                      ".\\Light Probes\\hdr-sets.com\\HDR_SETS_SATELLITE_01_FREE\\107_ENV_DOMELIGHT.exr",
        //                                      false))
        //    return false;

        //if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
        //                                      "Peace Garden EM",
        //                                      ".\\Light Probes\\panocapture.com\\PeaceGardens_Dusk.exr",
        //                                      false))
        //    return false;

        //if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
        //                                      "Doge2 EM",
        //                                      ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\doge2.exr",
        //                                      false))
        //    return false;

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblWholeTest,
                          "EnvironmentMapSteeringSampler::TriangulateEm()");
        return true;
    }

#endif
};
