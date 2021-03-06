#pragma once

#include "unit_testing.hxx"

#include <sstream>
#include <iomanip>


///////////////////////////////////////////////////////////////////////////////
// Various utility functions
///////////////////////////////////////////////////////////////////////////////
namespace Utils
{
    // Returns the length of an array
    template <typename T, size_t N>
    inline uint32_t ArrayLength(const T(&)[N])
    {
        return uint32_t(N);
    }

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

    void IntegerToHumanReadable(const uint64_t aInt, std::string &oResult)
    {
        std::ostringstream outStream;

        if (aInt < (uint64_t)1E3)
            outStream << aInt;
        else if (aInt < 1E6)
        {
            const auto tmp = aInt / (uint64_t)1E3;
            outStream << tmp << "K";
        }
        else if (aInt < (uint64_t)1E9)
        {
            const auto tmp = aInt / (uint64_t)1E6;
            outStream << tmp << "M";
        }
        else //if (aInt < (uint64_t)1E12)
        {
            const auto tmp = aInt / (uint64_t)1E9;
            outStream << tmp << "T";
        }

        oResult = outStream.str();
    }

#ifdef PG3_RUN_UNIT_TESTS_INSTEAD_OF_RENDERER

    bool _UT_IntegerToHumanReadable_SingleNumber(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const uint64_t               aValue,
        const char                  *aHumanReference)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel1,"%u", aValue);

        std::string result;
        IntegerToHumanReadable(aValue, result);
        if (result != aHumanReference)
        {
            std::ostringstream errorDescription;
            errorDescription << "Output is \"";
            errorDescription << result;
            errorDescription << "\" instead of \"";
            errorDescription << aHumanReference;
            errorDescription << "\"";

            PG3_UT_FAILED(
                aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%u",
                errorDescription.str().c_str(), aValue);
        }

        // debug
        //printf("%s\n", result.c_str());

        PG3_UT_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel1, "%u", aValue);
        return true;
    }

    bool _UT_IntegerToHumanReadable(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest, "Utils::IntegerToHumanReadable()");

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)0, "0"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E0, "1"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E1 - 1, "9"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E1,     "10"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E1 + 1, "11"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E2 - 1, "99"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E2,     "100"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E2 + 1, "101"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E3 - 1, "999"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E3,     "1K"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E3 + 1, "1K"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E4, "10K"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E5 - 1, "99K"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E5,     "100K"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E5 + 1, "100K"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E6 - 1, "999K"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E6,     "1M"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E6 + 1, "1M"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E7 - 1, "9M"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E7,     "10M"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E7 + 1, "10M"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E8 - 1, "99M"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E8,     "100M"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E8 + 1, "100M"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E9 - 1, "999M"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E9,     "1T"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E9 + 1, "1T"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E10 - 1, "9T"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E10,     "10T"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E10 + 1, "10T"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E11 - 1, "99T"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E11,     "100T"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E11 + 1, "100T"))
            return false;

        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E12 - 1, "999T"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E12,     "1000T"))
            return false;
        if (!_UT_IntegerToHumanReadable_SingleNumber(aMaxUtBlockPrintLevel, (uint64_t)1E12 + 1, "1000T"))
            return false;

        PG3_UT_PASSED(aMaxUtBlockPrintLevel, eutblWholeTest, "Utils::IntegerToHumanReadable()");
        return true;
    }

