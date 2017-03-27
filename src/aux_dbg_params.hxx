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

    AuxDbgParams() :
        float1(Math::InfinityF()),
        float2(Math::InfinityF()),
        float3(Math::InfinityF()),
        float4(Math::InfinityF()),
        float5(Math::InfinityF())
    {}

    bool IsEmpty() const
    {
        return
            (  (float1 == Math::InfinityF())
            && (float2 == Math::InfinityF())
            && (float3 == Math::InfinityF())
            && (float4 == Math::InfinityF())
            && (float5 == Math::InfinityF()));
    }
};
