#version 450

// INs
layout(location = 0) in vec3 a_Position;

layout(set = 0, binding = 0) uniform globalUBO
{
    mat4 viewMatrix;
    mat4 projMatrix;
};

layout( push_constant ) uniform modelMat
{
	mat4 modelMatrix;
    vec4 color;
};

layout(location = 0) out vec3 v_Color;

void main()
{
    v_Color = color.xyz;
    gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(a_Position, 1.0);
}