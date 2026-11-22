#version 460

// Linear HDR radiance written by the path tracer. Read with imageLoad, so pixel (x,y) of the
// output maps exactly to pixel (x,y) on screen - both have their origin at the top-left in Vulkan.
layout(set = 0, binding = 0, rgba32f) uniform readonly image2D uRadiance;

layout(push_constant) uniform DisplayParams
{
    float exposure;
    uint  tonemapper; // 0 = none (clamp), 1 = ACES fitted
    uint  encodeSRGB; // 1 when the swapchain is UNORM and won't apply the sRGB curve for us
    uint  padding;
} pc;

layout(location = 0) out vec4 outColor;

// Krzysztof Narkowicz's fit of the ACES RRT+ODT. Cheap, filmic shoulder, slight saturation boost.
vec3 TonemapACESFitted(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Exact piecewise sRGB OETF (not the pow(1/2.2) approximation).
vec3 LinearToSRGB(vec3 c)
{
    c           = clamp(c, 0.0, 1.0);
    vec3 low    = c * 12.92;
    vec3 high   = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), c));
}

void main()
{
    vec3 hdr = imageLoad(uRadiance, ivec2(gl_FragCoord.xy)).rgb * pc.exposure;

    // "None" matters: when validating the path tracer against analytic results (e.g. a furnace test)
    // you want to see raw radiance, not a tonemapped approximation of it.
    vec3 ldr = (pc.tonemapper == 1u) ? TonemapACESFitted(hdr) : clamp(hdr, 0.0, 1.0);

    outColor = vec4(pc.encodeSRGB == 1u ? LinearToSRGB(ldr) : ldr, 1.0);
}
