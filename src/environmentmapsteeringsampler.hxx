#pragma once

#include "environmentmapimage.hxx"
#include "unittesting.hxx"
#include "debugging.hxx"
#include "spectrum.hxx"
#include "types.hxx"

#include <list>
#include <stack>
#include <memory>

// Environment map sampler based on the paper "Steerable Importance Sampling"
// from Kartic Subr and Jim Arvo, 2007
class EnvironmentMapSteeringSampler
{
public:

     EnvironmentMapSteeringSampler() {}
    ~EnvironmentMapSteeringSampler() {}

protected:

    class SteeringValue
    {
    public:
        float operator* (const SteeringValue &aValue) const
        {
            float retval = 0.0f;
            for (size_t i = 0; i < 9; i++)
                retval += mBasisValues[i] * aValue.mBasisValues[i];
            return retval;
        }

        bool operator == (const SteeringValue &aBasis) const
        {
            return (mBasisValues[0] == aBasis.mBasisValues[0])
                && (mBasisValues[1] == aBasis.mBasisValues[1])
                && (mBasisValues[2] == aBasis.mBasisValues[2])
                && (mBasisValues[3] == aBasis.mBasisValues[3])
                && (mBasisValues[4] == aBasis.mBasisValues[4])
                && (mBasisValues[5] == aBasis.mBasisValues[5])
                && (mBasisValues[6] == aBasis.mBasisValues[6])
                && (mBasisValues[7] == aBasis.mBasisValues[7])
                && (mBasisValues[8] == aBasis.mBasisValues[8]);
        }

        bool operator != (const SteeringValue &aBasis) const
        {
            return !(*this == aBasis);
        }

    protected:
        float mBasisValues[9];
    };


    class SteeringBasisValue : public SteeringValue
    {
    public:
        // Sets the value of spherical harmonic base at given direction multiplied by the factor
        SteeringBasisValue& GenerateForSphericalHarmonic(const Vec3f &aDirection, float aMulFactor)
        {
            aDirection; aMulFactor;

            // TODO: Asserts:
            //  - direction: normalized
            //  - factor: non-negative

            // TODO: Get from paper...

            // debug
            std::memset(mBasisValues, 0, sizeof(mBasisValues));

            return *this;
        }
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


    class Vertex
    {
    public:
        Vertex(const Vec3f &aDirection, const SteeringBasisValue &aWeight) : 
            direction(aDirection),
            weight(aWeight)
        {};

        bool operator == (const Vertex &aVertex) const
        {
            return (direction == aVertex.direction)
                && (weight == aVertex.weight);
        }

        Vec3f               direction; // TODO: Use (2D) spherical coordinates to save memory?
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
        TriangleNode(
            std::shared_ptr<Vertex> aVertex1,
            std::shared_ptr<Vertex> aVertex2,
            std::shared_ptr<Vertex> aVertex3
            ) :
            TreeNode(true)
        {
            mSharedVertices[0] = aVertex1;
            mSharedVertices[1] = aVertex2;
            mSharedVertices[2] = aVertex3;
        }

        bool operator == (const TriangleNode &aTriangle)
        {
            return (mWeight == aTriangle.mWeight)
                && (mSharedVertices[0] == aTriangle.mSharedVertices[0])
                && (mSharedVertices[1] == aTriangle.mSharedVertices[1])
                && (mSharedVertices[2] == aTriangle.mSharedVertices[2]);
        }

    //protected:
    public:
        SteeringBasisValue      mWeight;

        // TODO: This is sub-optimal, both in terms of memory consumption and memory non-locality
        std::shared_ptr<Vertex> mSharedVertices[3];
    };

public:

    // Builds the internal structures needed for sampling
    bool Build(
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        Cleanup();

        std::list<TreeNode*> tmpTriangles;

        if (!TriangulateEm(tmpTriangles, aEmImage, aUseBilinearFiltering))
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
        PG3_ERROR_CODE_NOT_TESTED("");

        if (aNode == nullptr)
            return;

        if (aNode->IsTriangleNode())
            delete static_cast<TriangleNode*>(aNode);
        else
            delete static_cast<InnerNode*>(aNode);
    }

