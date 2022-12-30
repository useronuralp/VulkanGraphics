#version 450 core


#define MAX_POINT_LIGHT 10
layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

layout(set = 0, binding = 0) uniform globalUBO
{
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 directionalLightMVP;
    vec4 dirLightPos;
    vec4 cameraPosition;
    vec4 viewportDimension;
    vec4 pointLightPositions[MAX_POINT_LIGHT];
    vec4 pointlightIntensities[MAX_POINT_LIGHT];
    vec4 pointLightColors[MAX_POINT_LIGHT];
    vec4 directionalLightIntensity;
    mat4 shadowMatrices[MAX_POINT_LIGHT][6];
};

layout (push_constant) uniform pointLightIndex
{   
	layout (offset = 64)
    vec4 index;
};

layout(location = 0) out vec4 FragPos; // FragPos from GS (output per emitvertex)

void main()
{
    for(int face = 0; face < 6; ++face)
    {
        gl_Layer = face; // built-in variable that specifies to which face we render.
        for(int i = 0; i < 3; ++i) // for each triangle vertex
        {
            FragPos = gl_in[i].gl_Position;
            gl_Position = shadowMatrices[int(index.x)][face] * FragPos;
            EmitVertex();
        }    
        EndPrimitive();
    }
} 