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

    bool Build(
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        // TODO: Clean up

        std::list<TriangleNode> tmpTriangles;
        std::list<Vertex>       tmpVertices;

        if (!TriangulateEm(tmpTriangles, tmpVertices, aEmImage, aUseBilinearFiltering))
            return false;

        // Move list if vertices to vector to save some memory and maybe increase memory locality
        // a bit. We do it before building the tree to minimizes memory consumption peak.
        if (!FinalizeVertices(tmpVertices))
            return false;
        
        tmpVertices.clear();

        if (!BuildTriangleTree(tmpTriangles))
            return false;

        return true;
    }

    // TODO: Sample(const Normal &)

    // TODO: ...

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
            TreeNode(false),
            mLeftChild(nullptr),
            mRightChild(nullptr)
        {}

        ~InnerNode()
        {
            delete mLeftChild;
            delete mRightChild;
        }

        // The node becomes the owner of the children and is responsible for releasing them
        void SetLeftChild( TreeNode* aLeftChild)  { mLeftChild  = aLeftChild; }
        void SetRightChild(TreeNode* aRightChild) { mRightChild = aRightChild; }

        const TreeNode* GetLeftChild()  { return mLeftChild; }
        const TreeNode* GetRightChild() { return mRightChild; }

    protected:
        // Children - owned by the node
        TreeNode *mLeftChild;
        TreeNode *mRightChild;
    };


    class TriangleNode : public TreeNode
    {
    public:
        TriangleNode() : TreeNode(true) {}

    protected:
        SteerableWeight mWeight;
        Vertex*         mVertices[3]; // Weak pointers - not owned by the triangle
    };

protected:

    // Generates adaptive triangulation of the given environment map:
    // fills the list of triangles and list of vertices
    bool TriangulateEm(
        std::list<TriangleNode>     &oTriangles,
        std::list<Vertex>           &oVertices,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering
        //float                  aAproximationThreshold    // How much error can we accept
        // TODO: Maximal triangle count?
        )
    {
        std::stack<TriangleNode> toDoTriangles;

        if (!GenerateInitialEmTriangulation(toDoTriangles, oVertices, 
                                            aEmImage, aUseBilinearFiltering))
            return false;

        if (!RefineEmTriangulation(oTriangles, oVertices, toDoTriangles,
                                   aEmImage, aUseBilinearFiltering))
            return false;

        return true;
    }

    // Generates initial set of triangles and their vertices
    bool GenerateInitialEmTriangulation(
        std::stack<TriangleNode>    &oTriangles,
        std::list<Vertex>           &oVertices,
        const EnvironmentMapImage   &aEmImage,
        bool                         aUseBilinearFiltering)
    {
        return false;
    }

    // Sub-divides the "to do" triangle set of triangles according to the refinement rule and
    // fills the output list of triangles
    bool RefineEmTriangulation(
        std::list<TriangleNode>     &oRefinedTriangles,
        std::list<Vertex>           &oVertices,
        std::stack<TriangleNode>    &aToDoTriangles,
        const EnvironmentMapImage   &aEmImage,
              bool                   aUseBilinearFiltering
              //float                  aAproximationThreshold    // How much error can we accept
              // TODO: Maximal triangle count?
              )
    {
        return false;
    }

    // Move list of vertices to the vector
    bool FinalizeVertices(std::list<Vertex> &aVertices)
    {
        // TODO: ...
    }

    // Build a balanced tree from the provided triangles
    bool BuildTriangleTree(std::list<TriangleNode> &aTriangles)
    {
        // TODO: ...
    }

    // TODO: Pick triangle

    // TODO: Sample triangle

protected:

    std::unique_ptr<TreeNode>   mTreeRoot;
    std::vector<Vertex>         mVertices; // triangle vertices, referenced from mTreeRoot
};
