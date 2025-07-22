#include "Commons.hlsli"

struct PSOutput
{
    float4 color : SV_TARGET;
    float depth : SV_DEPTH;
};

PSOutput main(VSOutput input)
{  
    PSOutput output;
    output.color = float4(cb_object.color, 1);
    output.depth = 1.0f;
    
    float3 center = cb_object.position; // sphere center
    float radius = cb_object.radius; // sphere radius
    float3 origin = cb_scene.world_eye; // ray world space origin
    float3 direction = normalize(input.world_position - origin); // ray world space direction
    
    /*
        ray/sphere intersection
        p(t) = o + td                                                      ray
        (p - c).(p - c) = r^2                                              sphere
        ((o + td) - c).((o + td) - c) = r^2                                plug p(t) into the sphere equation
        (o + td).((o + td) - c) - c.((o + td) - c) = r^2                   . is distributive
        (o + td).(o + td) - (o + td).c - (c.(o + td) - c.c) = r^2          . is distributive
        (o + td).o + (o + td).td - (o.c + td.c) - (c.o + c.td - c.c) = r^2 . is distributive
        o.o + td.o + o.td + td.td - o.c - td.c - c.o - c.td + c.c = r^2    . is distributive
        o.o + 2td.o + td.td - 2o.c -2td.c + c.c = r^2                      algebra
        t^2(d.d) + t(2d.o - 2d.c) + (o.o + c.c - 2o.c - r^2) = 0           algebra
        t^2 + t(2d.o - 2d.c) + (o.o + c.c - 2o.c - r^2) = 0                d.d = 1 since |d| = 1
    
        discriminant = (2d.o - 2d.c)(2d.o - 2d.c) - 4(o.o + c.c - 2o.c - r^2)
    */
    {
        float b = 2 * dot(direction, origin) - 2 * dot(direction, center); // 2d.o - 2d.c
        float c = dot(origin, origin) + dot(center, center) - 2 * dot(origin, center) - radius * radius; // o.o + c.c - 2o.c - r^2
        float discriminant = b * b - 4 * c;
        
        if (discriminant < 0)
        {
            discard;
        }
        
        float t = (-b - sqrt(discriminant)) / 2;
        
        if (t < 0)
        {
            discard;
        }
        
        // compute updated depth of the fragment
        float3 p_world = origin + t * direction; // plug t into the ray equation to find the intersection point
        float4 p_clip = mul(cb_scene.projection, mul(cb_scene.view, float4(p_world, 1))); // go from world space to clip space
        float4 p_ndc = p_clip / p_clip.w; // manual perspective divide

        // write computed depth
        output.depth = p_ndc.z;
    }
    
    return output;
}
