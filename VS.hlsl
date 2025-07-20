#include "Commons.hlsli"

VSOutput main(VSInput input)
{
    VSOutput output;
    output.clip_position = mul(cb_scene.projection, mul(cb_scene.view, mul(cb_object.model, float4(input.position, 1))));
    return output;
}
