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

        static bool _UT_GenerateSphHarm_SingleDirection(
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

        static bool _UT_GenerateSphHarm_CanonicalDirections(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            return true;
        }

        static bool _UT_GenerateSphHarm_XYDiagonalDirections(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            return true;
        }

        static bool _UT_GenerateSphHarm_YZDiagonalDirections(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            return true;
        }

        static bool _UT_GenerateSphHarm_XZDiagonalDirections(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
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
            if (!_UT_GenerateSphHarm_SingleDirection(
                    aMaxUtBlockPrintLevel, direction, referenceVal, testName))
                return false;

            return true;
        }

        static bool _UT_GenerateSphHarm(
            const UnitTestBlockLevel aMaxUtBlockPrintLevel)
        {
            PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest,
                "SteeringBasisValue::GenerateSphHarm()");

            if (!_UT_GenerateSphHarm_CanonicalDirections(aMaxUtBlockPrintLevel))
                return false;

            if (!_UT_GenerateSphHarm_XYDiagonalDirections(aMaxUtBlockPrintLevel))
                return false;

            if (!_UT_GenerateSphHarm_YZDiagonalDirections(aMaxUtBlockPrintLevel))
                return false;

            if (!_UT_GenerateSphHarm_XZDiagonalDirections(aMaxUtBlockPrintLevel))
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

    static bool _UT_SteeringValues(
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
            //, index([aParentTriangle, aIndex](){
            //    std::ostringstream outStream;
            //    if (aParentTriangle != nullptr)
            //        outStream << aParentTriangle->index << "-";
            //    outStream << aIndex;
            //    return outStream.str();
            //}())
            , index(aIndex)
#endif
        {
            aIndex; // unused param (in release)

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
            const Vec3f centroid = Geom::TriangleCentroid(
                sharedVertices[0]->dir,
                sharedVertices[1]->dir,
                sharedVertices[2]->dir);
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
        //const std::string       index;
        const uint32_t          index;
#endif

        // TODO: This is sub-optimal, both in terms of memory consumption and memory non-locality
        std::shared_ptr<Vertex> sharedVertices[3];
    };

public:

    class Parameters
    {
    public:

        Parameters(
            float   aMaxApproxError         = Math::InfinityF(),
            float   aMaxSubdivLevel         = Math::InfinityF(),
            float   aOversamplingFactorDbg  = Math::InfinityF(),
            float   aMaxTriangleSpanDbg     = Math::InfinityF())
            :
            maxApproxError(aMaxApproxError),
            maxSubdivLevel(aMaxSubdivLevel),
            oversamplingFactorDbg(aOversamplingFactorDbg),
            maxTriangleSpanDbg(aMaxTriangleSpanDbg)
        {}

        float GetMaxApproxError() const
        {
            return (maxApproxError != Math::InfinityF()) ? maxApproxError : 0.1f;
        }

        uint32_t GetMaxSubdivLevel() const
        {
            return (maxSubdivLevel != Math::InfinityF()) ? static_cast<uint32_t>(maxSubdivLevel) : 5;
        }

        float GetOversamplingFactorDbg() const
        {
            return (oversamplingFactorDbg != Math::InfinityF()) ? oversamplingFactorDbg : 0.7f;
        }

        float GetMaxTriangleSpanDbg() const
        {
            return (maxTriangleSpanDbg != Math::InfinityF()) ? maxTriangleSpanDbg : 1.1f;
        }

    protected:

        float       maxApproxError;
        float       maxSubdivLevel; // uint32_t, float used for signaling unset value

        float       oversamplingFactorDbg;
        float       maxTriangleSpanDbg;
    };

    // Builds the internal structures needed for sampling
    bool Build(
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const Parameters            &aParams)
    {
        Cleanup();

        std::list<TreeNode*> tmpTriangles;

        if (!TriangulateEm(tmpTriangles, aEmImage, aUseBilinearFiltering, aParams))
            return false;

        if (!BuildTriangleTree(tmpTriangles, mTreeRoot))
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
        SingleLevelTriangulationStats() :
            mAllTriangleCount(0u), mRemovedTriangleCount(0u), mSampleCount(0u)
        {}

        void AddTriangle()
        {
            mAllTriangleCount++;
        }

        void RemoveTriangle()
        {
            mRemovedTriangleCount++;
        }

        void AddSample()
        {
            mSampleCount++;
        }

        int32_t GetFinalTriangleCount() const
        {
            return (int32_t)mAllTriangleCount - mRemovedTriangleCount;
        }

        uint32_t GetAllTriangleCount() const
        {
            return mAllTriangleCount;
        }

        uint32_t GetSampleCount() const
        {
            return mSampleCount;
        }

    protected:

        uint32_t mAllTriangleCount;
        uint32_t mRemovedTriangleCount;
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
            //,mEmHasSampleFlags(aEmImage.Height(), std::vector<std::string>(aEmImage.Width(), "*"))
            , mEmHasSampleFrom(aEmImage.Height(), std::vector<uint32_t>(aEmImage.Width(), 0))
#endif
        {}

        bool IsActive()
        {
            return true;
        }

        void AddTriangle(const TriangleNode &aTriangle)
        {
            if (mLevelStats.size() < (aTriangle.subdivLevel + 1))
                mLevelStats.resize(aTriangle.subdivLevel + 1);

            mLevelStats[aTriangle.subdivLevel].AddTriangle();
        }

        void RemoveTriangle(const TriangleNode &aTriangle)
        {
            if (mLevelStats.size() < (aTriangle.subdivLevel + 1))
                mLevelStats.resize(aTriangle.subdivLevel + 1);

            mLevelStats[aTriangle.subdivLevel].RemoveTriangle();
        }

        void AddSample(
            const TriangleNode  &aTriangle,
            const Vec3f         &aSampleDir)
        {
            if (mLevelStats.size() < (aTriangle.subdivLevel + 1))
                mLevelStats.resize(aTriangle.subdivLevel + 1);

            mLevelStats[aTriangle.subdivLevel].AddSample();

            // Sample counts per EM pixel (just for the first level of triangles)
            if (aTriangle.subdivLevel == 0)
            {
                const Vec2f uv = Geom::Dir2LatLongFast(aSampleDir);

                // UV to image coords
                const float x = uv.x * (float)mEmWidth;
                const float y = uv.y * (float)mEmHeight;
                const uint32_t x0 = Math::Clamp((uint32_t)x, 0u, mEmWidth - 1u);
                const uint32_t y0 = Math::Clamp((uint32_t)y, 0u, mEmHeight - 1u);

                mEmSampleCounts[y0][x0]++;
#ifdef _DEBUG
                //mEmHasSampleFlags[y0][x0] = aTriangle.index;
                mEmHasSampleFrom[y0][x0] = aTriangle.index;
#endif
            }
        }

        void Print()
        {
            printf("\nSteering Sampler - Triangulation Statistics:\n");
            if (mLevelStats.size() > 0)
            {
                uint32_t totalAllTriangleCount = 0u;
                uint32_t totalFinalTriangleCount = 0u;
                uint32_t totalSampleCount = 0u;

                const size_t levels = mLevelStats.size();
                for (size_t i = 0; i < levels; ++i)
                {
                    const auto &level = mLevelStats[i];
                    const auto samplesPerTriangle = (double)level.GetSampleCount() / level.GetAllTriangleCount();
                    std::string finalTriangleCountStr, allTriangleCountStr, sampleCountStr;
                    Utils::IntegerToHumanReadable(level.GetFinalTriangleCount(), finalTriangleCountStr);
                    Utils::IntegerToHumanReadable(level.GetAllTriangleCount(), allTriangleCountStr);
                    Utils::IntegerToHumanReadable(level.GetSampleCount(), sampleCountStr);
                    printf(
                        "Level %2d: % 4s/% 4s triangles, % 4s samples (% 10.1f per triangle)\n",
                        i,
                        finalTriangleCountStr.c_str(),
                        allTriangleCountStr.c_str(),
                        sampleCountStr.c_str(),
                        samplesPerTriangle);
                    totalAllTriangleCount   += level.GetAllTriangleCount();
                    totalFinalTriangleCount += level.GetFinalTriangleCount();
                    totalSampleCount        += level.GetSampleCount();
                }

                printf("-----------------------------------------------------------\n");
                
                const auto samplesPerTriangle = (double)totalSampleCount / totalAllTriangleCount;
                std::string finalTriangleCountStr, allTriangleCountStr, sampleCountStr;
                Utils::IntegerToHumanReadable(totalFinalTriangleCount, finalTriangleCountStr);
                Utils::IntegerToHumanReadable(totalAllTriangleCount, allTriangleCountStr);
                Utils::IntegerToHumanReadable(totalSampleCount, sampleCountStr);
                printf(
                    "Total   : % 4s/% 4s triangles, % 4s samples (% 10.1f per triangle)\n",
                    finalTriangleCountStr.c_str(),
                    allTriangleCountStr.c_str(),
                    sampleCountStr.c_str(),
                    samplesPerTriangle);
            }
            else
                printf("no data!\n");

            //ComputeZeroSampleCountsVert(32);
            //printf("\nSteering Sampler - EM Sampling Empty Pixels - Vertical Histogram:\n");
            //if (!mZeroSampleCountsVert.empty())
            //{
            //    for (size_t row = 0; row < mZeroSampleCountsVert.size(); ++row)
            //    {
            //        const auto &bin = mZeroSampleCountsVert[row];
            //        const auto zeroCount = bin.first;
            //        const auto total = bin.second;
            //        const auto percent = ((zeroCount != 0) && (total != 0)) ? ((100.f * zeroCount) / total) : 0;
            //        printf("row % 5d: % 7d of % 10d pixels (%7.3f%%): ", row, zeroCount, total, percent);
            //        Utils::PrintHistogramTicks((uint32_t)std::round(percent), 100, 100);
            //        printf("\n");
            //    }
            //}
            //else
            //    printf("no data!\n");

            //ComputeZeroSampleCountsHorz(32);
            //printf("\nSteering Sampler - EM Sampling Empty Pixels - Horizontal Histogram:\n");
            //if (!mZeroSampleCountsHorz.empty())
            //{
            //    for (size_t col = 0; col < mZeroSampleCountsHorz.size(); ++col)
            //    {
            //        const auto &bin = mZeroSampleCountsHorz[col];
            //        const auto zeroCount = bin.first;
            //        const auto total = bin.second;
            //        const auto percent = ((zeroCount != 0) && (total != 0)) ? ((100.f * zeroCount) / total) : 0;
            //        printf("col % 5d: % 7d of % 10d pixels (%7.3f%%): ", col, zeroCount, total, percent);
            //        Utils::PrintHistogramTicks((uint32_t)std::round(percent), 100, 100);
            //        printf("\n");
            //    }
            //}
            //else
            //    printf("no data!\n");

            //ComputeSamplesHist();
            //printf("\nSteering Sampler - EM Sampling Pixel Histogram:\n");
            //if (!mSamplesHist.empty())
            //{
            //    // Print histogram
            //    const uint32_t maxCount = *std::max_element(mSamplesHist.begin(), mSamplesHist.end());
            //    for (size_t samples = 0; samples < mSamplesHist.size(); ++samples)
            //    {
            //        const auto &count = mSamplesHist[samples];
            //        printf("% 4d samples: % 8d pixels: ", samples, count);
            //        Utils::PrintHistogramTicks(count, maxCount, 150, (samples == 0) ? '!' : '.');
            //        printf("\n");
            //    }
            //}
            //else
            //    printf("no data!\n");

            printf("\n");
        }

        void ComputeZeroSampleCountsVert(const uint32_t maxBinCount = 0u)
        {
            if ((mEmWidth > 0) && (mEmHeight > 0) && !mEmSampleCounts.empty())
            {
                const uint32_t rowCount = static_cast<uint32_t>(mEmSampleCounts.size());
                const uint32_t binCount =
                    (maxBinCount > 0) ? std::min(rowCount, maxBinCount) : rowCount;
                mZeroSampleCountsVert.resize(binCount, { 0, 0 });
                for (size_t row = 0; row < rowCount; ++row)
                {
                    const size_t binId =
                        (rowCount <= maxBinCount) ?
                    row : Math::RemapInterval<size_t>(row, rowCount - 1, binCount - 1);
                    auto &bin = mZeroSampleCountsVert[binId];
                    for (auto &pixelSampleCount : mEmSampleCounts[row])
                        bin.first += (pixelSampleCount == 0) ? 1 : 0; // zero count
                    bin.second += mEmWidth; // total count per row
                }
            }
        }

        const std::vector<std::pair<uint32_t, uint32_t>> &GetZeroSampleCountsVert()
        {
            return mZeroSampleCountsVert;
        }

        void ComputeZeroSampleCountsHorz(const uint32_t maxBinCount = 0u)
        {
            if ((mEmWidth > 0) && (mEmHeight > 0) && !mEmSampleCounts.empty())
            {
                const uint32_t colCount = mEmWidth;
                const uint32_t binCount =
                    (maxBinCount > 0) ? std::min(colCount, maxBinCount) : colCount;
                mZeroSampleCountsHorz.resize(binCount, { 0, 0 });
                for (size_t col = 0; col < colCount; ++col)
                {
                    const size_t binId =
                        (colCount <= maxBinCount) ?
                    col : Math::RemapInterval<size_t>(col, colCount - 1, binCount - 1);
                    auto &bin = mZeroSampleCountsHorz[binId];
                    for (const auto &countsRow : mEmSampleCounts)
                        bin.first += (countsRow[col] == 0) ? 1 : 0; // zero count
                    bin.second += mEmHeight; // total count per col
                }
            }
        }

        void ComputeSamplesHist(const uint32_t maxKeyVal = 46u)
        {
            if ((mEmWidth > 0) && (mEmHeight > 0) && !mEmSampleCounts.empty())
            {
                for (auto &rowCounts : mEmSampleCounts)
                {
                    for (const auto &pixelSampleCount : rowCounts)
                    {
                        const auto keyVal = std::min((uint32_t)pixelSampleCount, maxKeyVal);
                        if (keyVal >= mSamplesHist.size())
                            mSamplesHist.resize(keyVal + 1, 0u);
                        mSamplesHist[keyVal]++;
                    }
                }
            }
        }
    protected:

        std::vector<SingleLevelTriangulationStats>  mLevelStats;
        uint32_t                                    mEmWidth;
        uint32_t                                    mEmHeight;

        // Just for the first level of triangles (which covers the sphere completely)
        std::vector<std::vector<uint32_t>>          mEmSampleCounts;

        // Computed in the post-processing step
        std::vector<std::pair<uint32_t, uint32_t>>  mZeroSampleCountsVert;
        std::vector<std::pair<uint32_t, uint32_t>>  mZeroSampleCountsHorz;
        std::vector<uint32_t>                       mSamplesHist; // samples per pixel

#ifdef _DEBUG
        //std::vector<std::vector<std::string>>       mEmHasSampleFlags; // to be inspected within debugger
        std::vector<std::vector<uint32_t>>          mEmHasSampleFrom; // to be inspected within debugger
#endif
    };

    // Empty shell for efficient switching off
    class TriangulationStatsDummy
    {
    public:

        TriangulationStatsDummy(const EnvironmentMapImage &aEmImage)
        {
            aEmImage; // unused param
        }

        bool IsActive()
        {
            return false;
        }

        void AddTriangle(const TriangleNode &aTriangle)
        {
            aTriangle; // unused param
        }

        void RemoveTriangle(const TriangleNode &aTriangle)
        {
            aTriangle; // unused param
        }

        void AddSample(
            const TriangleNode  &aTriangle,
            const Vec3f         &aSampleDir)
        {
            aTriangle, aSampleDir; // unused param
        }

        void Print() {}

        void ComputeZeroSampleCountsVert(const uint32_t maxBinCount = 0u)
        {
            maxBinCount; // unused param
        }

        const std::vector<std::pair<uint32_t, uint32_t>> &GetZeroSampleCountsVert()
        {
            return mDummyCounts;
        }

        void ComputeZeroSampleCountsHorz(const uint32_t maxBinCount = 0u)
        {
            maxBinCount; // unused param
        }

        void ComputeSamplesHist(const uint32_t maxKeyVal = 0u)
        {
            maxKeyVal; // unused param
        }

    protected:

        std::vector<std::pair<uint32_t, uint32_t>>  mDummyCounts;
    };

#if defined PG3_COMPUTE_AND_PRINT_EM_STEERING_STATISTICS && !defined PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER
    typedef TriangulationStats      TriangulationStatsSwitchable;
#else
    typedef TriangulationStatsDummy TriangulationStatsSwitchable;
#endif

public: // debug: public is for visualisation/testing purposes

    // Generates adaptive triangulation of the given environment map: fills the list of triangles
    static bool TriangulateEm(
        std::list<TreeNode*>        &oTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const Parameters            &aParams)
    {
        PG3_ASSERT(oTriangles.empty());        

        std::deque<TriangleNode*> toDoTriangles;

        TriangulationStatsSwitchable stats(aEmImage);

        if (!GenerateInitialEmTriangulation(toDoTriangles, aEmImage, aUseBilinearFiltering))
            return false;

        if (!RefineEmTriangulation(oTriangles, toDoTriangles,
                                   aEmImage, aUseBilinearFiltering,
                                   aParams, stats))
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
                i+1));
        }

        return true;
    }

    static void GenerateRandomTriangleVertices(
        Rng         &aRng,
        Vec3f       (&oCoords)[3])
    {
        float edge0LenSqr = 0.f;
        float edge1LenSqr = 0.f;
        float edge2LenSqr = 0.f;

        {
            oCoords[0] = Sampling::SampleUniformSphereW(aRng.GetVec2f());
            oCoords[1] = Sampling::SampleUniformSphereW(aRng.GetVec2f());
            oCoords[2] = Sampling::SampleUniformSphereW(aRng.GetVec2f());

            edge0LenSqr = (oCoords[0] - oCoords[1]).LenSqr();
            edge1LenSqr = (oCoords[1] - oCoords[2]).LenSqr();
            edge2LenSqr = (oCoords[2] - oCoords[0]).LenSqr();
        }
        while ((edge0LenSqr < 0.001f)
            || (edge1LenSqr < 0.001f)
            || (edge2LenSqr < 0.001f));
    }

    // Generate random triangle list. Mainly for debugging/testing purposes.
    // Triangles are guaranteed to lie on the unit sphere, but are neither guaranteed to cover
    // the whole sphere properly, nor face outside the sphere. In fact they are just a bunch of 
    // randomly genetrated triangles on a sphere.
    static void GenerateRandomTriangulation(
        std::list<TreeNode*>    &aTriangles,
        uint32_t                 aTriangleCount)
    {
        Rng rng;
        for (uint32_t triangle = 0; triangle < aTriangleCount; triangle++)
        {
            Vec3f vertexCoords[3];
            GenerateRandomTriangleVertices(rng, vertexCoords);

            float vertexLuminances[3] {
                static_cast<float>(triangle),
                static_cast<float>(triangle) + 0.3f,
                static_cast<float>(triangle) + 0.6f
            };

            std::vector<std::shared_ptr<Vertex>> sharedVertices(3);
            GenerateSharedVertex(sharedVertices[0], vertexCoords[0], vertexLuminances[0]);
            GenerateSharedVertex(sharedVertices[1], vertexCoords[1], vertexLuminances[1]);
            GenerateSharedVertex(sharedVertices[2], vertexCoords[2], vertexLuminances[2]);

            aTriangles.push_back(new TriangleNode(
                sharedVertices[0],
                sharedVertices[1],
                sharedVertices[2],
                0));
        }
    }

    static void GenerateSharedVertex(
        std::shared_ptr<Vertex>     &oSharedVertex,
        const Vec3f                 &aVertexDir,
        const float                  aLuminance)
    {
        SteeringBasisValue weight;
        weight.GenerateSphHarm(aVertexDir, aLuminance);

        oSharedVertex = std::make_shared<Vertex>(aVertexDir, weight);
    }

    static void GenerateSharedVertex(
        std::shared_ptr<Vertex>     &oSharedVertex,
        const Vec3f                 &aVertexDir,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        const auto radiance = aEmImage.Evaluate(aVertexDir, aUseBilinearFiltering);
        const auto luminance = radiance.Luminance();

        GenerateSharedVertex(oSharedVertex, aVertexDir, luminance);
    }

    // Sub-divides the "to do" triangle set of triangles according to the refinement rule and
    // fills the output list of triangles. The refined triangles are released. The triangles 
    // are either moved from the "to do" set into the output list or deleted on error.
    // Although the "to do" triangle set is a TreeNode* container, it must contain 
    // TriangleNode* data only, otherwise an error will occur.
    template <class TTriangulationStats>
    static bool RefineEmTriangulation(
        std::list<TreeNode*>        &oRefinedTriangles,
        std::deque<TriangleNode*>   &aToDoTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const Parameters            &aParams,
        TTriangulationStats         &aStats)
    {
        PG3_ASSERT(!aToDoTriangles.empty());
        PG3_ASSERT(oRefinedTriangles.empty());

        while (!aToDoTriangles.empty())
        {
            auto currentTriangle = aToDoTriangles.front();
            aToDoTriangles.pop_front();
            aStats.AddTriangle(*currentTriangle);

            PG3_ASSERT(currentTriangle != nullptr);
            PG3_ASSERT(currentTriangle->IsTriangleNode());

            if (currentTriangle == nullptr)
                continue;

            if (TriangleHasToBeSubdivided(
                    *currentTriangle, aEmImage, aUseBilinearFiltering, aParams, aStats))
            {
                // Replace the triangle with sub-division triangles
                std::list<TriangleNode*> subdivisionTriangles;
                SubdivideTriangle(subdivisionTriangles, *currentTriangle, aEmImage, aUseBilinearFiltering);
                aStats.RemoveTriangle(*currentTriangle);
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

    static void SubdivTestSamplesPerDim(
        const Vec3f         &aVertex0,
        const Vec3f         &aVertex1,
        const Vec3f         &aVertex2,
        const Vec2ui        &aEmSize,
        const Vec3f         &aPlanarTriangleCentroid,
        const float          aMinSinClamped,
        const float          aMaxSinClamped,
        float               &aMinSamplesPerDimF,
        float               &aMaxSamplesPerDimF,
        const Parameters    &aParams)
    {
        // Angular sample size based on the size of an EM pixel
        const Vec2f minEmPixelAngularSize{
                             Math::kPiF  / aEmSize.y,
            aMinSinClamped * Math::k2PiF / aEmSize.x };
        const Vec2f maxEmPixelAngularSize{
                             Math::kPiF  / aEmSize.y,
            aMaxSinClamped * Math::k2PiF / aEmSize.x };
        const Vec2f pixelAngularSizeLowBound = { // switching to vector arithmetics
            minEmPixelAngularSize.Min(),
            maxEmPixelAngularSize.Min()
        };
        const auto angularSampleSizeUpBound =
            Min(pixelAngularSizeLowBound / 2.0f/*Nyquist frequency*/, Math::kPiDiv2F - 0.1f);

        // The distance of the planar triangle centroid from the origin - a cheap estimate 
        // of the distance of the triangle from the origin; works well for regular triangles
        const float triangleDistEst = aPlanarTriangleCentroid.Length();

        // Planar sample size
        const auto tanAngSample = angularSampleSizeUpBound.Tan();
        const auto planarSampleSizeUpBound = tanAngSample * triangleDistEst;

        // Estimate triangle sampling density.
        // Based on the sampling frequency of a rectangular grid, but using average triangle 
        // edge length instead of rectangle size. A squared form is used to avoid unnecessary 
        // square roots.
        const auto edge0LenSqr = (aVertex0 - aVertex1).LenSqr();
        const auto edge1LenSqr = (aVertex1 - aVertex2).LenSqr();
        const auto edge2LenSqr = (aVertex2 - aVertex0).LenSqr();
        const float avgTriangleEdgeLengthSqr = (edge0LenSqr + edge1LenSqr + edge2LenSqr) / 3.0f;
        const auto planarGridBinSizeSqr = planarSampleSizeUpBound.Sqr() / 2.0f; //considering diagonal worst case
        const auto rectSamplesPerDimSqr = Vec2f(avgTriangleEdgeLengthSqr) / planarGridBinSizeSqr;
        const auto samplesPerDimSqr =
            rectSamplesPerDimSqr / 2.f/*triangle covers roughly half the rectangle*/;
        auto samplesPerDim = samplesPerDimSqr.Sqrt();
        const float oversamplingFactorDbg = aParams.GetOversamplingFactorDbg();
        samplesPerDim *= oversamplingFactorDbg;

        aMaxSamplesPerDimF = samplesPerDim.x; // based on the minimal sine
        aMinSamplesPerDimF = samplesPerDim.y; // based on the maximal sine
    }

    template <class TTriangulationStats>
    static bool IsEstimationErrorTooLarge(
        const TriangleNode          &aWholeTriangle,
        const Vec3f                 &aSubVertex0,
        const Vec3f                 &aSubVertex1,
        const Vec3f                 &aSubVertex2,
        uint32_t                     samplesPerDim,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const Parameters            &aParams,
        TTriangulationStats         &aStats)
    {
        const float binSize = 1.f / samplesPerDim;
        for (uint32_t i = 0; i <= samplesPerDim; ++i)
        {
            for (uint32_t j = 0; j <= samplesPerDim; ++j)
            {
                Vec2f sample = Vec2f(Math::Sqr(i * binSize), j * binSize);

                // Sample planar sub-triangle
                const auto subTriangleSampleBary = Sampling::SampleUniformTriangle(sample);
                const auto point = Geom::GetTrianglePoint(
                    aSubVertex0, aSubVertex1, aSubVertex2,
                    subTriangleSampleBary);
                const Vec2f wholeTriangleSampleBary = Geom::TriangleBarycentricCoords(
                    point,
                    aWholeTriangle.sharedVertices[0]->dir,
                    aWholeTriangle.sharedVertices[1]->dir,
                    aWholeTriangle.sharedVertices[2]->dir,
                    0.1f);

                // Evaluate
                const Vec2f wholeTriangleSampleBaryCrop = {
                    Math::Clamp(wholeTriangleSampleBary.x, 0.f, 1.f),
                    Math::Clamp(wholeTriangleSampleBary.y, 0.f, 1.f),
                };
                const auto approxVal = aWholeTriangle.EvaluateLuminanceApprox(
                    wholeTriangleSampleBaryCrop, aEmImage, aUseBilinearFiltering);
                const auto sampleDir = Normalize(point);
                const auto emRadiance = aEmImage.Evaluate(sampleDir, aUseBilinearFiltering);
                const auto emVal = emRadiance.Luminance();

                PG3_ASSERT_FLOAT_NONNEGATIVE(emVal);

                aStats.AddSample(aWholeTriangle, sampleDir);

                // Analyze error
                const auto diffAbs = std::abs(emVal - approxVal);
                const auto threshold = std::max(aParams.GetMaxApproxError() * emVal, 0.001f);
                if (diffAbs > threshold)
                    return true; // The approximation is too far from the original function
            }
        }

        return false;
    }

    template <class TTriangulationStats>
    static bool TriangleHasToBeSubdividedImpl(
        const Vec3f                 &aVertex0,
        const float                  aVertex0Sin,
        const Vec3f                 &aVertex1,
        const float                  aVertex1Sin,
        const Vec3f                 &aVertex2,
        const float                  aVertex2Sin,
        const TriangleNode          &aWholeTriangle,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const Parameters            &aParams,
        TTriangulationStats         &aStats)
    {
        PG3_ASSERT_VEC3F_NORMALIZED(aVertex0);
        PG3_ASSERT_VEC3F_NORMALIZED(aVertex1);
        PG3_ASSERT_VEC3F_NORMALIZED(aVertex2);

        if ((aEmImage.Height() == 0) || (aEmImage.Width() == 0))
            return false;

        // Estimate the maximum and minimum sine(theta) value over the triangle.
        // Sine value directly affect the necessary sampling density in each EM pixel.

        const auto triangleCentroid = Geom::TriangleCentroid(aVertex0, aVertex1, aVertex2);

        const auto edgeCentre01Dir = ((aVertex0 + aVertex1) / 2.f).Normalize();
        const auto edgeCentre12Dir = ((aVertex1 + aVertex2) / 2.f).Normalize();
        const auto edgeCentre20Dir = ((aVertex2 + aVertex0) / 2.f).Normalize();
        const auto centroidDir     = Normalize(triangleCentroid);

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

        // Determine minimal and maximal sampling frequency
        float minSamplesPerDimF, maxSamplesPerDimF;
        SubdivTestSamplesPerDim(aVertex0, aVertex1, aVertex2,
                                aEmImage.Size(), triangleCentroid,
                                minSinClamped, maxSinClamped,
                                minSamplesPerDimF, maxSamplesPerDimF,
                                aParams);

        // Sample sub-triangles independently if sines differ too much (to avoid unnecessary oversampling)
        const auto triangleSpan = maxSamplesPerDimF / minSamplesPerDimF;
        const float maxTriangleSpanDbg = aParams.GetMaxTriangleSpanDbg();
        if ((triangleSpan >= maxTriangleSpanDbg) && (maxSamplesPerDimF > 32.0f))
        {
            // Check sub-triangle near vertex 0
            if (TriangleHasToBeSubdividedImpl(
                    aVertex0, aVertex0Sin,
                    edgeCentre01Dir, edgeCentre01Sin,
                    edgeCentre20Dir, edgeCentre20Sin,
                    aWholeTriangle, aEmImage, aUseBilinearFiltering,
                    aParams, aStats))
                return true;

            // Check sub-triangle near vertex 1
            if (TriangleHasToBeSubdividedImpl(
                    aVertex1, aVertex1Sin,
                    edgeCentre12Dir, edgeCentre12Sin,
                    edgeCentre01Dir, edgeCentre01Sin,
                    aWholeTriangle, aEmImage, aUseBilinearFiltering,
                    aParams, aStats))
                return true;

            // Check sub-triangle near vertex 2
            if (TriangleHasToBeSubdividedImpl(
                    aVertex2, aVertex2Sin,
                    edgeCentre20Dir, edgeCentre20Sin,
                    edgeCentre12Dir, edgeCentre12Sin,
                    aWholeTriangle, aEmImage, aUseBilinearFiltering,
                    aParams, aStats))
                return true;

            // Check center sub-triangle
            if (TriangleHasToBeSubdividedImpl(
                    edgeCentre01Dir, edgeCentre01Sin,
                    edgeCentre12Dir, edgeCentre12Sin,
                    edgeCentre20Dir, edgeCentre20Sin,
                    aWholeTriangle, aEmImage, aUseBilinearFiltering,
                    aParams, aStats))
                return true;

            return false;
        }

        // Sample and check error
        const bool result = IsEstimationErrorTooLarge(
            aWholeTriangle, aVertex0, aVertex1, aVertex2, (uint32_t)std::ceil(maxSamplesPerDimF),
            aEmImage, aUseBilinearFiltering, aParams, aStats);

        return result;
    }

    template <class TTriangulationStats>
    static bool TriangleHasToBeSubdivided(
        const TriangleNode          &aTriangle,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const Parameters            &aParams,
        TTriangulationStats         &aStats)
    {
        // TODO: Build triangle count/size limit into the sub-division criterion (if too small, stop)
        if (aTriangle.subdivLevel >= aParams.GetMaxSubdivLevel())
            return false;

        const auto &vertices = aTriangle.sharedVertices;

        const float vertex0Sin = std::sqrt(1.f - Math::Sqr(vertices[0]->dir.z));
        const float vertex1Sin = std::sqrt(1.f - Math::Sqr(vertices[1]->dir.z));
        const float vertex2Sin = std::sqrt(1.f - Math::Sqr(vertices[2]->dir.z));

        bool result = TriangleHasToBeSubdividedImpl(
            vertices[0]->dir, vertex0Sin,
            vertices[1]->dir, vertex1Sin,
            vertices[2]->dir, vertex2Sin,
            aTriangle, aEmImage, aUseBilinearFiltering,
            aParams, aStats);

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
            new TriangleNode(newVertices[0], newVertices[1], newVertices[2], 1, &aTriangle));

        // 3 corner triangles
        const auto &oldVertices = aTriangle.sharedVertices;
        oSubdivisionTriangles.push_back(
            new TriangleNode(oldVertices[0], newVertices[0], newVertices[2], 2, &aTriangle));
        oSubdivisionTriangles.push_back(
            new TriangleNode(newVertices[0], oldVertices[1], newVertices[1], 3, &aTriangle));
        oSubdivisionTriangles.push_back(
            new TriangleNode(newVertices[1], oldVertices[2], newVertices[2], 4, &aTriangle));

        PG3_ASSERT_INTEGER_EQUAL(oSubdivisionTriangles.size(), 4);

        // LATER: Adaptive (more memory-efficient) sub-division?

    }

#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

public:

    static bool _UT_SubdivideTriangle(
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

        if (!_UT_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant +X+Y+Z",
                { Vec3f(1.f, 0.f, 0.f), Vec3f(0.f, 1.f, 0.f), Vec3f(0.f, 0.f, 1.f) },
                { Vec3f(c45, c45, 0.f), Vec3f(0.f, c45, c45), Vec3f(c45, 0.f, c45) },
                *dummyImage, false))
            return false;

        if (!_UT_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant -X-Y-Z",
                { Vec3f(-1.f,  0.f, 0.f), Vec3f(0.f, -1.f,  0.f), Vec3f( 0.f, 0.f, -1.f) },
                { Vec3f(-c45, -c45, 0.f), Vec3f(0.f, -c45, -c45), Vec3f(-c45, 0.f, -c45) },
                *dummyImage, false))
            return false;

        if (!_UT_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant +X-Y+Z",
                { Vec3f(0.f, -1.f, 0.f), Vec3f(1.f, 0.f, 0.f), Vec3f(0.f,  0.f, 1.f) },
                { Vec3f(c45, -c45, 0.f), Vec3f(c45, 0.f, c45), Vec3f(0.f, -c45, c45) },
                *dummyImage, false))
            return false;

        if (!_UT_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant -X+Y-Z",
                { Vec3f(0.f,  1.f, 0.f), Vec3f(-1.f, 0.f,  0.f), Vec3f(0.f, 0.f, -1.f) },
                { Vec3f(-c45, c45, 0.f), Vec3f(-c45, 0.f, -c45), Vec3f(0.f, c45, -c45) },
                *dummyImage, false))
            return false;

        if (!_UT_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant +X-Y-Z",
                { Vec3f(0.f, -1.f, 0.f), Vec3f(0.f, 0.f, -1.f), Vec3f(1.f,  0.f, 0.f) },
                { Vec3f(0.f, -c45, -c45), Vec3f(c45, 0.f, -c45), Vec3f(c45, -c45, 0.f) },
                *dummyImage, false))
            return false;

        if (!_UT_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant -X+Y+Z",
                { Vec3f(0.f, 1.f, 0.f), Vec3f(0.f,  0.f, 1.f), Vec3f(-1.f, 0.f, 0.f) },
                { Vec3f(0.f, c45, c45), Vec3f(-c45, 0.f, c45), Vec3f(-c45, c45, 0.f) },
                *dummyImage, false))
            return false;

        if (!_UT_SubdivideTriangle_SingleConfiguration(
                aMaxUtBlockPrintLevel, "Octant +X+Y-Z",
                { Vec3f(1.f, 0.f,  0.f), Vec3f(0.f, 0.f, -1.f), Vec3f(0.f, 1.f, 0.f) },
                { Vec3f(c45, 0.f, -c45), Vec3f(0.f, c45, -c45), Vec3f(c45, c45, 0.f) },
                *dummyImage, false))
            return false;

        if (!_UT_SubdivideTriangle_SingleConfiguration(
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

    static bool _UT_SubdivideTriangle_SingleConfiguration(
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

    // Build a balanced tree from the provided list of nodes (typically triangles).
    // The tree is built from bottom to top, accumulating the children data into their parents.
    // The triangles are either moved from the list into the tree or deleted on error.
    static bool BuildTriangleTree(
        std::list<TreeNode*>        &aNodes,
        std::unique_ptr<TreeNode>   &oTreeRoot)
    {
        aNodes, oTreeRoot;

        // TODO: While there is more than one node, do:
        //  - Replace pairs of nodes with nodes containing the pair as its children.
        //    Un-paired noded (if any) stays in the list for the next iteration.

        // TODO: Move the resulting node (if any) to the tree root

        // TODO: Assert: triangles list is empty

        return false; // debug
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

    static bool _UT_InitialTriangulation(
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

    template <class TTriangulationStats>
    static bool _UT_RefineTriangulation(
        std::deque<TriangleNode*>   &aInitialTriangles,
        uint32_t                     aMaxSubdivLevel,
        uint32_t                     aExpectedRefinedCount,
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        TTriangulationStats         &aStats,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement");

        std::list<TreeNode*> refinedTriangles;
        Parameters params(Math::InfinityF(), static_cast<float>(aMaxSubdivLevel));
        if (!RefineEmTriangulation(refinedTriangles, aInitialTriangles,
                                   aEmImage, aUseBilinearFiltering, params, aStats))
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

        // Are there pixels without error samples?
        if (aStats.IsActive())
        {
            aStats.ComputeZeroSampleCountsVert();
            const auto &zeroSampleCountsVert = aStats.GetZeroSampleCountsVert();
            if (zeroSampleCountsVert.empty())
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                    "Failed to generate ZeroSampleCountsVert");
                FreeNodesList(refinedTriangles);
                return false;
            }
            for (size_t row = 0; row < zeroSampleCountsVert.size(); ++row)
            {
                const auto &bin = zeroSampleCountsVert[row];
                const auto zeroCount = bin.first;
                const auto total = bin.second;
                const auto zeroCountPercent = ((zeroCount != 0) && (total != 0)) ? ((100.f * zeroCount) / total) : 0;
                // We should test against 0.0, but since there is a horizontal mapping problem caused by 
                // Math::FastAtan2, we need to be a little bit tolerant. When the problem is solved,
                // this should be switched to 0.0.
                if (zeroCountPercent >= 0.4)
                {
                    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                        "There is an EM row which containt more than 0.4% non-sampled pixels!");
                    FreeNodesList(refinedTriangles);
                    return false;
                }
            }
        }

        FreeNodesList(refinedTriangles);
        
        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement");

        return true;
    }

    static bool _UT_Build_SingleEm(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        char                        *aTestName,
        uint32_t                     aMaxSubdivLevel,
        uint32_t                     aExpectedRefinedCount,
        bool                         aCheckSamplingCoverage,
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

        if (!_UT_InitialTriangulation(
                initialTriangles,
                aMaxUtBlockPrintLevel,
                *image.get(),
                aUseBilinearFiltering))
        {
            FreeTrianglesDeque(initialTriangles);
            return false;
        }

        bool refinePassed;
        if (aCheckSamplingCoverage)
        {
            TriangulationStats stats(*image.get());
            refinePassed = _UT_RefineTriangulation(
                initialTriangles,
                aMaxSubdivLevel,
                aExpectedRefinedCount,
                aMaxUtBlockPrintLevel,
                stats,
                *image.get(),
                aUseBilinearFiltering);
        }
        else
        {
            TriangulationStatsDummy stats(*image.get());
            refinePassed = _UT_RefineTriangulation(
                initialTriangles,
                aMaxSubdivLevel,
                aExpectedRefinedCount,
                aMaxUtBlockPrintLevel,
                stats,
                *image.get(),
                aUseBilinearFiltering);
        }
        if (!refinePassed)
        {
            FreeTrianglesDeque(initialTriangles);
            return false;
        }

        FreeTrianglesDeque(initialTriangles);

        // TODO: Test build
        //std::list<TreeNode*> refinedTriangles; from Refine()
        //if (!_UT_BuildTriangleTree_SingleList(aMaxUtBlockPrintLevel, refinedTriangles))
        //    return false;

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", aTestName);

        return true;
    }

    static bool _UT_BuildTriangleTree_SingleList(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const UnitTestBlockLevel     aUtBlockPrintLevel,
        std::list<TreeNode*>        &aTriangles)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "BuildTriangleTree()");

        std::unique_ptr<TreeNode> treeRoot;

        if (!BuildTriangleTree(aTriangles, treeRoot))
        {
            PG3_UT_END_FAILED(
                aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "BuildTriangleTree()",
                "BuildTriangleTree() failed!");
            FreeNodesList(aTriangles);
            return false;
        }

        // TODO: ...
        // - nullptrs
        // - Leaves count (same as initial size)
        // - Height: ceil(log2(count))
        // - Both children are valid or it is a leaf
        // - Inner nodes weights: valid coef, consistent with children
        // - ...

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "BuildTriangleTree()");
        return true;
    }

    static bool _UT_BuildTriangleTree_SingleRandomList(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const UnitTestBlockLevel     aUtBlockPrintLevel,
        uint32_t                     aTriangleCount)
    {
        std::list<TreeNode*> triangles;
        GenerateRandomTriangulation(triangles, aTriangleCount);

        return _UT_BuildTriangleTree_SingleList(
            aMaxUtBlockPrintLevel, aUtBlockPrintLevel, triangles);
    }

    static bool _UT_BuildTriangleTreeSynthetic(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(
            aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::BuildTriangleTree() - Synthetic");

        // TODO: List 0

        // TODO: List 2
        if (!_UT_BuildTriangleTree_SingleRandomList(aMaxUtBlockPrintLevel, eutblSubTestLevel1, 2))
            return false;

        // TODO: List 2
        // TODO: List 3
        // TODO: List 4
        // TODO: List 5

        PG3_UT_END_PASSED(
            aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::BuildTriangleTree() - Synthetic");
        return true;
    }


    static bool _UT_Build(const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(
            aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::Build()");

        // TODO: Empty EM
        // TODO: Black constant EM (Luminance 0)
        // TODO: ?

        if (!_UT_Build_SingleEm(
                aMaxUtBlockPrintLevel,
                "Const white 8x4", 5, 20, true,
                ".\\Light Probes\\Debugging\\Const white 8x4.exr",
                false))
            return false;

        if (!_UT_Build_SingleEm(
                aMaxUtBlockPrintLevel,
                "Const white 512x256", 5, 20, true,
                ".\\Light Probes\\Debugging\\Const white 512x256.exr",
                false))
            return false;

        if (!_UT_Build_SingleEm(
                aMaxUtBlockPrintLevel,
                "Const white 1024x512", 5, 20, true,
                ".\\Light Probes\\Debugging\\Const white 1024x512.exr",
                false))
            return false;

        if (!_UT_Build_SingleEm(
                aMaxUtBlockPrintLevel,
                "Single pixel", 5, 0, false,
                ".\\Light Probes\\Debugging\\Single pixel.exr",
                false))
            return false;

        if (!_UT_Build_SingleEm(
                aMaxUtBlockPrintLevel,
                "Three point lighting 1024x512", 5, 0, false,
                ".\\Light Probes\\Debugging\\Three point lighting 1024x512.exr",
                false))
            return false;

        if (!_UT_Build_SingleEm(
                aMaxUtBlockPrintLevel,
                "Satellite", 5, 0, false,
                ".\\Light Probes\\hdr-sets.com\\HDR_SETS_SATELLITE_01_FREE\\107_ENV_DOMELIGHT.exr",
                false))
            return false;


        ///////////////

        if (!_UT_Build_SingleEm(
                aMaxUtBlockPrintLevel,
                "Doge2", 5, 0, false,
                ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\doge2.exr",
                false))
            return false;

        if (!_UT_Build_SingleEm(
                aMaxUtBlockPrintLevel,
                "Peace Garden", 5, 0, false,
                ".\\Light Probes\\panocapture.com\\PeaceGardens_Dusk.exr",
                false))
            return false;

        PG3_UT_END_PASSED(
            aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::Build()");
        return true;
    }


    static void _UnitTests(const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        //SteeringBasisValue::_UT_GenerateSphHarm(aMaxUtBlockPrintLevel);

        //_UT_SteeringValues(aMaxUtBlockPrintLevel);
        //_UT_SubdivideTriangle(aMaxUtBlockPrintLevel);
        _UT_BuildTriangleTreeSynthetic(aMaxUtBlockPrintLevel);
        //_UT_Build(aMaxUtBlockPrintLevel);
    }
#endif
};
