/*
    pbrt source code Copyright(c) 1998-2012 Matt Pharr and Greg Humphreys.

    This file is part of pbrt.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#pragma once

#include "math.hxx"
#include "types.hxx"

#include <algorithm>
#include <vector>

// Representation of a probability density function over the interval [0,1]
class Distribution1D 
{
public:
    Distribution1D(const float * const aFunc, uint32_t aCount) :
        mCount(aCount)
    {
        PG3_ASSERT(aCount > 0);

        mPdf = new float[mCount];
        mCdf = new float[mCount+1];
        
        // Compute integral of step function at $x_i$.
        // Note that the whole integral spans over the interval [0,1].
        mCdf[0] = 0.;
        for (uint32_t i = 1; i < mCount + 1; ++i)
        {
            PG3_ASSERT_FLOAT_NONNEGATIVE(aFunc[i - 1]);
            mCdf[i] = mCdf[i - 1] + aFunc[i - 1] / mCount; // 1/mCount = size of a segment
        }

        // Transform step function integral into CDF
        mFuncIntegral = mCdf[mCount];
        if (mFuncIntegral == 0.f) 
        {
            // The function is zero; use uniform PDF as a fallback
            for (uint32_t i = 1; i < mCount + 1; ++i)
            {
                mPdf[i-1] = 1.0f / mCount;
                mCdf[i]   = (float)i / mCount;
            }
        }
        else 
        {
            for (uint32_t i = 1; i < mCount + 1; ++i)
            {
                mPdf[i - 1]  = aFunc[i - 1] / mFuncIntegral;
                mCdf[i]     /= mFuncIntegral;
            }
        }
        PG3_ASSERT_FLOAT_EQUAL(mCdf[mCount], 1.0f, 1e-7F);
    }

    ~Distribution1D()
    {
        delete[] mPdf;
        delete[] mCdf;
    }

    PG3_PROFILING_NOINLINE
    void SampleContinuous(const float aRndSample, float &oX, uint32_t &oSegm, float &oPdf) const 
    {
        // Find surrounding CDF segments and _offset_
        float *ptr = std::upper_bound(mCdf, mCdf+mCount+1, aRndSample);
        oSegm = Math::Clamp(uint32_t(ptr-mCdf-1), 0u, mCount - 1u);

        PG3_ASSERT_INTEGER_IN_RANGE(oSegm, 0, mCount - 1);
        PG3_ASSERT(aRndSample >= mCdf[oSegm] && (aRndSample < mCdf[oSegm+1] || aRndSample == 1.0f));

        // Fix the case when func ends with zeros
        if (mCdf[oSegm] == mCdf[oSegm + 1])
        {
            PG3_ASSERT(aRndSample == 1.0f);

            do { 
                oSegm--; 
            } while (mCdf[oSegm] == mCdf[oSegm + 1] && oSegm > 0);

            PG3_ASSERT(mCdf[oSegm] != mCdf[oSegm + 1]);
        }

        // Compute offset within CDF segment
        const float offset = (aRndSample - mCdf[oSegm]) / (mCdf[oSegm+1] - mCdf[oSegm]);
        PG3_ASSERT_FLOAT_VALID(offset);
        PG3_ASSERT_FLOAT_IN_RANGE(offset, 0.0f, 1.0f);

        // since the float random generator generates 1.0 from time to time, we need to ignore this one
        //PG3_ASSERT_FLOAT_LESS_THAN(offset, 1.0f);

        // Get the segment's constant PDF
        oPdf = mPdf[oSegm]; // TODO: Use cdf for pdf computations? May save non-local memory accesses
        PG3_ASSERT(mPdf[oSegm] > 0);

        // Return $x\in{}[0,1]$ corresponding to sample
        oX = (oSegm + offset) / mCount;
    }

    //int32_t SampleDiscrete(float aRndSample, float *oProbability) const
    //{
    //    // Find surrounding CDF segments and _offset_
    //    float *ptr = std::upper_bound(mCdf, mCdf+mCount+1, aRndSample);
    //    int32_t segment = std::max(0, int32_t(ptr-mCdf-1));

    //    PG3_ASSERT(segment < mCount);
    //    PG3_ASSERT(aRndSample >= mCdf[segment] && aRndSample < mCdf[segment+1]);

    //    if (oProbability)
    //        // pdf * size of segment
    //        *oProbability = mPdf[segment] / mCount;
    //    return segment;
    //}

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator
    // explicitly, the compiler may complain about not being able 
    // to create its default implementation.
    Distribution1D & operator=(const Distribution1D&) = delete;
    //Distribution1D(const Distribution1D&) = delete;

private:
    friend class Distribution2D;

    float           *mPdf;
    float           *mCdf;
    float            mFuncIntegral;
    const uint32_t   mCount;
};

class Distribution2D
{
public:
    Distribution2D(const float *aFunc, int32_t sCountU, int32_t sCountV)
    {
        mConditionalV.reserve(sCountV);
        for (int32_t v = 0; v < sCountV; ++v) 
        {
            // Compute conditional sampling distribution for $\tilde{v}$
            mConditionalV.push_back(new Distribution1D(&aFunc[v*sCountU], sCountU));
        }

        // Compute marginal sampling distribution $p[\tilde{v}]$
        std::vector<float> marginalFunc;
        marginalFunc.reserve(sCountV);
        for (int32_t v = 0; v < sCountV; ++v)
            marginalFunc.push_back(mConditionalV[v]->mFuncIntegral);
        mMarginal = new Distribution1D(&marginalFunc[0], sCountV);
    }

    ~Distribution2D()
    {
        delete mMarginal;
        for (auto& conditional : mConditionalV)
            delete conditional;
    }

    void SampleContinuous(const Vec2f &rndSamples, Vec2f &oUV, Vec2ui &oSegm, float *oPdf) const
    {
        float margPdf;
        float condPdf;

        mMarginal->SampleContinuous(rndSamples.x, oUV.y, oSegm.y, margPdf);
        mConditionalV[oSegm.y]->SampleContinuous(rndSamples.y, oUV.x, oSegm.x, condPdf);

        if (oPdf != nullptr)
            *oPdf = margPdf * condPdf;
    }

    float Pdf(const Vec2f &aUV) const
    {
        // Find u and v segments
        uint32_t iu = 
            Math::Clamp((uint32_t)(aUV.x * mConditionalV[0]->mCount), 0u, mConditionalV[0]->mCount - 1);
        uint32_t iv = 
            Math::Clamp((uint32_t)(aUV.y * mMarginal->mCount), 0u, mMarginal->mCount - 1);

        // Compute probabilities
        return mConditionalV[iv]->mPdf[iu] * mMarginal->mPdf[iv];
    }

private:
    std::vector<Distribution1D*>     mConditionalV;
    Distribution1D                  *mMarginal;
};