#endif

    void PrintHistogramTicks(
        const uint32_t  aCount,
        const uint32_t  aMaxCount,
        const uint32_t  aMaxTickCount,
        const char      aTickCharacter  = '.',
        const char      aEmptyCharacter = ' ',
        const char      aLimitCharacter = '|'
        )
    {
        const uint32_t tickCount =
            (aMaxCount <= aMaxTickCount) ?
            aCount : Math::RemapInterval(aCount, aMaxCount, aMaxTickCount);

        for (uint32_t tick = 0; tick <= aMaxTickCount; ++tick)
        {
            if (tick < tickCount)
                printf("%c", aTickCharacter);
            else if (tick == aMaxTickCount)
                printf("%c", aLimitCharacter);
            else
                printf("%c", aEmptyCharacter);
        }
    }

    namespace IO
    {
        bool GetFileName(const char *aPath, std::string &oFilenameWithExt)
        {
            //char drive[_MAX_DRIVE];
            //char dir[_MAX_DIR];
            char fname[_MAX_FNAME];
            char ext[_MAX_EXT];
            errno_t err;

            err = _splitpath_s(aPath, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT);
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

            oFilenameWithExt = fname;
            oFilenameWithExt += ext;
            return true;
        }


        bool GetDirAndFileName(const char *aPath, std::string &oDirPath, std::string &oFilenameWithExt)
        {
            char drive[_MAX_DRIVE];
            char dir[_MAX_DIR];
            char fname[_MAX_FNAME];
            char ext[_MAX_EXT];
            errno_t err;

            err = _splitpath_s(aPath, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT);
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

            oDirPath = drive;
            oDirPath += dir;

            oFilenameWithExt = fname;
            oFilenameWithExt += ext;

            return true;
        }


        template <typename T>
        static void WriteVariableToStream(
            std::ofstream       &aOfs,
            const T             &aValue,
            bool                 aDebugging = false)
        {
            if (aDebugging)
            {
                // debugging, textual version of data
                aOfs << typeid(aValue).name() << ", size " << sizeof(T) << ": ";
                aOfs.precision(20);
                aOfs << aValue;
                aOfs << std::endl;
            }
            else
                aOfs.write(reinterpret_cast<const char*>(&aValue), sizeof(T));
        }


        template <>
        static void WriteVariableToStream(
            std::ofstream       &aOfs,
            const bool          &aBoolValue,
            bool                 aDebugging)
        {
            const uint32_t uintVal = aBoolValue ? 1u : 0u;
            WriteVariableToStream(aOfs, uintVal, aDebugging);
        }


        static void WriteStringToStream(
            std::ofstream       &aOfs,
            const char          *aStrBuff,
            bool                 aDebugging = false)
        {
            if (aStrBuff == nullptr)
                return;

            const size_t buffLength = std::strlen(aStrBuff) + 1; // add trailing zero
            if (aDebugging)
            {
                // debugging, textual version of data
                aOfs << typeid(aStrBuff).name();
                aOfs << ", size " << buffLength << "*" << sizeof(char) << ": ";
                aOfs.precision(20);
                aOfs << "\"" << aStrBuff << "\"";
                aOfs << std::endl;
            }
            else
                aOfs.write(reinterpret_cast<const char*>(aStrBuff), buffLength * sizeof(char));
        }


        template <typename T>
        static bool LoadVariableFromStream(
            std::ifstream       &aIfs,
            T                   &aValue)
        {
            aIfs.read(reinterpret_cast<char*>(&aValue), sizeof(T));

            bool retVal = !aIfs.fail();
            return retVal;
        }


        template <>
        static bool LoadVariableFromStream(
            std::ifstream       &aIfs,
            bool                &aBoolValue)
        {
            uint32_t uintValue;
            if (!LoadVariableFromStream(aIfs, uintValue))
                return false;

            aBoolValue = uintValue != 0u;
            return true;
        }


        static bool LoadStringFromStream(
            std::ifstream       &aIfs,
            char                *aStrBuff,
            const size_t         aCharCount)
        {
            if (aStrBuff == nullptr)
                return false;
            if (aCharCount == 0)
                return false;

            aIfs.read(reinterpret_cast<char*>(aStrBuff), aCharCount * sizeof(char));

            if (aIfs.fail())
                return false;
            if (aStrBuff[aCharCount - 1] != '\0')
                return false;

            return true;
        }
    }

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
