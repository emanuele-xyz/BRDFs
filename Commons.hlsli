#ifndef __COMMONS__
#define __COMMONS__

#include "ConstantBuffers.hlsli"

struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 clip_position : SV_POSITION;
};

cbuffer CBScene : register(b0)
{
    SceneConstants cb_scene;
};

cbuffer CBObject : register(b1)
{
    ObjectConstants cb_object;
}

#endif