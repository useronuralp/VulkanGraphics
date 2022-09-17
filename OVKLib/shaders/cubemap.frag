#version 450 core

layout(location = 0) in vec3 v_UV;

layout(binding = 1) uniform samplerCube u_Skybox;

layout(location = 0) out vec4 FragColor;


void main()
{
	FragColor = texture(u_Skybox, v_UV);
}