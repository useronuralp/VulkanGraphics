#version 450 core

layout(location = 0) in vec3 a_Position;

layout(binding = 0) uniform globalUBO
{
	mat4 viewMatrix;
    mat4 ProjMat;
};

layout (push_constant) uniform MM
{
	mat4 modelMat;
};

void main()
{
	gl_Position = ProjMat * viewMatrix * modelMat * vec4(a_Position, 1.0);
}