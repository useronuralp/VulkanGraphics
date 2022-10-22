#version 450

// INs
layout(location = 0) in vec3 a_Position;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 viewMat;
    mat4 projMat;
};

layout( push_constant ) uniform modelMat
{
	mat4 modelMatrix;
};

void main()
{
    gl_Position = projMat * viewMat * modelMatrix * vec4(a_Position, 1.0);
}