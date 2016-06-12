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

    protected:
        float mBasisValues[9];
    };


    class SteeringBasisValue : public SteeringValue
    {
    public:
        // Sets the value of spherical harmonic base at given direction multiplied by the factor
        void GenerateForSphericalHarmonic(const Vec3f &aDirection, float aMulFactor)
        {
            aDirection; aMulFactor;

            // TODO: Asserts:
            //  - direction: normalized
            //  - factor: non-negative

            // TODO: Get from paper...
        }
    };


    class SteeringCoefficients : public SteeringValue
    {
    public:
        // Generate clamped cosine spherical harmonic coefficients for the given normal
        void GenerateForClampedCosSh(const Vec3f &aNormal)
        {
            aNormal;

            // TODO: Asserts:
            //  - normal: normalized

            // TODO: Get from paper...
        }
    };


    class Vertex
    {
    public:
        Vertex(const Vec3f &aDirection, const SteeringBasisValue &aWeight) : 
            direction(aDirection),
            weight(aWeight)
        {};

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
        TriangleNode() : TreeNode(true) {}

    protected:
        SteeringBasisValue      mWeight;

        //Vertex*               mVertices[3]; // Weak pointers - not owned by the triangle

        // FIXME: This is suboptimal, both in terms of memory consumption and memory non-locality
        std::shared_ptr<Vertex> mVertices[3];
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

    static void FreeNodesStack(std::stack<TreeNode*> &aNodes)
    {
        while (!aNodes.empty())
        {
            FreeNode(aNodes.top());
            aNodes.pop();
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

        std::stack<TreeNode*> toDoTriangles;

        if (!GenerateInitialEmTriangulation(toDoTriangles, aEmImage, aUseBilinearFiltering))
            return false;

        if (!RefineEmTriangulation(oTriangles, toDoTriangles, aEmImage, aUseBilinearFiltering))
            return false;

        // Assert: the stack must be empty

        return true;
    }

    // Generates initial set of triangles and their vertices
    static bool GenerateInitialEmTriangulation(
        std::stack<TreeNode*>       &oTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        oTriangles;  aEmImage;  aUseBilinearFiltering;

        // Generate the geometrical data
        Vec3f vertices[12];
        Vec3ui faces[20];
        Geom::UnitIcosahedron(vertices, faces);

        PG3_ERROR_CODE_NOT_TESTED("");

        // Allocate shared vertices for the triangles
        std::vector<std::shared_ptr<Vertex>> sharedVertices(Utils::ArrayLength(vertices)); // empty shared pointers
        for (uint32_t i = 0; i < Utils::ArrayLength(vertices); i++)
        {
            const auto &vertexCoords = vertices[i];

            SteeringBasisValue weight;
            weight.GenerateForSphericalHarmonic(vertexCoords, 1.0f/*TODO: evaluate EM*/);

            sharedVertices[i] = std::make_shared<Vertex>(vertexCoords, weight);
        }

        // TODO: Build triangle set

        return false;
    }

    // Sub-divides the "to do" triangle set of triangles according to the refinement rule and
    // fills the output list of triangles. The refined triangles are released. The triangles 
    // are either moved from the "to do" set into the output list or deleted on error.
    // Although the "to do" triangle set is a TreeNode* container, it must contain 
    // TriangleNode* data only, otherwise an error will occur.
    static bool RefineEmTriangulation(
        std::list<TreeNode*>        &oRefinedTriangles,
        std::stack<TreeNode*>       &aToDoTriangles,
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

    static bool _UnitTest_TriangulateEm(
        const UnitTestBlockLevel aMaxUtBlockPrintLevel)
    {
        PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblWholeTest,
            "EnvironmentMapSteeringSampler::TriangulateEm()");

        // TODO: Empty EM

        // Constant EM (Luminance 1, small)
        {
            PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSubTest, "Small constant EM");

            std::unique_ptr<EnvironmentMapImage> image(
                EnvironmentMapImage::LoadImage(
                    ".\\Light Probes\\Debugging\\Const white 8x4.exr"));
            if (!image)
            {
                PG3_UT_FATAL_ERROR(aMaxUtBlockPrintLevel, eutblSubTest,
                                   "Small constant EM", "Unable to load image!");
                return false;
            }

            PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSingleStep, "Initial triangulation");

            std::stack<TreeNode*> triangles;
            GenerateInitialEmTriangulation(triangles, *image, false);

            // Test count: regular icosahedron (20 faces, 30 edges, and 12 vertices)
            if (triangles.size() != 20)
            {
                std::ostringstream errorDescription;
                errorDescription << "Initial triangle count is ";
                errorDescription << triangles.size();
                errorDescription << " instead of 20!";
                PG3_UT_END_FAILED(aMaxUtBlockPrintLevel, eutblSingleStep, "Initial triangulation",
                                  errorDescription.str().c_str());
                return false;
            }

            // TODO: Check each triangle: vertices are not equal, vertices are normalized
            // TODO: Test vertex weights, but not triangle weights: ???

            PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSingleStep, "Initial triangulation");

            PG3_UT_BEGIN(aMaxUtBlockPrintLevel, eutblSingleStep, "Triangulation refinement");

            // TODO: Refine triangulation

            // TODO: Test:
            // - Count: same as initial triangulation
            // - Weights...

            FreeNodesStack(triangles);

            PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSingleStep, "Triangulation refinement");

            PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblSubTest, "Small constant EM");
        }

        // TODO: Constant EM (Luminance 1, large?)

        // TODO: Constant EM (Luminance 0)

        // TODO: Synthetic EM (One pixel?)

        // TODO: ...

        PG3_UT_END_PASSED(aMaxUtBlockPrintLevel, eutblWholeTest,
                          "EnvironmentMapSteeringSampler::TriangulateEm()");
        return true;
    }

#endif
};
