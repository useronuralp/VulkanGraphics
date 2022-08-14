#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;

layout(binding = 0) uniform OwnModelMat
{
    mat4 matrix;
} Model;


layout(binding = 1) uniform depthMVP
{
    mat4 matrix;
} MVP;



out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main()
{
	gl_Position = MVP.matrix * Model.matrix * vec4(a_Position, 1.0);
}