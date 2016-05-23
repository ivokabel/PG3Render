#pragma once

#include "environmentmapimage.hxx"
#include "debugging.hxx"
#include "spectrum.hxx"
#include "types.hxx"

// Environment map sampler based on the paper "Steerable Importance Sampling"
// from Kartic Subr and Jim Arvo, 2007
class EnvironmentMapSteeringSampler
{
public:
    // TODO:
    //  EnvironmentMapSteeringSampler(EnvironmentMapImage& aEmImage, bool aDoBilinFiltering, ...)
    // ~EnvironmentMapSteeringSampler

    // TODO: Sample(const Normal &)

    // TODO: ...

protected:
    // TODO: Data structures: vertices, triangles (leaves), inner nodes
    class Vertex
    {
    };

    class TreeNode
    {
        // Node type?
    };

    class Triangle : TreeNode
    {
        // Init node type
    };

    class InnerNode : TreeNode
    {
        // Init node type
    };

    // TODO: Triangulate EM

    // TODO: Build tree

    // TODO: Pick triangle

    // TODO: Sample triangle

    // TODO: ...

    // TODO: ...

    int dummy;
};
