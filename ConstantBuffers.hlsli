#ifndef __CONSTANT_BUFFERS__
#define __CONSTANT_BUFFERS__

struct SceneConstants
{
    matrix view;
    matrix projection;
};

struct ObjectConstants
{
    matrix model;
    float3 color;
    float _pad[1];
};

#endif