#pragma once

#include "environmentmapimage.hxx"
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
            // TODO: Asserts:
            //  - normal: normalized

            // TODO: Get from paper...
        }
    };


    class Vertex
    {
    public:
        Vec3f               mDirection; // TODO: Use (2D) spherical coordinates to save memory?
        SteeringBasisValue  mWeight;
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

    // Generates adaptive triangulation of the given environment map: fills the list of triangles
    bool TriangulateEm(
        std::list<TreeNode*>        &oTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering) const
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
    bool GenerateInitialEmTriangulation(
        std::stack<TreeNode*>       &oTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering) const
    {
        return false;
    }

    // Sub-divides the "to do" triangle set of triangles according to the refinement rule and
    // fills the output list of triangles. The refined triangles are released. The triangles 
    // are either moved from the "to do" set into the output list or deleted on error.
    // Although the "to do" triangle set is a TreeNode* container, it must contain 
    // TriangleNode* data only, otherwise an error will occur.
    bool RefineEmTriangulation(
        std::list<TreeNode*>        &oRefinedTriangles,
        std::stack<TreeNode*>       &aToDoTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering
        ) const
    {
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
    }

protected:

    std::unique_ptr<TreeNode>   mTreeRoot;
};
