#version 450
// INs
layout(location = 0) in vec3 a_Position;

layout(binding = 0) uniform ModelMatrix
{
    mat4 ModelMat;
} Model;

layout(binding = 1) uniform ViewMatrix
{
    mat4 ViewMat;
} View;

layout(binding = 2) uniform ProjectionMatrix
{
    mat4 ProjMat;
} Proj;

void main()
{
	gl_Position = (Proj.ProjMat * View.ViewMat * Model.ModelMat) * vec4(a_Position, 1.0);
}