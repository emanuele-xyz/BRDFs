#include "Commons.hlsli"

VSOutput main(VSInput input)
{
    float4 world_position = mul(cb_object.model, float4(input.position, 1));
    
    VSOutput output;
    output.world_position = world_position.xyz;
    output.clip_position = mul(cb_scene.projection, mul(cb_scene.view, world_position));
    return output;
}
