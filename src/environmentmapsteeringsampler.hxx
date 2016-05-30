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

    class SteerableWeight
    {
    protected:
        float mCoeffs[9]; // the first 9 coefficients for spherical harmonics or derived bases
    };


    class Vertex
    {
    public:
        Vec3f           mDirection; // TODO: Use (2D) spherical coordinates to save memory?
        SteerableWeight mWeight;
    };


    class TreeNode
    {
    public:
        TreeNode(bool aIsTriangleNode) : mIsTriangleNode(aIsTriangleNode) {}

        bool IsTriangleNode() { return mIsTriangleNode; }

    protected:
        bool mIsTriangleNode; // either triangle node or inner node
    };


    class InnerNode : public TreeNode
    {
    public:
        InnerNode() :
            TreeNode(false)
        {}

        ~InnerNode()
        {}

        // The node becomes the owner of the children and is responsible for releasing them
        void SetLeftChild( TreeNode* aLeftChild)  { mLeftChild.reset(aLeftChild); }
        void SetRightChild(TreeNode* aRightChild) { mLeftChild.reset(aRightChild); }

        const TreeNode* GetLeftChild()  { return mLeftChild.get(); }
        const TreeNode* GetRightChild() { return mRightChild.get(); }

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
        SteerableWeight         mWeight;
        //Vertex*               mVertices[3]; // Weak pointers - not owned by the triangle
        std::shared_ptr<Vertex> mVertices[3];
    };

public:

    bool Build(
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        Cleanup();

        std::list<TriangleNode*> tmpTriangles;

        if (!TriangulateEm(tmpTriangles, aEmImage, aUseBilinearFiltering))
            return false;

        if (!BuildTriangleTree(tmpTriangles))
            return false;

        return true;
    }

    // TODO: Sample(const Normal &)

    // TODO: ...

protected:

    // Releases the current data structures
    void Cleanup()
    {
        mTreeRoot.reset(nullptr);
    }

    // Generates adaptive triangulation of the given environment map: fills the list of triangles
    bool TriangulateEm(
        std::list<TriangleNode*>    &oTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        std::stack<TriangleNode*> toDoTriangles;

        if (!GenerateInitialEmTriangulation(toDoTriangles, aEmImage, aUseBilinearFiltering))
            return false;

        if (!RefineEmTriangulation(oTriangles, toDoTriangles, aEmImage, aUseBilinearFiltering))
            return false;

        // Assert: stack must be empty

        return true;
    }

    // Generates initial set of triangles and their vertices
    bool GenerateInitialEmTriangulation(
        std::stack<TriangleNode*>   &oTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        return false;
    }

    // Sub-divides the "to do" triangle set of triangles according to the refinement rule and
    // fills the output list of triangles. The refined triangles are released. The triangles 
    // are removed from the "to do" set and placed into the output list or deleted on error.
    bool RefineEmTriangulation(
        std::list<TriangleNode*>    &oRefinedTriangles,
        std::stack<TriangleNode*>   &aToDoTriangles,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering
        )
    {
        // TODO: While TO DO list is not empty
        // - If the current triangle is OK, move it to the output list
        // - If the current triangle is NOT OK, generate sub-division triangles into TO DO list and delete it

        // On error, delete the TO DO list

        // Assert: TO DO list must be empty

        return false;
    }

    // Build a balanced tree from the provided triangles.
    // The triangles are removed from the list and placed into the tree or deleted on error
    bool BuildTriangleTree(std::list<TriangleNode*> &aTriangles)
    {
        // TODO: Build the tree in bottom-to-top manner accumulating the children data into their parent

        // Assert: triangles list is empty
    }

    // TODO: Pick triangle, const

    // TODO: Sample triangle, const

protected:

    std::unique_ptr<TreeNode>   mTreeRoot;
};
