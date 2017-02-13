#pragma once

#include "em_sampler.hxx"
#include "distribution.hxx"
#include "debugging.hxx"

template <typename TEmValues>
class EnvironmentMapSimpleSphericalSampler : public EnvironmentMapSampler<TEmValues>
{
public:

    EnvironmentMapSimpleSphericalSampler() :
        mPlan2AngPdfCoeff(1.0f / (2.0f * Math::kPiF * Math::kPiF))
    {}

    virtual bool Init(
        std::shared_ptr<EnvironmentMapImage>    aEmImage,
        bool                                    aUseBilinearFiltering) override
    {
        if (!EnvironmentMapSampler::Init(aEmImage, aUseBilinearFiltering))
            return false;

        mDistribution.reset(GenerateImageDistribution());

        return mDistribution != nullptr;
    }


    virtual bool SampleImpl(
        Vec3f           &oDirection,
        float           &oPdfW,
        SpectrumF       &oRadianceCos, // radiance * abs(cos(thetaIn)
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide,
        Rng             &aRng) const override
    {
        if (!IsBuilt())
            return false;

        const Vec2f uniSample = aRng.GetVec2f();

        PG3_ASSERT_FLOAT_IN_RANGE(uniSample.x, 0.0f, 1.0f);
        PG3_ASSERT_FLOAT_IN_RANGE(uniSample.y, 0.0f, 1.0f);

        Vec2f uv;
        Vec2ui segm;
        float distrPdf;
        mDistribution->SampleContinuous(uniSample, uv, segm, &distrPdf);

        PG3_ASSERT(distrPdf > 0.f);

        oDirection = Geom::LatLong2Dir(uv);

        // Convert the sample's planar PDF over the rectangle [0,1]x[0,1] to 
        // the angular PDF on the unit sphere over the appropriate trapezoid
        //
        // angular pdf = planar pdf * planar segment surf. area / sphere segment surf. area
        //             = planar pdf * (1 / (width*height)) / (2*Pi*Pi*Sin(MidTheta) / (width*height))
        //             = planar pdf / (2*Pi*Pi*Sin(MidTheta))
        //
        // FIXME: Uniform sampling of a segment of the 2D distribution doesn't yield 
        //        uniform sampling of a corresponding segment on a sphere 
        //        - the closer we are to the poles, the denser the sampling will be 
        //        (even though the overall probability of the segment is correct).
        // \int_a^b{1/hdh} = [ln(h)]_a^b = ln(b) - ln(a)
        const float sinMidTheta = SinMidTheta(mEmImage.get(), segm.y);
        oPdfW = distrPdf * mPlan2AngPdfCoeff / sinMidTheta;

        // Radiance multiplied by cosine
        const SpectrumF radiance = EvalRadiance(segm);
        const float cosThetaIn = Dot(oDirection, aSurfFrame.Normal());
        if (   (aSampleFrontSide && (cosThetaIn > 0.0f))
            || (aSampleBackSide  && (cosThetaIn < 0.0f)))
            oRadianceCos = radiance * std::abs(cosThetaIn);
        else
            oRadianceCos.MakeZero();

        return false;
    }


    virtual float PdfW(
        const Vec3f     &aDirection,
        const Frame     &aSurfFrame,
        bool             aSampleFrontSide,
        bool             aSampleBackSide) const override
    {
        aSurfFrame, aSampleFrontSide, aSampleBackSide; // unused param

        PG3_ASSERT(IsBuilt());

        const Vec2f uv = Geom::Dir2LatLongFast(aDirection);
        float pdf = mDistribution->Pdf(uv) * mPlan2AngPdfCoeff / SinMidTheta(mEmImage.get(), uv.y);

        return pdf;
    }


    virtual void ReleaseData() override
    {
        mDistribution.reset();
        EnvironmentMapSampler::ReleaseData();
    };

private:

    bool IsBuilt() const
    {
        return (mEmImage != nullptr) && (mDistribution != nullptr);
    }


    // Generates a 2D distribution with latitude-longitude mapping 
    // based on the luminance of the provided environment map image
    Distribution2D* GenerateImageDistribution() const
    {
        // Prepare source distribution from the environment map image data, 
        // i.e. convert image values so that the probability of a pixel within 
        // the lattitute-longitude parametrization is equal to the angular probability of 
        // the projected segment on a unit sphere.

        if (!mEmImage)
            return nullptr;

        const Vec2ui size = mEmImage->Size();
        std::unique_ptr<float[]> srcData(new float[size.x * size.y]);

        if (srcData == nullptr)
            return nullptr;

        for (uint32_t row = 0; row < size.y; ++row)
        {
            // We compute the projected surface area of the current segment on the unit sphere.
            // We can ommit the height of the segment because it only changes the result 
            // by a multiplication constant and thus doesn't affect the shape of the resulting PDF.
            const float sinAvgTheta = SinMidTheta(mEmImage.get(), row);

            const uint32_t rowOffset = row * size.x;

            for (uint32_t column = 0; column < size.x; ++column)
            {
                const float luminance = mEmImage->ElementAt(column, row).Luminance();
                srcData[rowOffset + column] = sinAvgTheta * luminance;
            }
        }

        Distribution2D* distribution = new Distribution2D(srcData.get(), size.x, size.y);

        return distribution;
    }


    // Returns radiance for the given segment of the image
    SpectrumF EvalRadiance(const Vec2ui &aSegm) const
    {
        PG3_ASSERT(mEmImage != nullptr);
        PG3_ASSERT_INTEGER_IN_RANGE(aSegm.x, 0u, mEmImage->Width());
        PG3_ASSERT_INTEGER_IN_RANGE(aSegm.y, 0u, mEmImage->Height());

        // FIXME: This interface shouldn't be used if bilinear or any smoother filtering is active!

        return mEmImage->ElementAt(aSegm.x, aSegm.y);
    }


    // The sine of latitude of the midpoint of the map pixel (a.k.a. segment)
    static float SinMidTheta(const EnvironmentMapImage* aImage, const uint32_t aSegmY)
    {
        PG3_ASSERT(aImage != nullptr);

        const uint32_t height = aImage->Height();

        PG3_ASSERT_INTEGER_LESS_THAN(aSegmY, height);

        const float result = sinf(Math::kPiF * (aSegmY + 0.5f) / height);

        PG3_ASSERT(result > 0.f && result <= 1.f);

        return result;
    }


    // The sine of latitude of the midpoint of the map pixel defined by the given v coordinate.
    static float SinMidTheta(const EnvironmentMapImage* aImage, const float aV)
    {
        PG3_ASSERT(aImage != nullptr);
        PG3_ASSERT_FLOAT_IN_RANGE(aV, 0.0f, 1.0f);

        const uint32_t height = aImage->Height();
        const uint32_t segment = std::min((uint32_t)(aV * height), height - 1u);

        return SinMidTheta(aImage, segment);
    }

private:

    std::unique_ptr<Distribution2D> mDistribution;      // 2D distribution of the environment map
    const float                     mPlan2AngPdfCoeff;  // Coefficient for conversion from planar to angular PDF
};

typedef EnvironmentMapSimpleSphericalSampler<EnvironmentMapImage>   SimpleSphericalImageEmSampler;
typedef EnvironmentMapSimpleSphericalSampler<ConstEnvironmentValue> SimpleSphericalConstEmSampler;
