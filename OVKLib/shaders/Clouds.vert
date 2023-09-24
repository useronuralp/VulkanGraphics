#version 450

// INs
layout(location = 0) in vec3 a_Position;

layout(set = 0, binding = 0) uniform cloudUBO
{
    mat4 viewMatrix;
    mat4 projMatrix;
    vec4 BoundsMax;
    vec4 BoundsMin;
};

layout( push_constant ) uniform modelMat
{
	mat4 modelMatrix;
    vec4 color;
};

layout(location = 0) out vec3 v_Color;
layout(location = 1) out vec2 v_UV;
layout(location = 2) out vec3 v_BoundsMax;
layout(location = 3) out vec3 v_BoundsMin;


// NOTE: This will need to be carried in to the POST-FX pass. That is where the cloud caluclations happen. I am leaving it here for now, maybe you'll come back to this project and fix it later. 
// The tutorial: https://www.youtube.com/watch?v=4QOcCGI6xOU&t=3s&ab_channel=SebastianLague
// CLOUDS
vec2 rayBoxDst(vec3 boundsMin, vec3 boundsMax, vec3 rayOrigin, vec3 rayDir)
{
    vec3 t0 = (boundsMin - rayOrigin) / rayDir;
    vec3 t1 = (boundsMax - rayOrigin) / rayDir;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);

    float dstA = max(max(tmin.x, tmin.y), tmin.z);
    float dstB = min(tmax.x, min(tmax.y, tmax.z));

    float dstToBox = max(0, dstA);
    float dstInsideBox = max(0, dstB - dstToBox);
    return vec2(dstToBox, dstInsideBox);
}

void main()
{
    v_Color = color.xyz;
    v_UV = a_Position.xy;
    v_BoundsMax = BoundsMax.rgb;
    v_BoundsMin = BoundsMin.rgb;
    gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(a_Position, 1.0);
}