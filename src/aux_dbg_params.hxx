#pragma once

#include "math.hxx"

class AuxDbgParams
{
public:

    float   float1;
    float   float2;
    float   float3;
    float   float4;
    float   float5;

    bool    bool1;
    bool    bool2;
    bool    bool3;
    bool    bool4;
    bool    bool5;

    AuxDbgParams() :
        float1(Math::InfinityF()),
        float2(Math::InfinityF()),
        float3(Math::InfinityF()),
        float4(Math::InfinityF()),
        float5(Math::InfinityF()),
        bool1(false),
        bool2(false),
        bool3(false),
        bool4(false),
        bool5(false)
    {}

    bool IsEmpty() const
    {
        return
               (float1 == Math::InfinityF())
            && (float2 == Math::InfinityF())
            && (float3 == Math::InfinityF())
            && (float4 == Math::InfinityF())
            && (float5 == Math::InfinityF())
            && !bool1
            && !bool2
            && !bool3
            && !bool4
            && !bool5;
    }
};
