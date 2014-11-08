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

#include <algorithm>
#include <vector>
#include "math.hxx"

// Representation of a probability density function over the interval [0,1]
struct Distribution1D 
{
public:
    Distribution1D(const float * const func, int n) :
        mCount(n)
    {
        PG3_DEBUG_ASSERT(n > 0);

        mPdf = new float[mCount];
        mCdf = new float[mCount+1];
        
        // Compute integral of step function at $x_i$.
        // Note that the whole integral spans over the interval [0,1].
        mCdf[0] = 0.;
        for (int i = 1; i < mCount + 1; ++i)
        {
            PG3_DEBUG_ASSERT_VAL_NONNEGATIVE(func[i - 1]);
            mCdf[i] = mCdf[i - 1] + func[i - 1] / mCount; // 1/mCount = size of a segment
        }

        // Transform step function integral into CDF
        mFuncIntegral = mCdf[mCount];
        if (mFuncIntegral == 0.f) 
        {
            // The function is zero; use uniform PDF as a fallback
            for (int i = 1; i < mCount + 1; ++i)
            {
                mPdf[i-1] = 1.0f / mCount;
                mCdf[i]   = (float)i / mCount;
            }
        }
        else 
        {
            for (int i = 1; i < mCount + 1; ++i)
            {
                mPdf[i - 1]  = func[i - 1] / mFuncIntegral;
                mCdf[i]     /= mFuncIntegral;
            }
        }
        PG3_DEBUG_ASSERT_FLOAT_EQUAL(mCdf[mCount], 1.0f, 1e-7F);
    }

    ~Distribution1D()
    {
        delete[] mPdf;
        delete[] mCdf;
    }

    PG3_PROFILING_NOINLINE
    float SampleContinuous(float rndSample, float *pdf, int *segm = NULL) const 
    {
        // Find surrounding CDF segments and _offset_
        float *ptr = std::upper_bound(mCdf, mCdf+mCount+1, rndSample);
        int segment = Clamp(int(ptr-mCdf-1), 0, mCount - 1);
        if (segm) *segm = segment;

        PG3_DEBUG_ASSERT_VAL_IN_RANGE(segment, 0, mCount - 1);
        PG3_DEBUG_ASSERT(rndSample >= mCdf[segment] && (rndSample < mCdf[segment+1] || rndSample == 1.0f));

        // Fix the case when func ends with zeros
        if (mCdf[segment] == mCdf[segment + 1])
        {
            PG3_DEBUG_ASSERT(rndSample == 1.0f);

            do { 
                segment--; 
            } while (mCdf[segment] == mCdf[segment + 1] && segment > 0);

            PG3_DEBUG_ASSERT(mCdf[segment] != mCdf[segment + 1]);
        }

        // Compute offset within CDF segment
        float offset = (rndSample - mCdf[segment]) / (mCdf[segment+1] - mCdf[segment]);
        PG3_DEBUG_ASSERT_VALID_FLOAT(offset);
        PG3_DEBUG_ASSERT_VAL_IN_RANGE(offset, 0.0f, 1.0f);

        // Get the segment's constant PDF
        if (pdf) 
            *pdf = mPdf[segment];
        PG3_DEBUG_ASSERT(mPdf[segment] > 0);

        // Return $x\in{}[0,1]$ corresponding to sample
        return (segment + offset) / mCount;
    }

    //PG3_PROFILING_NOINLINE
    //int SampleDiscrete(float rndSample, float *probability) const
    //{
    //    // Find surrounding CDF segments and _offset_
    //    float *ptr = std::upper_bound(mCdf, mCdf+mCount+1, rndSample);
    //    int segment = std::max(0, int(ptr-mCdf-1));

    //    PG3_DEBUG_ASSERT(segment < mCount);
    //    PG3_DEBUG_ASSERT(rndSample >= mCdf[segment] && rndSample < mCdf[segment+1]);

    //    if (probability)
    //        *probability = mPdf[segment] / mCount;
    //    return segment;
    //}

    // This class is not copyable because of a const member.
    // If we don't delete the assignment operator and copy constructor 
    // explicitly, the compiler may complain about not being able 
    // to create default implementations.
    Distribution1D & operator=(const Distribution1D&) = delete;
    Distribution1D(const Distribution1D&) = delete;

private:
    friend struct Distribution2D;

    float       *mPdf, *mCdf;
    float       mFuncIntegral;
    const int   mCount;
};

struct Distribution2D
{
public:
    Distribution2D(const float *func, int nu, int nv)
    {
        pConditionalV.reserve(nv);
        for (int v = 0; v < nv; ++v) 
        {
            // Compute conditional sampling distribution for $\tilde{v}$
            pConditionalV.push_back(new Distribution1D(&func[v*nu], nu));
        }

        // Compute marginal sampling distribution $p[\tilde{v}]$
        std::vector<float> marginalFunc;
        marginalFunc.reserve(nv);
        for (int v = 0; v < nv; ++v)
            marginalFunc.push_back(pConditionalV[v]->mFuncIntegral);
        pMarginal = new Distribution1D(&marginalFunc[0], nv);
    }

    ~Distribution2D()
    {
        delete pMarginal;
        for (unsigned int i = 0; i < pConditionalV.size(); ++i)
            delete pConditionalV[i];
    }

    void SampleContinuous(const Vec2f &rndSamples, Vec2f &uv, float *pdf) const
    {
        float margPdf;
        float condPdf;
        int v;

        uv.y = pMarginal->SampleContinuous(rndSamples.x, &margPdf, &v);
        uv.x = pConditionalV[v]->SampleContinuous(rndSamples.y, &condPdf);

        *pdf = margPdf * condPdf;
    }

    PG3_PROFILING_NOINLINE
    float Pdf(const Vec2f &uv) const
    {
        // Find u and v segments
        int iu = 
            Clamp((int)(uv.x * pConditionalV[0]->mCount), 0, pConditionalV[0]->mCount - 1);
        int iv = 
            Clamp((int)(uv.y * pMarginal->mCount), 0, pMarginal->mCount - 1);

        // Compute probabilities
        return pConditionalV[iv]->mPdf[iu] * pMarginal->mPdf[iv];
    }

private:
    std::vector<Distribution1D*>     pConditionalV;
    Distribution1D                  *pMarginal;
};