    static void FreeNodesList(std::list<TreeNode*> &aNodes)
    {
        PG3_ERROR_CODE_NOT_TESTED("");

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

    static void FreeTrianglesDeque(std::deque<TriangleNode*> &aTriangles)
    {
        while (!aTriangles.empty())
        {
            delete aTriangles.back();
            aTriangles.pop_back();
        }
    }

    // Generates adaptive triangulation of the given environment map: fills the list of triangles
    static bool TriangulateEm(
        std::list<TreeNode*>        &oTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        // TODO: Asserts:
        //  - triangles: empty?

        std::deque<TriangleNode*> toDoTriangles;

        if (!GenerateInitialEmTriangulation(toDoTriangles, aEmImage, aUseBilinearFiltering))
            return false;

        if (!RefineEmTriangulation(oTriangles, toDoTriangles, aEmImage, aUseBilinearFiltering))
            return false;

        // Assert: the stack must be empty

        return true;
    }

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
        {
            const auto &vertexDir = vertices[i];

            const auto radiance  = aEmImage.Evaluate(vertexDir, aUseBilinearFiltering);
            const auto luminance = radiance.Luminance();

            SteeringBasisValue weight;
            weight.GenerateForSphericalHarmonic(vertexDir, luminance);

            sharedVertices[i] = std::make_shared<Vertex>(vertexDir, weight);
        }

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
                sharedVertices[faceVertices.Get(2)]));
        }

        return true;
    }

    // Sub-divides the "to do" triangle set of triangles according to the refinement rule and
    // fills the output list of triangles. The refined triangles are released. The triangles 
    // are either moved from the "to do" set into the output list or deleted on error.
    // Although the "to do" triangle set is a TreeNode* container, it must contain 
    // TriangleNode* data only, otherwise an error will occur.
    static bool RefineEmTriangulation(
        std::list<TreeNode*>        &oRefinedTriangles,
        std::deque<TriangleNode*>   &aToDoTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        oRefinedTriangles; aToDoTriangles; aEmImage; aUseBilinearFiltering;

        // TODO: Asserts:
        //  - to do triangles: non-empty?

        // TODO: While TO DO list is not empty
        // - Check whether it is a triangle (assert?)
        // - If the current triangle is OK, move it to the output list
        // - If the current triangle is NOT OK, generate sub-division triangles into TO DO list and delete it

        // On error, delete the TO DO list

        // Assert: TO DO list must be empty

        return false;
    }

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
            const auto &sharedVertices = triangle->mSharedVertices;

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
                    const auto edgeLengthSqr = (vertex->direction - vertexNext->direction).LenSqr();
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
                    const auto radiance  = aEmImage.Evaluate(vertex->direction, aUseBilinearFiltering);
                    const auto luminance = radiance.Luminance();
                    auto referenceWeight = 
                        SteeringBasisValue().GenerateForSphericalHarmonic(vertex->direction,
                                                                          luminance);
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
        std::deque<TriangleNode*>   &oTriangles,
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        oTriangles; aMaxUtBlockPrintLevel; aEmImage; aUseBilinearFiltering;

        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement");

        // TODO: Refine triangulation
        //if (!RefineEmTriangulation(oTriangles, toDoTriangles, aEmImage, aUseBilinearFiltering))
        //    return false;

        // TODO: Test:
        // - Count: same as initial triangulation
        // - Weights...

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTestLevel2, "Triangulation refinement");

        return true;
    }

    static bool _UnitTest_TriangulateEm_SingleEm(
        const UnitTestBlockLevel     aMaxUtBlockPrintLevel,
        char                        *aTestName,
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

        std::deque<TriangleNode*> triangles;

        if (!_UnitTest_TriangulateEm_SingleEm_InitialTriangulation(triangles,
                                                                   aMaxUtBlockPrintLevel,
                                                                   *image.get(),
                                                                   aUseBilinearFiltering))
        {
            FreeTrianglesDeque(triangles);
            return false;
        }

        //if (!_UnitTest_TriangulateEm_SingleEm_RefineTriangulation(triangles,
        //                                                          aMaxUtBlockPrintLevel,
        //                                                          *image.get(),
        //                                                          aUseBilinearFiltering))
        //{
        //    FreeTrianglesDeque(triangles);
        //    return false;
        //}

        FreeTrianglesDeque(triangles);

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
                                              "Small white EM",
                                              ".\\Light Probes\\Debugging\\Const white 8x4.exr",
                                              false))
            return false;

        if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
                                              "Large white EM",
                                              ".\\Light Probes\\Debugging\\Const white 1024x512.exr",
                                              false))
            return false;

        if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
                                              "Single pixel EM",
                                              ".\\Light Probes\\Debugging\\Single pixel.exr",
                                              false))
            return false;

        if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
                                              "Satellite EM",
                                              ".\\Light Probes\\hdr-sets.com\\HDR_SETS_SATELLITE_01_FREE\\107_ENV_DOMELIGHT.exr",
                                              false))
            return false;

        if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
                                              "Peace Garden EM",
                                              ".\\Light Probes\\panocapture.com\\PeaceGardens_Dusk.exr",
                                              false))
            return false;

        if (!_UnitTest_TriangulateEm_SingleEm(aMaxUtBlockPrintLevel,
                                              "Doge2 EM",
                                              ".\\Light Probes\\High-Resolution Light Probe Image Gallery\\doge2.exr",
                                              false))
            return false;

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblWholeTest,
                          "EnvironmentMapSteeringSampler::TriangulateEm()");
        return true;
    }

#endif
};
