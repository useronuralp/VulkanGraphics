#version 450 core

layout(location = 0) in vec3 a_Position;

layout(binding = 0) uniform ViewMatrix
{
    mat4 ViewMat;
} View;

layout(binding = 1) uniform ProjectionMatrix
{
    mat4 ProjMat;
} Proj;

layout(location = 0) out vec3 v_UV;
mat4 modelMat = mat4(1.0);
void main()
{
	v_UV = a_Position;
	v_UV.z = -v_UV.z;
	gl_Position = Proj.ProjMat * View.ViewMat * modelMat * vec4(a_Position, 1.0);
}