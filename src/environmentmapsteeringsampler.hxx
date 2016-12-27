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

        SteeringValue(float aValue)
        {
            mBasisValues.fill(aValue);
        }

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

        SteeringBasisValue(float aValue) :
            SteeringValue(aValue)
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

        bool IsValid() const
        {
            for (size_t i = 0; i < mBasisValues.size(); i++)
                if (!Math::IsValid(mBasisValues[i]))
                    return false;
            // TODO: What else?
            return true;
        }

        SteeringBasisValue operator* (const SteeringBasisValue &aValue) const
        {
            SteeringBasisValue retVal;
            for (size_t i = 0; i < mBasisValues.size(); i++)
                retVal.mBasisValues[i] = mBasisValues[i] * aValue.mBasisValues[i];
            return retVal;
        }

        SteeringBasisValue operator* (float aValue) const
        {
            SteeringBasisValue retVal;
            for (size_t i = 0; i < mBasisValues.size(); i++)
                retVal.mBasisValues[i] = mBasisValues[i] * aValue;
            return retVal;
        }

        friend SteeringBasisValue operator* (float aValueLeft, const SteeringBasisValue &aValueRight)
        {
            return aValueRight * aValueLeft;
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

        friend std::ostream &operator<< (std::ostream &aStream, const SteeringBasisValue &aBasis)
        {
            for (size_t i = 0; i < aBasis.mBasisValues.size(); i++)
            {
                if (i > 0)
                    aStream << ", ";
                aStream << aBasis.mBasisValues[i];
            }
            return aStream;
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

        //SteeringCoefficients(float aValue) :
        //    SteeringValue(aValue)
        //{}

        // Generate clamped cosine spherical harmonic coefficients for the given normal
        SteeringCoefficients& GenerateForClampedCosSh(const Vec3f &aNormal)
        {
            aNormal;

            // TODO: Asserts:
            //  - normal: normalized

            // TODO: Get from paper...

            return *this;
        }

        friend std::ostream &operator<< (std::ostream &aStream, const SteeringCoefficients &aBasis)
        {
            PG3_ERROR_CODE_NOT_TESTED("");

            for (size_t i = 0; i < aBasis.mBasisValues.size(); i++)
            {
                if (i > 0)
                    aStream << ", ";
                aStream << aBasis.mBasisValues[i];
            }
            return aStream;
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

        class BuildParameters
        {
        public:

            BuildParameters(
                float   aMaxApproxError = Math::InfinityF(),
                float   aMaxSubdivLevel = Math::InfinityF(),
                float   aOversamplingFactorDbg = Math::InfinityF(),
                float   aMaxTriangleSpanDbg = Math::InfinityF())
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


public:

    class VertexStorage
    {
    public:

        bool IsEmpty() const
        {
            return mVertices.empty();
        }

        void PreAllocate(uint32_t aSize)
        {
            mVertices.reserve(aSize);
        }

        bool AddVertex(const Vertex &aVertex, uint32_t oIndex)
        {
            PG3_ERROR_CODE_NOT_TESTED("");

            mVertices.push_back(aVertex);

            if (!mVertices.empty())
            {
                oIndex = static_cast<uint32_t>(mVertices.size()) - 1u;
                return true;
            }
            else
                return false;
        }

        bool AddVertex(Vertex &&aVertex, uint32_t &oIndex)
        {
            mVertices.push_back(aVertex);

            if (!mVertices.empty())
            {
                oIndex = static_cast<uint32_t>(mVertices.size()) - 1u;
                return true;
            }
            else
                return false;
        }

        Vertex *Get(uint32_t aIndex)
        {
            PG3_ASSERT(aIndex < static_cast<uint32_t>(mVertices.size()));

            if (aIndex < static_cast<uint32_t>(mVertices.size()))
                return &mVertices[aIndex];
            else
                return nullptr;
        }

        const Vertex *Get(uint32_t aIndex) const
        {
            PG3_ASSERT(aIndex < static_cast<uint32_t>(mVertices.size()));

            if (aIndex < static_cast<uint32_t>(mVertices.size()))
                return &mVertices[aIndex];
            else
                return nullptr;
        }

        uint32_t GetCount() const
        {
            return static_cast<uint32_t>(mVertices.size());
        }

        void Free()
        {
            mVertices.clear();
        }

        bool operator == (const VertexStorage &aStorage) const
        {
            return mVertices == aStorage.mVertices;
        }

        bool operator != (const VertexStorage &aStorage) const
        {
            return !(*this == aStorage);
        }

    protected:
        std::vector<Vertex> mVertices;
    };


    class TreeNodeBase
    {
    protected:

        TreeNodeBase(bool aIsTriangleNode, const SteeringBasisValue &aWeight) :
            mIsTriangleNode(aIsTriangleNode),
            mWeight(aWeight)
        {}

    public:

        bool IsTriangleNode() const
        {
            return mIsTriangleNode;
        }

        SteeringBasisValue GetWeight() const
        {
            return mWeight;
        }

    protected:
        bool                mIsTriangleNode; // node can be either triangle (leaf) or inner node
        SteeringBasisValue  mWeight;
    };


    class TriangleSetNode : public TreeNodeBase
    {
    public:

        // The node becomes the owner of the children and is responsible for releasing them
        TriangleSetNode(
            TreeNodeBase* aLeftChild,
            TreeNodeBase* aRightChild)
            :
            TreeNodeBase(
                false,
                [aLeftChild, aRightChild](){
                    if ((aLeftChild != nullptr) && (aRightChild != nullptr))
                        return aLeftChild->GetWeight() + aRightChild->GetWeight();
                    else
                        return SteeringBasisValue();
                }()),
            mLeftChild(aLeftChild),
            mRightChild(aRightChild)
        {}


        ~TriangleSetNode() {}


        bool operator == (const TriangleSetNode &aTriangleSet) const
        {
            return (*GetLeftChild()  == *aTriangleSet.GetLeftChild())
                && (*GetRightChild() == *aTriangleSet.GetRightChild());
        }


        bool operator != (const TriangleSetNode &aTriangleSet)
        {
            return !(*this == aTriangleSet);
        }


        const TreeNodeBase* GetLeftChild()  const { return mLeftChild.get(); }
        const TreeNodeBase* GetRightChild() const { return mRightChild.get(); }

    protected:
        // Children - owned by the node
        std::unique_ptr<TreeNodeBase> mLeftChild;
        std::unique_ptr<TreeNodeBase> mRightChild;
    };


    class TriangleNode : public TreeNodeBase
    {
    public:

#ifdef _DEBUG
        // This class is not copyable because of a const member.
        // If we don't delete the assignment operator
        // explicitly, the compiler may complain about not being able 
        // to create their default implementations.
        TriangleNode & operator=(const TriangleNode&) = delete;
        //TriangleNode(const TriangleNode&) = delete;
#endif


        TriangleNode(
            uint32_t                 aVertexIndex0,
            uint32_t                 aVertexIndex1,
            uint32_t                 aVertexIndex2,
            VertexStorage           &aVertexStorage,
            uint32_t                 aIndex,
            uint32_t                 aSubdivLevel)
            :
            TreeNodeBase(
                true,
                ComputeTriangleWeight(aVertexIndex0, aVertexIndex1, aVertexIndex2, aVertexStorage)),
            subdivLevel(aSubdivLevel)
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

            vertexIndices[0] = aVertexIndex0;
            vertexIndices[1] = aVertexIndex1;
            vertexIndices[2] = aVertexIndex2;
        }


        TriangleNode(
            uint32_t                 aVertexIndex0,
            uint32_t                 aVertexIndex1,
            uint32_t                 aVertexIndex2,
            VertexStorage           &aVertexStorage,
            uint32_t                 aIndex,
            const TriangleNode      *aParentTriangle = nullptr) // only for the subdivision process
            :
            TriangleNode(
                aVertexIndex0,
                aVertexIndex1,
                aVertexIndex2,
                aVertexStorage,
                aIndex,
                (aParentTriangle == nullptr) ? 0 : (aParentTriangle->subdivLevel + 1))
        {}


        SteeringBasisValue ComputeTriangleWeight(
            uint32_t                 aVertexIndex0,
            uint32_t                 aVertexIndex1,
            uint32_t                 aVertexIndex2,
            VertexStorage           &aVertexStorage)
        {
            const auto vertex0 = aVertexStorage.Get(aVertexIndex0);
            const auto vertex1 = aVertexStorage.Get(aVertexIndex1);
            const auto vertex2 = aVertexStorage.Get(aVertexIndex2);

            if ((vertex0 == nullptr) || (vertex1 == nullptr) || (vertex2 == nullptr))
                return SteeringBasisValue(0.f);

            const float area =
                Geom::TriangleSurfaceArea(vertex0->dir, vertex1->dir, vertex2->dir);
            const auto averageVertexWeight =
                (vertex0->weight + vertex1->weight + vertex2->weight) / 3.f;

            return averageVertexWeight * area;
        }


        bool operator == (const TriangleNode &aTriangle) const
        {
            return (mWeight == aTriangle.mWeight)
                && (vertexIndices[0] == aTriangle.vertexIndices[0])
                && (vertexIndices[1] == aTriangle.vertexIndices[1])
                && (vertexIndices[2] == aTriangle.vertexIndices[2]);
        }


        bool operator != (const TriangleNode &aTriangle)
        {
            return !(*this == aTriangle);
        }


        Vec3f ComputeCrossProduct(const VertexStorage &aVertexStorage) const
        {
            return Geom::TriangleCrossProduct(
                aVertexStorage.Get(vertexIndices[0])->dir,
                aVertexStorage.Get(vertexIndices[1])->dir,
                aVertexStorage.Get(vertexIndices[2])->dir);
        }


        Vec3f ComputeNormal(const VertexStorage &aVertexStorage) const
        {
            auto crossProduct = ComputeCrossProduct(aVertexStorage);

            auto lenSqr = crossProduct.LenSqr();
            if (lenSqr > 0.0001f)
                return crossProduct.Normalize();
            else
                return Vec3f(0.f);
        }


        float ComputeSurfaceArea(const VertexStorage &aVertexStorage) const
        {
            return Geom::TriangleSurfaceArea(
                aVertexStorage.Get(vertexIndices[0])->dir,
                aVertexStorage.Get(vertexIndices[1])->dir,
                aVertexStorage.Get(vertexIndices[2])->dir);
        }


        Vec3f ComputeCentroid(const VertexStorage &aVertexStorage) const
        {
            const Vec3f centroid = Geom::TriangleCentroid(
                aVertexStorage.Get(vertexIndices[0])->dir,
                aVertexStorage.Get(vertexIndices[1])->dir,
                aVertexStorage.Get(vertexIndices[2])->dir);
            return centroid;
        }

        // Evaluates the linear approximation of the radiance function 
        // (without cosine multiplication) in the given direction. The direction is assumed to be 
        // pointing into the triangle.
        // TODO: Delete this?
        float EvaluateLuminanceApproxForDirection(
            const Vec3f                 &aDirection,
            const VertexStorage         &aVertexStorage,
            const EnvironmentMapImage   &aEmImage,
            bool                         aUseBilinearFiltering
            ) const
        {
            PG3_ASSERT_VEC3F_NORMALIZED(aDirection);

            const auto &dir0 = aVertexStorage.Get(vertexIndices[0])->dir;
            const auto &dir1 = aVertexStorage.Get(vertexIndices[1])->dir;
            const auto &dir2 = aVertexStorage.Get(vertexIndices[2])->dir;

            float t, u, v;
            bool isIntersection =
                Geom::RayTriangleIntersect(
                    Vec3f(0.f, 0.f, 0.f), aDirection,
                    dir0, dir1, dir2,
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
            const auto emVal0 = aEmImage.Evaluate(dir0, aUseBilinearFiltering);
            const auto emVal1 = aEmImage.Evaluate(dir1, aUseBilinearFiltering);
            const auto emVal2 = aEmImage.Evaluate(dir2, aUseBilinearFiltering);
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
            const VertexStorage         &aVertexStorage,
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
            const auto &dir0 = aVertexStorage.Get(vertexIndices[0])->dir;
            const auto &dir1 = aVertexStorage.Get(vertexIndices[1])->dir;
            const auto &dir2 = aVertexStorage.Get(vertexIndices[2])->dir;
            const auto emVal0 = aEmImage.Evaluate(dir0, aUseBilinearFiltering);
            const auto emVal1 = aEmImage.Evaluate(dir1, aUseBilinearFiltering);
            const auto emVal2 = aEmImage.Evaluate(dir2, aUseBilinearFiltering);
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
        uint32_t            subdivLevel; // Used only for building the tree and introspection

#ifdef _DEBUG
        //const std::string   index;
        const uint32_t      index;
#endif

        // Indices of shared vertices pointing into a VertexStorage
        uint32_t            vertexIndices[3];
    };


    friend bool operator == (const TreeNodeBase &aNode1, const TreeNodeBase &aNode2)
    {
        if (aNode1.IsTriangleNode() != aNode2.IsTriangleNode())
            return false;

        if (aNode1.IsTriangleNode())
        {
            const TriangleNode *triangle1 = static_cast<const TriangleNode*>(&aNode1);
            const TriangleNode *triangle2 = static_cast<const TriangleNode*>(&aNode2);
            return (*triangle1 == *triangle2);
        }
        else
        {
            const TriangleSetNode *triangleSet1 = static_cast<const TriangleSetNode*>(&aNode1);
            const TriangleSetNode *triangleSet2 = static_cast<const TriangleSetNode*>(&aNode2);
            return (*triangleSet1 == *triangleSet2);
        }
    }


    friend bool operator != (const TreeNodeBase &aNode1, const TreeNodeBase &aNode2)
    {
        return !(aNode1 == aNode2);
    }


protected:

    // Builds the internal structures needed for sampling
    bool Build(
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const BuildParameters       &aParams)
    {
        Cleanup();

        std::list<TreeNodeBase*> tmpTriangles;

        if (!TriangulateEm(tmpTriangles, mVertexStorage, aEmImage, aUseBilinearFiltering, aParams))
            return false;

        if (!BuildTriangleTree(tmpTriangles, mTreeRoot))
            return false;

        return true;
    }


    static void CountNodes(
        const TreeNodeBase      *aNode,
        uint32_t                &oNonTriangleCount,
        uint32_t                &oTriangleCount)
    {
        if (aNode == nullptr)
            return;

        if (!aNode->IsTriangleNode())
        {
            oNonTriangleCount++;

            const TriangleSetNode *triangleSetNode = static_cast<const TriangleSetNode*>(aNode);
            CountNodes(triangleSetNode->GetLeftChild(),  oNonTriangleCount, oTriangleCount);
            CountNodes(triangleSetNode->GetRightChild(), oNonTriangleCount, oTriangleCount);
        }
        else
            oTriangleCount++;
    }


    static bool GenerateSaveFilePath(
        std::string                         &oPath,
        const EnvironmentMapImage           &aEmImage,
        bool                                 aUseBilinearFiltering,
        const BuildParameters               &aParams)
    {
        std::string emDirPath;
        std::string emFilenameWithExt;

        const auto emPath = aEmImage.Filename();
        if (!Utils::IO::GetDirAndFileName(emPath.c_str(), emDirPath, emFilenameWithExt))
            return false;

        // Build the full path

        std::ostringstream ossPath;

        ossPath << emDirPath;
        ossPath << emFilenameWithExt;
        ossPath << ".";

        ossPath << (aUseBilinearFiltering ? "bi" : "nn");

        ossPath << "_e";
        ossPath << std::setprecision(2) << aParams.GetMaxApproxError();

        ossPath << "_sl";
        ossPath << aParams.GetMaxSubdivLevel();

        ossPath << "_ts";
        ossPath << std::setprecision(2) << aParams.GetMaxTriangleSpanDbg();

        ossPath << "_os";
        ossPath << std::setprecision(2) << aParams.GetOversamplingFactorDbg();

        ossPath << ".emssd";

        oPath = ossPath.str();

        return true;
    }


    static const char * SaveLoadFileHeader10()
    {
        return "Environment Map Steering Sampler Data, format ver. 1.0\n";
    }


    static bool SaveToDisk10_HeaderAndParams(
        std::ofstream                   &aOfs,
        const BuildParameters           &aParams,
        const bool                       aUseDebugSave)
    {
        // Header
        Utils::IO::WriteStringToStream(aOfs, SaveLoadFileHeader10(), aUseDebugSave);

        // Build parameters
        Utils::IO::WriteVariableToStream(aOfs, aParams.GetMaxApproxError(),         aUseDebugSave);
        Utils::IO::WriteVariableToStream(aOfs, aParams.GetMaxSubdivLevel(),         aUseDebugSave);
        Utils::IO::WriteVariableToStream(aOfs, aParams.GetMaxTriangleSpanDbg(),     aUseDebugSave);
        Utils::IO::WriteVariableToStream(aOfs, aParams.GetOversamplingFactorDbg(),  aUseDebugSave);

        return true;
    }


    static bool SaveToDisk10_Vertices(
        std::ofstream                   &aOfs,
        const VertexStorage             &aVertexStorage,
        const bool                       aUseDebugSave)
    {
        // Count
        const uint32_t count = aVertexStorage.GetCount();
        Utils::IO::WriteVariableToStream(aOfs, count, aUseDebugSave);

        // List of vertices
        for (uint32_t vertexIndex = 0u; vertexIndex < count; ++vertexIndex)
        {
            const Vertex *vertex = aVertexStorage.Get(vertexIndex);
            Utils::IO::WriteVariableToStream(aOfs, vertex->dir, aUseDebugSave);
            Utils::IO::WriteVariableToStream(aOfs, vertex->weight, aUseDebugSave);
        }

        return true;
    }


    static bool SaveToDisk10_TreeNode(
        std::ofstream                   &aOfs,
        const TreeNodeBase              *aNode,
        const bool                       aUseDebugSave)
    {
        if (aNode == nullptr)
            return false;

        Utils::IO::WriteVariableToStream(aOfs, aNode->IsTriangleNode(), aUseDebugSave);

        if (!aNode->IsTriangleNode())
        {
            const TriangleSetNode *triangleSetNode = static_cast<const TriangleSetNode*>(aNode);
            SaveToDisk10_TreeNode(aOfs, triangleSetNode->GetLeftChild(),  aUseDebugSave);
            SaveToDisk10_TreeNode(aOfs, triangleSetNode->GetRightChild(), aUseDebugSave);
        }
        else
        {
            const TriangleNode *triangleNode = static_cast<const TriangleNode*>(aNode);
            Utils::IO::WriteVariableToStream(aOfs, triangleNode->subdivLevel, aUseDebugSave);
            Utils::IO::WriteVariableToStream(aOfs, triangleNode->vertexIndices, aUseDebugSave);
        }

        return true;
    }

        
    static bool SaveToDisk10_Tree(
        std::ofstream                   &aOfs,
        const TreeNodeBase              *aTreeRoot,
        const bool                       aUseDebugSave)
    {
        // Counts
        uint32_t nonTriangleCount = 0u, triangleCount = 0u;
        CountNodes(aTreeRoot, nonTriangleCount, triangleCount);
        Utils::IO::WriteVariableToStream(aOfs, nonTriangleCount, aUseDebugSave);
        Utils::IO::WriteVariableToStream(aOfs,    triangleCount, aUseDebugSave);

        // Nodes
        SaveToDisk10_TreeNode(aOfs, aTreeRoot, aUseDebugSave);

        return true;
    }


    // Save internal structures needed for sampling to disk
    static bool SaveToDisk10(
        const VertexStorage             &aVertexStorage,
        const TreeNodeBase              *aTreeRoot,
        const EnvironmentMapImage       &aEmImage,
        bool                             aUseBilinearFiltering,
        const BuildParameters           &aParams,
        const bool                       aUseDebugSave = false)
    {
        // Is tree built?
        if ((aTreeRoot == nullptr) || aVertexStorage.IsEmpty())
            return false;

        // Open file
        std::string savePath;
        if (!GenerateSaveFilePath(savePath, aEmImage, aUseBilinearFiltering, aParams))
            return false;
        std::ofstream ofs(savePath, std::ios::binary | std::ios::trunc);
        if (ofs.fail())
            return false;
        if (!ofs.is_open())
            return false;

        // Header and Params
        if (!SaveToDisk10_HeaderAndParams(ofs, aParams, aUseDebugSave))
            return false;

        // Vertices
        if (!SaveToDisk10_Vertices(ofs, aVertexStorage, aUseDebugSave))
            return false;

        // Tree
        if (!SaveToDisk10_Tree(ofs, aTreeRoot, aUseDebugSave))
            return false;

        return true;
    }


    // Save internal structures needed for sampling to disk
    bool SaveToDisk(
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const BuildParameters       &aParams) const
    {
        if (!IsBuilt())
            return false;

        return SaveToDisk10(mVertexStorage, mTreeRoot.get(), aEmImage, aUseBilinearFiltering, aParams);
    }


    static bool LoadFromDisk10_HeaderAndParams(
        std::ifstream                   &aIfs,
        const BuildParameters           &aParams)
    {
        // Header
        const char *header = SaveLoadFileHeader10();
        const size_t buffSize = strlen(header) + 1; // with trailing zero
        std::unique_ptr<char> buff(new char[buffSize]);
        if (!Utils::IO::LoadStringFromStream(aIfs, buff.get(), buffSize))
            return false;
        if (strcmp(header, buff.get()) != 0)
            return false; // Wrong header

        // Build parameters

        float maxApproxError;
        uint32_t maxSubdivLevel;
        float maxTriangleSpanDbg;
        float oversamplingFactorDbg;

        // TODO: ?LoadVariableFromStreamAndCompare()?

        if (!Utils::IO::LoadVariableFromStream(aIfs, maxApproxError))
            return false;
        if (!Utils::IO::LoadVariableFromStream(aIfs, maxSubdivLevel))
            return false;
        if (!Utils::IO::LoadVariableFromStream(aIfs, maxTriangleSpanDbg))
            return false;
        if (!Utils::IO::LoadVariableFromStream(aIfs, oversamplingFactorDbg))
            return false;

        if (maxApproxError != aParams.GetMaxApproxError())
            return false;
        if (maxSubdivLevel != aParams.GetMaxSubdivLevel())
            return false;
        if (maxTriangleSpanDbg != aParams.GetMaxTriangleSpanDbg())
            return false;
        if (oversamplingFactorDbg != aParams.GetOversamplingFactorDbg())
            return false;

        return true;
    }


    static bool LoadFromDisk10_Vertices(
        std::ifstream                   &aIfs,
        VertexStorage                   &aVertexStorage)
    {
        // Count
        uint32_t count;
        if (!Utils::IO::LoadVariableFromStream(aIfs, count))
            return false;
        aVertexStorage.PreAllocate(count); // TODO: count check and/or exception handling?

        // List of vertices
        for (uint32_t vertexIndex = 0u; vertexIndex < count; ++vertexIndex)
        {
            Vec3f dir;
            SteeringBasisValue weight;
            if (!Utils::IO::LoadVariableFromStream(aIfs, dir))
                return false;
            if (!Utils::IO::LoadVariableFromStream(aIfs, weight))
                return false;
            aVertexStorage.AddVertex(Vertex(dir, weight), vertexIndex);
        }

        return true;
    }


    static bool LoadFromDisk10_TreeNode(
        std::ifstream                   &aIfs,
        VertexStorage                   &aVertexStorage,
        std::unique_ptr<TreeNodeBase>   &oNode)
    {
        bool isTriangleNode;
        if (!Utils::IO::LoadVariableFromStream(aIfs, isTriangleNode))
            return false;

        if (!isTriangleNode)
        {
            std::unique_ptr<TreeNodeBase> leftChild, rightChild;
            if (!LoadFromDisk10_TreeNode(aIfs, aVertexStorage, leftChild))
                return false;
            if (!LoadFromDisk10_TreeNode(aIfs, aVertexStorage, rightChild))
                return false;

            oNode.reset(new TriangleSetNode(leftChild.release(), rightChild.release()));
        }
        else
        {
            uint32_t subdivLevel;
            uint32_t vertexIndices[3];
            if (!Utils::IO::LoadVariableFromStream(aIfs, subdivLevel))
                return false;
            if (!Utils::IO::LoadVariableFromStream(aIfs, vertexIndices))
                return false;

            if (   (aVertexStorage.Get(vertexIndices[0]) == nullptr)
                || (aVertexStorage.Get(vertexIndices[1]) == nullptr)
                || (aVertexStorage.Get(vertexIndices[2]) == nullptr))
                return false;

            oNode.reset(
                new TriangleNode(
                    vertexIndices[0],
                    vertexIndices[1],
                    vertexIndices[2],
                    aVertexStorage,
                    0, // Ignoring index - it is used only for debugging triangle sub-division
                    subdivLevel));
        }

        return true;
    }


    static bool LoadFromDisk10_Tree(
        std::ifstream                   &aIfs,
        VertexStorage                   &aVertexStorage,
        std::unique_ptr<TreeNodeBase>   &aTreeRoot)
    {
        // TODO: Pre-allocate nodes (nodes storage/tree wrapper?) with count check and/or exception handling?

        // Counts
        uint32_t nonTriangleCount, triangleCount;
        if (!Utils::IO::LoadVariableFromStream(aIfs, nonTriangleCount))
            return false;
        if (!Utils::IO::LoadVariableFromStream(aIfs, triangleCount))
            return false;

        // Nodes
        if (!LoadFromDisk10_TreeNode(aIfs, aVertexStorage, aTreeRoot))
            return false;

        // Sanity check: node counts
        uint32_t treeNonTriangleCount = 0u, treeTriangleCount = 0u;
        CountNodes(aTreeRoot.get(), treeNonTriangleCount, treeTriangleCount);
        if ((nonTriangleCount != treeNonTriangleCount) || (triangleCount != treeTriangleCount))
            return false;

        return true;
    }


    // Loads pre-built internal structures needed for sampling
    static bool LoadFromDisk10(
        VertexStorage                   &aVertexStorage,
        std::unique_ptr<TreeNodeBase>   &aTreeRoot,
        const EnvironmentMapImage       &aEmImage,
        bool                             aUseBilinearFiltering,
        const BuildParameters           &aParams)
    {
        // Clean-up data structures
        aTreeRoot.reset(nullptr);
        aVertexStorage.Free();

        // Open file
        std::string savePath;
        if (!GenerateSaveFilePath(savePath, aEmImage, aUseBilinearFiltering, aParams))
            return false;
        std::ifstream ifs(savePath, std::ios::binary);
        if (ifs.fail())
            return false;
        if (!ifs.is_open())
            return false;

        // Header
        if (!LoadFromDisk10_HeaderAndParams(ifs, aParams))
            return false;

        // Vertices
        if (!LoadFromDisk10_Vertices(ifs, aVertexStorage))
            return false;

        // Tree
        if (!LoadFromDisk10_Tree(ifs, aVertexStorage, aTreeRoot))
            return false;

        // Sanity tests on stream
        {
            if (ifs.bad())
                return false;

            // Did we reach the end of file just now?
            // ...we need to try read something to find out
            char dummy;
            ifs >> dummy;
            if (!ifs.eof())
                return false;
        }

        // TODO: Checks: magic numbers padding, numbers validity, usage of all vertices, ...)

        // TODO: (Optional, UT?) Sanity check (error criterion?, spherical coverage?, ...)

        return true;
    }


    // Loads pre-built internal structures needed for sampling
    bool LoadFromDisk(
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const BuildParameters       &aParams)
    {
        Cleanup();

        if (!LoadFromDisk10(mVertexStorage, mTreeRoot, aEmImage, aUseBilinearFiltering, aParams))
        {
            Cleanup();
            return false;
        }
        else
            return true;
    }


public:

    // Builds the internal structures needed for sampling
    bool Init(
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const BuildParameters       &aParams)
    {
        // Building the tree is slow - try to load a pre-built tree from disk
        if (LoadFromDisk(aEmImage, aUseBilinearFiltering, aParams))
            return true;

        // Not loaded - build a new tree
        if (Build(aEmImage, aUseBilinearFiltering, aParams))
        {
            if (!SaveToDisk(aEmImage, aUseBilinearFiltering, aParams))
            {
                // TODO: Print a warning
            }
            return true;
        }

        return false;
    }

    bool IsBuilt() const
    {
        return (mTreeRoot.get() != nullptr) && (!mVertexStorage.IsEmpty());
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
        mVertexStorage.Free();
    }

    static void FreeNode(TreeNodeBase* aNode)
    {
        if (aNode == nullptr)
            return;

        if (aNode->IsTriangleNode())
            delete static_cast<TriangleNode*>(aNode);
        else
            delete static_cast<TriangleSetNode*>(aNode);
    }

public:

    static void FreeNodesList(std::list<TreeNodeBase*> &aNodes)
    {
        for (TreeNodeBase* node : aNodes)
            FreeNode(node);
    }

    static void FreeNodesDeque(std::deque<TreeNodeBase*> &aNodes)
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
                        "Level %2u: % 4s/% 4s triangles, % 4s samples (% 10.1f per triangle)\n",
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
        std::list<TreeNodeBase*>    &oTriangles,
        VertexStorage               &aVertexStorage,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const BuildParameters       &aParams)
    {
        PG3_ASSERT(oTriangles.empty());        

        std::deque<TriangleNode*> toDoTriangles;

        TriangulationStatsSwitchable stats(aEmImage);

        if (!GenerateInitialEmTriangulation(toDoTriangles, aVertexStorage, aEmImage, aUseBilinearFiltering))
            return false;

        if (!RefineEmTriangulation(oTriangles, toDoTriangles, aVertexStorage,
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
        VertexStorage               &aVertexStorage,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        aEmImage;  aUseBilinearFiltering;

        // Generate the geometrical data
        Vec3f vertices[12];
        Vec3ui faces[20];
        Geom::UnitIcosahedron(vertices, faces);

        std::array<uint32_t, 12> vertexIndices;

        // Allocate shared vertices for the triangles
        for (uint32_t i = 0; i < Utils::ArrayLength(vertices); i++)
            CreateNewVertex(vertexIndices[i], aVertexStorage, vertices[i], aEmImage, aUseBilinearFiltering);

        // Build triangle set
        for (uint32_t i = 0; i < Utils::ArrayLength(faces); i++)
        {
            const auto &faceVertices = faces[i];

            PG3_ASSERT_INTEGER_IN_RANGE(faceVertices.Get(0), 0, Utils::ArrayLength(vertices) - 1);
            PG3_ASSERT_INTEGER_IN_RANGE(faceVertices.Get(1), 0, Utils::ArrayLength(vertices) - 1);
            PG3_ASSERT_INTEGER_IN_RANGE(faceVertices.Get(2), 0, Utils::ArrayLength(vertices) - 1);

            oTriangles.push_back(new TriangleNode(
                vertexIndices[faceVertices.Get(0)],
                vertexIndices[faceVertices.Get(1)],
                vertexIndices[faceVertices.Get(2)],
                aVertexStorage,
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

        do {
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
        std::list<TreeNodeBase*>    &aTriangles,
        VertexStorage               &aVertexStorage,
        uint32_t                     aTriangleCount)
    {
        Rng rng;
        for (uint32_t triangleIdx = 0; triangleIdx < aTriangleCount; triangleIdx++)
        {
            Vec3f vertexCoords[3];
            GenerateRandomTriangleVertices(rng, vertexCoords);

            float vertexLuminances[3] {
                static_cast<float>(triangleIdx),
                static_cast<float>(triangleIdx) + 0.3f,
                static_cast<float>(triangleIdx) + 0.6f
            };

            std::array<uint32_t, 3> vertexIndices;
            CreateNewVertex(vertexIndices[0], aVertexStorage, vertexCoords[0], vertexLuminances[0]);
            CreateNewVertex(vertexIndices[1], aVertexStorage, vertexCoords[1], vertexLuminances[1]);
            CreateNewVertex(vertexIndices[2], aVertexStorage, vertexCoords[2], vertexLuminances[2]);

            auto triangle = new TriangleNode(
                vertexIndices[0],
                vertexIndices[1],
                vertexIndices[2],
                aVertexStorage,
                triangleIdx);
            aTriangles.push_back(triangle);
        }
    }

    static void CreateNewVertex(
        uint32_t                    &oVertexIndex,
        VertexStorage               &aVertexStorage,
        const Vec3f                 &aVertexDir,
        const float                  aLuminance)
    {
        SteeringBasisValue weight;
        weight.GenerateSphHarm(aVertexDir, aLuminance);

        aVertexStorage.AddVertex(Vertex(aVertexDir, weight), oVertexIndex);
    }

    static void CreateNewVertex(
        uint32_t                    &oVertexIndex,
        VertexStorage               &aVertexStorage,
        const Vec3f                 &aVertexDir,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        const auto radiance = aEmImage.Evaluate(aVertexDir, aUseBilinearFiltering);
        const auto luminance = radiance.Luminance();

        CreateNewVertex(oVertexIndex, aVertexStorage, aVertexDir, luminance);
    }

    // Sub-divides the "to do" triangle set of triangles according to the refinement rule and
    // fills the output list of triangles. The refined triangles are released. The triangles 
    // are either moved from the "to do" set into the output list or deleted on error.
    // Although the "to do" triangle set is a TreeNodeBase* container, it must contain 
    // TriangleNode* data only, otherwise an error will occur.
    template <class TTriangulationStats>
    static bool RefineEmTriangulation(
        std::list<TreeNodeBase*>    &oRefinedTriangles,
        std::deque<TriangleNode*>   &aToDoTriangles,
        VertexStorage               &aVertexStorage,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const BuildParameters       &aParams,
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
                    *currentTriangle, aVertexStorage, aEmImage, aUseBilinearFiltering, aParams, aStats))
            {
                // Replace the triangle with sub-division triangles
                std::list<TriangleNode*> subdivisionTriangles;
                SubdivideTriangle(subdivisionTriangles, *currentTriangle, aVertexStorage, aEmImage, aUseBilinearFiltering);
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
        const Vec3f                 &aVertex0,
        const Vec3f                 &aVertex1,
        const Vec3f                 &aVertex2,
        const Vec2ui                &aEmSize,
        const Vec3f                 &aPlanarTriangleCentroid,
        const float                  aMinSinClamped,
        const float                  aMaxSinClamped,
        float                       &aMinSamplesPerDimF,
        float                       &aMaxSamplesPerDimF,
        const BuildParameters       &aParams)
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
        VertexStorage               &aVertexStorage,
        const Vec3f                 &aSubVertex0,
        const Vec3f                 &aSubVertex1,
        const Vec3f                 &aSubVertex2,
        uint32_t                     samplesPerDim,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const BuildParameters       &aParams,
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
                    aVertexStorage.Get(aWholeTriangle.vertexIndices[0])->dir,
                    aVertexStorage.Get(aWholeTriangle.vertexIndices[1])->dir,
                    aVertexStorage.Get(aWholeTriangle.vertexIndices[2])->dir,
                    0.1f);

                // Evaluate
                const Vec2f wholeTriangleSampleBaryCrop = {
                    Math::Clamp(wholeTriangleSampleBary.x, 0.f, 1.f),
                    Math::Clamp(wholeTriangleSampleBary.y, 0.f, 1.f),
                };
                const auto approxVal = aWholeTriangle.EvaluateLuminanceApprox(
                    wholeTriangleSampleBaryCrop, aVertexStorage, aEmImage, aUseBilinearFiltering);
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
        VertexStorage               &aVertexStorage,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const BuildParameters       &aParams,
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
                    aWholeTriangle, aVertexStorage, aEmImage, aUseBilinearFiltering,
                    aParams, aStats))
                return true;

            // Check sub-triangle near vertex 1
            if (TriangleHasToBeSubdividedImpl(
                    aVertex1, aVertex1Sin,
                    edgeCentre12Dir, edgeCentre12Sin,
                    edgeCentre01Dir, edgeCentre01Sin,
                    aWholeTriangle, aVertexStorage, aEmImage, aUseBilinearFiltering,
                    aParams, aStats))
                return true;

            // Check sub-triangle near vertex 2
            if (TriangleHasToBeSubdividedImpl(
                    aVertex2, aVertex2Sin,
                    edgeCentre20Dir, edgeCentre20Sin,
                    edgeCentre12Dir, edgeCentre12Sin,
                    aWholeTriangle, aVertexStorage, aEmImage, aUseBilinearFiltering,
                    aParams, aStats))
                return true;

            // Check center sub-triangle
            if (TriangleHasToBeSubdividedImpl(
                    edgeCentre01Dir, edgeCentre01Sin,
                    edgeCentre12Dir, edgeCentre12Sin,
                    edgeCentre20Dir, edgeCentre20Sin,
                    aWholeTriangle, aVertexStorage, aEmImage, aUseBilinearFiltering,
                    aParams, aStats))
                return true;

            return false;
        }

        // Sample and check error
        const bool result = IsEstimationErrorTooLarge(
            aWholeTriangle, aVertexStorage, aVertex0, aVertex1, aVertex2,
            (uint32_t)std::ceil(maxSamplesPerDimF),
            aEmImage, aUseBilinearFiltering, aParams, aStats);

        return result;
    }

    template <class TTriangulationStats>
    static bool TriangleHasToBeSubdivided(
        const TriangleNode          &aTriangle,
        VertexStorage               &aVertexStorage,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering,
        const BuildParameters       &aParams,
        TTriangulationStats         &aStats)
    {
        // TODO: Build triangle count/size limit into the sub-division criterion (if too small, stop)
        if (aTriangle.subdivLevel >= aParams.GetMaxSubdivLevel())
            return false;

        const auto &dir0 = aVertexStorage.Get(aTriangle.vertexIndices[0])->dir;
        const auto &dir1 = aVertexStorage.Get(aTriangle.vertexIndices[1])->dir;
        const auto &dir2 = aVertexStorage.Get(aTriangle.vertexIndices[2])->dir;

        const float vertex0Sin = std::sqrt(1.f - Math::Sqr(dir0.z));
        const float vertex1Sin = std::sqrt(1.f - Math::Sqr(dir1.z));
        const float vertex2Sin = std::sqrt(1.f - Math::Sqr(dir2.z));

        bool result = TriangleHasToBeSubdividedImpl(
            dir0, vertex0Sin,
            dir1, vertex1Sin,
            dir2, vertex2Sin,
            aTriangle, aVertexStorage, aEmImage, aUseBilinearFiltering,
            aParams, aStats);

        return result;
    }

    static void SubdivideTriangle(
        std::list<TriangleNode*>    &oSubdivisionTriangles,
        const TriangleNode          &aTriangle,
        VertexStorage               &aVertexStorage,
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
        const auto &dir0 = aVertexStorage.Get(aTriangle.vertexIndices[0])->dir;
        const auto &dir1 = aVertexStorage.Get(aTriangle.vertexIndices[1])->dir;
        const auto &dir2 = aVertexStorage.Get(aTriangle.vertexIndices[2])->dir;
        newVertexCoords[0] = ((dir0 + dir1) / 2.f).Normalize();
        newVertexCoords[1] = ((dir1 + dir2) / 2.f).Normalize();
        newVertexCoords[2] = ((dir2 + dir0) / 2.f).Normalize();

        // New shared vertices
        std::array<uint32_t, 3> newIndices;
        CreateNewVertex(newIndices[0], aVertexStorage, newVertexCoords[0], aEmImage, aUseBilinearFiltering);
        CreateNewVertex(newIndices[1], aVertexStorage, newVertexCoords[1], aEmImage, aUseBilinearFiltering);
        CreateNewVertex(newIndices[2], aVertexStorage, newVertexCoords[2], aEmImage, aUseBilinearFiltering);

        // Central triangle
        oSubdivisionTriangles.push_back(
            new TriangleNode(newIndices[0], newIndices[1], newIndices[2], aVertexStorage, 1, &aTriangle));

        // 3 corner triangles
        const auto &oldIndices = aTriangle.vertexIndices;
        oSubdivisionTriangles.push_back(
            new TriangleNode(oldIndices[0], newIndices[0], newIndices[2], aVertexStorage, 2, &aTriangle));
        oSubdivisionTriangles.push_back(
            new TriangleNode(newIndices[0], oldIndices[1], newIndices[1], aVertexStorage, 3, &aTriangle));
        oSubdivisionTriangles.push_back(
            new TriangleNode(newIndices[1], oldIndices[2], newIndices[2], aVertexStorage, 4, &aTriangle));

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

        VertexStorage vertexStorage;

        // Generate triangle with vertices
        std::array<uint32_t, 3> vertexIndices;
        CreateNewVertex(vertexIndices[0], vertexStorage, aTriangleCoords[0], aEmImage, aUseBilinearFiltering);
        CreateNewVertex(vertexIndices[1], vertexStorage, aTriangleCoords[1], aEmImage, aUseBilinearFiltering);
        CreateNewVertex(vertexIndices[2], vertexStorage, aTriangleCoords[2], aEmImage, aUseBilinearFiltering);
        TriangleNode triangle(vertexIndices[0], vertexIndices[1], vertexIndices[2], vertexStorage, 0);

        // Subdivide
        std::list<TriangleNode*> subdivisionTriangles;
        SubdivideTriangle(subdivisionTriangles, triangle, vertexStorage, aEmImage, aUseBilinearFiltering);

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
        const auto triangleNormal = triangle.ComputeNormal(vertexStorage);
        for (auto subdividedTriange : subdivisionTriangles)
        {
            const auto subdivNormal = subdividedTriange->ComputeNormal(vertexStorage);
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
        {
            const auto &dir0 = vertexStorage.Get((**itSubdivs).vertexIndices[0])->dir;
            const auto &dir1 = vertexStorage.Get((**itSubdivs).vertexIndices[1])->dir;
            const auto &dir2 = vertexStorage.Get((**itSubdivs).vertexIndices[2])->dir;
            if (   !dir0.EqualsDelta(aSubdivisionPoints[0], 0.0001f)
                || !dir1.EqualsDelta(aSubdivisionPoints[1], 0.0001f)
                || !dir2.EqualsDelta(aSubdivisionPoints[2], 0.0001f))
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex positions",
                    "Central subdivision triangle has at least one incorrectly positioned vertex");
                return false;
            }
        }
        itSubdivs++;

        // Corner triangle 1
        {
            const auto &dir0 = vertexStorage.Get((**itSubdivs).vertexIndices[0])->dir;
            const auto &dir1 = vertexStorage.Get((**itSubdivs).vertexIndices[1])->dir;
            const auto &dir2 = vertexStorage.Get((**itSubdivs).vertexIndices[2])->dir;
            if (   !dir0.EqualsDelta(aTriangleCoords[0],    0.0001f)
                || !dir1.EqualsDelta(aSubdivisionPoints[0], 0.0001f)
                || !dir2.EqualsDelta(aSubdivisionPoints[2], 0.0001f))
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex positions",
                    "Corner 1 subdivision triangle has at least one incorrectly positioned vertex");
                return false;
            }
        }
        itSubdivs++;

        // Corner triangle 2
        {
            const auto &dir0 = vertexStorage.Get((**itSubdivs).vertexIndices[0])->dir;
            const auto &dir1 = vertexStorage.Get((**itSubdivs).vertexIndices[1])->dir;
            const auto &dir2 = vertexStorage.Get((**itSubdivs).vertexIndices[2])->dir;
            if (   !dir0.EqualsDelta(aSubdivisionPoints[0], 0.0001f)
                || !dir1.EqualsDelta(aTriangleCoords[1],    0.0001f)
                || !dir2.EqualsDelta(aSubdivisionPoints[1], 0.0001f))
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex positions",
                    "Corner 2 subdivision triangle has at least one incorrectly positioned vertex");
                return false;
            }
        }
        itSubdivs++;

        // Corner triangle 3
        {
            const auto &dir0 = vertexStorage.Get((**itSubdivs).vertexIndices[0])->dir;
            const auto &dir1 = vertexStorage.Get((**itSubdivs).vertexIndices[1])->dir;
            const auto &dir2 = vertexStorage.Get((**itSubdivs).vertexIndices[2])->dir;
            if (   !dir0.EqualsDelta(aSubdivisionPoints[1], 0.0001f)
                || !dir1.EqualsDelta(aTriangleCoords[2],    0.0001f)
                || !dir2.EqualsDelta(aSubdivisionPoints[2], 0.0001f))
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Vertex positions",
                    "Corner 3 subdivision triangle has at least one incorrectly positioned vertex");
                return false;
            }
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
    // The triangles are either moved into the tree or deleted on error.
    static bool BuildTriangleTree(
        std::list<TreeNodeBase*>        &aNodes,
        std::unique_ptr<TreeNodeBase>   &oTreeRoot)
    {
        oTreeRoot.reset(nullptr);

        // TODO: Switch to a more efficient container? (e.g. deque - less allocations?)

        // Process in layers from bottom to top.
        // If the current layer has odd element count, the last element can be merged with 
        // the first element of the next layer. This does not increase the height of the tree,
        // but can lead to worse memory access pattern (a triangle subset from the one end 
        // is merged with a subset from the other end of list).
        // TODO: Move the last element of an odd list to the end of the next layer?
        while (aNodes.size() >= 2)
        {
            const auto node1 = aNodes.front(); aNodes.pop_front();
            const auto node2 = aNodes.front(); aNodes.pop_front();

            PG3_ASSERT(node1 != nullptr);
            PG3_ASSERT(node2 != nullptr);

            auto newNode = new TriangleSetNode(node1, node2);
            if (newNode == nullptr)
            {
                delete node1;
                delete node2;
                FreeNodesList(aNodes);
                return false;
            }
            aNodes.push_back(newNode);
        }

        PG3_ASSERT(aNodes.size() <= 1u);

        // Fill tree root
        if (aNodes.size() == 1u)
        {
            oTreeRoot.reset(aNodes.front());
            aNodes.pop_front();
        }

        PG3_ASSERT(aNodes.empty());

        return true;
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

    // Contains all used vertices.
    // Referenced from mTreeRoot through indices
    VertexStorage                   mVertexStorage;

    // Sampling tree. Leaves represent triangles, inner nodes represent sets of triangles.
    // Triangles reference vertices in mVertexStorage through indices.
    std::unique_ptr<TreeNodeBase>   mTreeRoot;

public:

#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

    static bool _UT_InitialTriangulation(
        std::deque<TriangleNode*>   &oTriangles,
        VertexStorage               &aVertexStorage,
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation");

        if (!GenerateInitialEmTriangulation(oTriangles, aVertexStorage, aEmImage, aUseBilinearFiltering))
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
            std::array<Vertex*, 3> vertices = {
                aVertexStorage.Get(triangle->vertexIndices[0]),
                aVertexStorage.Get(triangle->vertexIndices[1]),
                aVertexStorage.Get(triangle->vertexIndices[2]) };

            // Each triangle is unique
            {
                std::set<Vertex*> vertexSet = { vertices[0], vertices[1], vertices[2] };
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
                const auto vertex0 = vertices[0];
                const auto vertex1 = vertices[1];
                const auto vertex2 = vertices[2];
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
                    const auto vertex     = vertices[vertexSeqNum];
                    const auto vertexNext = vertices[(vertexSeqNum + 1) % 3];

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

                    //// Each vertex must be shared by exactly 5 owning triangles
                    //auto useCount = vertices[vertexSeqNum].use_count();
                    //if (useCount != 5)
                    //{
                    //    std::ostringstream errorDescription;
                    //    errorDescription << "The vertex ";
                    //    errorDescription << vertexSeqNum;
                    //    errorDescription << " is shared by ";
                    //    errorDescription << useCount;
                    //    errorDescription << " owners instead of 5!";
                    //    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Initial triangulation",
                    //        errorDescription.str().c_str());
                    //    return false;
                    //}

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
        std::list<TreeNodeBase*>    &oRefinedTriangles,
        std::deque<TriangleNode*>   &aInitialTriangles,
        VertexStorage               &aVertexStorage,
        BuildParameters             &aParams,
        uint32_t                     aExpectedRefinedCount,
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        TTriangulationStats         &aStats,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement");

        if (!RefineEmTriangulation(
                oRefinedTriangles, aInitialTriangles, aVertexStorage,
                aEmImage, aUseBilinearFiltering, aParams, aStats))
        {
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                "RefineEmTriangulation() failed!");
            FreeNodesList(oRefinedTriangles);
            return false;
        }

        // Triangles count (optional)
        if ((aExpectedRefinedCount > 0) && (oRefinedTriangles.size() != aExpectedRefinedCount))
        {
            std::ostringstream errorDescription;
            errorDescription << "Initial triangle count is ";
            errorDescription << oRefinedTriangles.size();
            errorDescription << " instead of ";
            errorDescription << aExpectedRefinedCount;
            errorDescription << "!";
            PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                errorDescription.str().c_str());
            FreeNodesList(oRefinedTriangles);
            return false;
        }

        // All vertices lie on unit sphere
        for (auto node : oRefinedTriangles)
        {
            auto triangle = static_cast<EnvironmentMapSteeringSampler::TriangleNode*>(node);
            const auto &dir0 = aVertexStorage.Get(triangle->vertexIndices[0])->dir;
            const auto &dir1 = aVertexStorage.Get(triangle->vertexIndices[1])->dir;
            const auto &dir2 = aVertexStorage.Get(triangle->vertexIndices[2])->dir;
            if (   !Math::EqualDelta(dir0.LenSqr(), 1.0f, 0.001f)
                || !Math::EqualDelta(dir1.LenSqr(), 1.0f, 0.001f)
                || !Math::EqualDelta(dir2.LenSqr(), 1.0f, 0.001f))
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                    "Triangulation contains a vertex not lying on the unit sphere");
                FreeNodesList(oRefinedTriangles);
                return false;
            }
        }

        // Non-zero triangle size
        for (auto node : oRefinedTriangles)
        {
            auto triangle = static_cast<EnvironmentMapSteeringSampler::TriangleNode*>(node);
            auto surfaceArea = triangle->ComputeSurfaceArea(aVertexStorage);
            if (surfaceArea < 0.0001f)
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                    "Triangulation contains a degenerated triangle");
                FreeNodesList(oRefinedTriangles);
                return false;
            }
        }

        // Sanity check for normals
        for (auto node : oRefinedTriangles)
        {
            const auto triangle = static_cast<EnvironmentMapSteeringSampler::TriangleNode*>(node);
            const auto centroid = triangle->ComputeCentroid(aVertexStorage);
            const auto centroidDirection = Normalize(centroid);
            const auto normal = triangle->ComputeNormal(aVertexStorage);
            const auto dot = Dot(centroidDirection, normal);
            if (dot < 0.0f)
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                    "A triangle normal is oriented inside the sphere");
                FreeNodesList(oRefinedTriangles);
                return false;
            }
        }

        // Weights
        for (auto node : oRefinedTriangles)
        {
            const auto triangle = static_cast<EnvironmentMapSteeringSampler::TriangleNode*>(node);

            // Vertex weights
            for (auto vertexIndex : triangle->vertexIndices)
            {
                auto vertex = aVertexStorage.Get(vertexIndex);

                const auto radiance = aEmImage.Evaluate(vertex->dir, aUseBilinearFiltering);
                const auto luminance = radiance.Luminance();
                const auto referenceWeight =
                    SteeringBasisValue().GenerateSphHarm(vertex->dir, luminance);
                if (vertex->weight != referenceWeight)
                {
                    PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                        "Incorect triangle vertex weight");
                    FreeNodesList(oRefinedTriangles);
                    return false;
                }
            }

            // Triangle weight
            const auto area = triangle->ComputeSurfaceArea(aVertexStorage);
            const SteeringBasisValue referenceWeight =
                area * (aVertexStorage.Get(triangle->vertexIndices[0])->weight
                      + aVertexStorage.Get(triangle->vertexIndices[1])->weight
                      + aVertexStorage.Get(triangle->vertexIndices[2])->weight) / 3.0f;
            if (!referenceWeight.EqualsDelta(triangle->GetWeight(), 0.0001f))
            {
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement",
                    "Incorect triangle weight");
                FreeNodesList(oRefinedTriangles);
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
                FreeNodesList(oRefinedTriangles);
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
                    FreeNodesList(oRefinedTriangles);
                    return false;
                }
            }
        }

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement");

        return true;
    }


    static bool _UT_Init_SingleEm(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        char                        *aTestName,
        uint32_t                     aMaxSubdivLevel,
        uint32_t                     aExpectedRefinedCount,
        bool                         aCheckSamplingCoverage,
        char                        *aImagePath,
        bool                         aUseBilinearFiltering)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", aTestName);

        // Load image
        std::unique_ptr<EnvironmentMapImage> image(EnvironmentMapImage::LoadImage(aImagePath));
        if (!image)
        {
            PG3_UT_FATAL_ERROR(aMaxUtBlockPrintLevel, eutblSubTestLevel1,
                               "%s", "Unable to load image!", aTestName);
            return false;
        }

        VertexStorage vertexStorage;
        std::deque<TriangleNode*> initialTriangles;
        std::list<TreeNodeBase*> refinedTriangles;
        std::unique_ptr<TreeNodeBase> treeRoot;

        BuildParameters params(Math::InfinityF(), static_cast<float>(aMaxSubdivLevel));

        // Initial triangulation
        if (!_UT_InitialTriangulation(
                initialTriangles,
                vertexStorage,
                aMaxUtBlockPrintLevel,
                *image.get(),
                aUseBilinearFiltering))
        {
            FreeTrianglesDeque(initialTriangles);
            return false;
        }

        // Triangulation refinement
        bool refinePassed;
        if (aCheckSamplingCoverage)
        {
            TriangulationStats stats(*image.get());
            refinePassed = _UT_RefineTriangulation(
                refinedTriangles,
                initialTriangles,
                vertexStorage,
                params,
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
                refinedTriangles,
                initialTriangles,
                vertexStorage,
                params,
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

        // Build tree
        if (!_UT_BuildTriangleTree_SingleList(
                aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Build tree",
                refinedTriangles, vertexStorage, treeRoot))
            return false;

        // Save/Load
        if (!_UT_SaveToAndLoadFromDisk(
                aMaxUtBlockPrintLevel, eutblSubTestLevel2,
                vertexStorage, treeRoot, *image.get(), aUseBilinearFiltering, params))
            return false;

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%s", aTestName);

        return true;
    }

    static bool _UT_InspectTree(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const UnitTestBlockLevel     aUtBlockPrintLevel,
        const char                  *aTestName,
        const TreeNodeBase          *aCurrentNode,
        VertexStorage               &aVertexStorage,
        uint32_t                    &oLeafCount,
        uint32_t                    &oMaxDepth,
        uint32_t                     aCurrentDepth = 1)
    {
        if (aCurrentNode == nullptr)
            return true; // Accept an empty tree

        if (!aCurrentNode->IsTriangleNode()) // Inner node
        {
            auto innerNode  = static_cast<const TriangleSetNode*>(aCurrentNode);
            auto leftChild  = innerNode->GetLeftChild();
            auto rightChild = innerNode->GetRightChild();

            // Null pointers
            if ((leftChild == nullptr) || (rightChild == nullptr))
            {
                PG3_UT_END_FAILED(
                    aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s",
                    "Found null child node!", aTestName);
                return false;
            }

            // Check children recursively
            if (   !_UT_InspectTree(
                        aMaxUtBlockPrintLevel, aUtBlockPrintLevel, aTestName,
                        leftChild, aVertexStorage, oLeafCount, oMaxDepth, aCurrentDepth + 1)
                || !_UT_InspectTree(
                        aMaxUtBlockPrintLevel, aUtBlockPrintLevel, aTestName,
                        rightChild, aVertexStorage, oLeafCount, oMaxDepth, aCurrentDepth + 1))
            {
                return false;
            }

            // Weight validity
            const auto innerNodeWeight = innerNode->GetWeight();
            if (!innerNodeWeight.IsValid())
            {
                PG3_UT_END_FAILED(
                    aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s",
                    "Found invalid inner node weight!", aTestName);
                return false;
            }

            // Weight consistency
            const auto leftChildWeight   = leftChild->GetWeight();
            const auto rightChildWeight  = rightChild->GetWeight();
            const auto summedChildWeight = leftChildWeight + rightChildWeight;
            if (innerNodeWeight != summedChildWeight)
            {
                std::ostringstream errorDescription;
                errorDescription << "Node weight is not equal to the sum of child weights";
                PG3_UT_END_FAILED(
                    aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s",
                    errorDescription.str().c_str(), aTestName);
                return false;
            }
        }
        else // Triangle node
        {
            oLeafCount++;
            oMaxDepth = std::max(oMaxDepth, aCurrentDepth);

            auto triangleNode = static_cast<const TriangleNode*>(aCurrentNode);
            for (auto vertexIndex : triangleNode->vertexIndices)
            {
                auto vertex = aVertexStorage.Get(vertexIndex);

                if (vertex == nullptr)
                {
                    PG3_UT_END_FAILED(
                        aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s",
                        "Found null triangle vertex!", aTestName);
                    return false;
                }

                // Normalized direction
                if (!Math::EqualDelta(vertex->dir.LenSqr(), 1.0f, 0.001f))
                {
                    PG3_UT_END_FAILED(
                        aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s",
                        "Found invalid direction!", aTestName);
                    return false;
                }

                // Weight validity
                if (!vertex->weight.IsValid())
                {
                    PG3_UT_END_FAILED(
                        aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s",
                        "Found invalid weight!", aTestName);
                    return false;
                }
            }
        }

        return true;
    }

    static bool _UT_BuildTriangleTree_SingleList(
        const UnitTestBlockLevel         aMaxUtBlockPrintLevel,
        const UnitTestBlockLevel         aUtBlockPrintLevel,
        const char                      *aTestName,
        std::list<TreeNodeBase*>        &aTriangles,
        VertexStorage                   &aVertexStorage,
        std::unique_ptr<TreeNodeBase>   &oTreeRoot)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s", aTestName);

        const auto initialListSize = aTriangles.size();

        if (!BuildTriangleTree(aTriangles, oTreeRoot))
        {
            PG3_UT_END_FAILED(
                aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s",
                "BuildTriangleTree() failed!", aTestName);
            return false;
        }

        // Analyze tree
        uint32_t leafCount = 0;
        uint32_t maxDepth = 0;
        if (!_UT_InspectTree(
                aMaxUtBlockPrintLevel, aUtBlockPrintLevel, aTestName,
                oTreeRoot.get(), aVertexStorage, leafCount, maxDepth))
            return false;
        
        // Leaf count
        if (leafCount != initialListSize)
        {
            PG3_UT_END_FAILED(
                aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s",
                "Leaf count doesn't equal to triangle count!", aTestName);
            return false;
        }

        // Max depth
        const auto expectedMaxDepth =
            (initialListSize == 0) ?
            0 : static_cast<uint32_t>(std::ceil(std::log2((float)initialListSize))) + 1u;
        if (maxDepth != expectedMaxDepth)
        {
            std::ostringstream errorDescription;
            errorDescription << "Max depth ";
            errorDescription << maxDepth;
            errorDescription << " doesn't equal to expected (log) depth ";
            errorDescription << expectedMaxDepth;
            PG3_UT_END_FAILED(
                aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s",
                errorDescription.str().c_str(), aTestName);
            return false;
        }

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "%s", aTestName);
        return true;
    }


    static bool _UT_BuildTriangleTree_SingleRandomList(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const UnitTestBlockLevel     aUtBlockPrintLevel,
        uint32_t                     aTriangleCount)
    {
        VertexStorage vertexStorage;
        std::list<TreeNodeBase*> triangles;
        std::unique_ptr<TreeNodeBase> treeRoot;

        GenerateRandomTriangulation(triangles, vertexStorage, aTriangleCount);

        std::ostringstream testName;
        testName << "Random triangle list (";
        testName << aTriangleCount;
        testName << " items)";

        return _UT_BuildTriangleTree_SingleList(
            aMaxUtBlockPrintLevel, aUtBlockPrintLevel, testName.str().c_str(),
            triangles, vertexStorage, treeRoot);
    }


    static bool _UT_BuildTriangleTreeSynthetic(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(
            aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::BuildTriangleTree() - Synthetic");

        for (uint32_t i = 0; i < 9; i++)
            if (!_UT_BuildTriangleTree_SingleRandomList(aMaxUtBlockPrintLevel, eutblSubTestLevel1, i))
                return false;

        if (!_UT_BuildTriangleTree_SingleRandomList(aMaxUtBlockPrintLevel, eutblSubTestLevel1, 10))
            return false;
        if (!_UT_BuildTriangleTree_SingleRandomList(aMaxUtBlockPrintLevel, eutblSubTestLevel1, 100))
            return false;
        if (!_UT_BuildTriangleTree_SingleRandomList(aMaxUtBlockPrintLevel, eutblSubTestLevel1, 1000))
            return false;
        if (!_UT_BuildTriangleTree_SingleRandomList(aMaxUtBlockPrintLevel, eutblSubTestLevel1, 10000))
            return false;
        if (!_UT_BuildTriangleTree_SingleRandomList(aMaxUtBlockPrintLevel, eutblSubTestLevel1, 100000))
            return false;
        if (!_UT_BuildTriangleTree_SingleRandomList(aMaxUtBlockPrintLevel, eutblSubTestLevel1, 1000000))
            return false;

        PG3_UT_END_PASSED(
            aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::BuildTriangleTree() - Synthetic");
        return true;
    }


    static bool _UT_SaveToAndLoadFromDisk(
        const UnitTestBlockLevel         aMaxUtBlockPrintLevel,
        const UnitTestBlockLevel         aUtBlockPrintLevel,
        VertexStorage                   &aVertexStorage,
        std::unique_ptr<TreeNodeBase>   &aTreeRoot,
        const EnvironmentMapImage       &aEmImage,
        bool                             aUseBilinearFiltering,
        BuildParameters                 &aParams)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "SaveToDisk10 and LoadFromDisk10");

        const bool isDebugging = false; // makes the file more human readable (but machine un-readable!)

        // Save
        if (!SaveToDisk10(
                aVertexStorage, aTreeRoot.get(), aEmImage, aUseBilinearFiltering,
                aParams, isDebugging))
        {
            PG3_UT_END_FAILED(
                aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "SaveToDisk10 and LoadFromDisk10",
                "SaveToDisk10() failed!");
            return false;
        }

        // Load
        VertexStorage                   loadedVertexStorage;
        std::unique_ptr<TreeNodeBase>   loadedTreeRoot;
        if (!LoadFromDisk10(
                loadedVertexStorage, loadedTreeRoot, aEmImage, aUseBilinearFiltering, aParams))
        {
            PG3_UT_END_FAILED(
                aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "SaveToDisk10 and LoadFromDisk10",
                "LoadFromDisk10() failed!");
            return false;
        }

        // Compare vertices
        if (aVertexStorage != loadedVertexStorage)
        {
            PG3_UT_END_FAILED(
                aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "SaveToDisk10 and LoadFromDisk10",
                "Loaded vertex storage differs from the saved one!");
            return false;
        }

        if (!loadedTreeRoot)
        {
            PG3_UT_END_FAILED(
                aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "SaveToDisk10 and LoadFromDisk10",
                "Loaded tree is empty!");
            return false;
        }

        // Compare with the original tree
        if (*aTreeRoot != *loadedTreeRoot)
        {
            PG3_UT_END_FAILED(
                aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "SaveToDisk10 and LoadFromDisk10",
                "Loaded tree differs from the saved one!");
            return false;
        }

        // TODO:
        // - Sanity tests?...
        // - ...



        //// Leaf count
        //if (leafCount != initialListSize)
        //{
        //    PG3_UT_END_FAILED(
        //        aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "SaveToDisk10 and LoadFromDisk10",
        //        "Leaf count doesn't equal to triangle count!");
        //    return false;
        //}

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, aUtBlockPrintLevel, "SaveToDisk10 and LoadFromDisk10");
        return true;
    }


    static bool _UT_Init(const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(
            aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::Build()");

        // TODO: Empty EM
        // TODO: Black constant EM (Luminance 0)
        // TODO: ?

        if (!_UT_Init_SingleEm(
                aMaxUtBlockPrintLevel,
                "Const white 8x4", 5, 20, true,
                ".\\Light Probes\\Debugging\\Const white 8x4.exr",
                false))
            return false;

        //if (!_UT_Init_SingleEm(
        //        aMaxUtBlockPrintLevel,
        //        "Const white 512x256", 5, 20, true,
        //        ".\\Light Probes\\Debugging\\Const white 512x256.exr",
        //        false))
        //    return false;

        //if (!_UT_Init_SingleEm(
        //        aMaxUtBlockPrintLevel,
        //        "Const white 1024x512", 5, 20, true,
        //        ".\\Light Probes\\Debugging\\Const white 1024x512.exr",
        //        false))
        //    return false;

        //if (!_UT_Init_SingleEm(
        //        aMaxUtBlockPrintLevel,
        //        "Single pixel", 5, 0, false,
        //        ".\\Light Probes\\Debugging\\Single pixel.exr",
        //        false))
        //    return false;

        //if (!_UT_Init_SingleEm(
        //        aMaxUtBlockPrintLevel,
        //        "Three point lighting 1024x512", 5, 0, false,
        //        ".\\Light Probes\\Debugging\\Three point lighting 1024x512.exr",
        //        false))
        //    return false;

        //if (!_UT_Init_SingleEm(
        //        aMaxUtBlockPrintLevel,
        //        "Satellite 4000x2000", 5, 0, false,
        //        ".\\Light Probes\\hdr-sets.com\\HDR_SETS_SATELLITE_01_FREE\\107_ENV_DOMELIGHT.exr",
        //        false))
        //    return false;


        ///////////////

        //if (!_UT_Init_SingleEm(
        //        aMaxUtBlockPrintLevel,
        //        "Doge2", 5, 0, false,
        //        ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\doge2.exr",
        //        false))
        //    return false;

        //if (!_UT_Init_SingleEm(
        //        aMaxUtBlockPrintLevel,
        //        "Peace Garden", 5, 0, false,
        //        ".\\Light Probes\\panocapture.com\\PeaceGardens_Dusk.exr",
        //        false))
        //    return false;

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
        //_UT_BuildTriangleTreeSynthetic(aMaxUtBlockPrintLevel);
        _UT_Init(aMaxUtBlockPrintLevel);
    }
#endif
};
