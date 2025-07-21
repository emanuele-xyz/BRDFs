#ifndef __CONSTANT_BUFFERS__
#define __CONSTANT_BUFFERS__

struct SceneConstants
{
    matrix view;
    matrix projection;
    float3 world_eye;
    float _pad[1];
};

struct ObjectConstants
{
    matrix model;
    float3 color;
    float _pad0;
    float3 position;
    float radius;
};

#endif