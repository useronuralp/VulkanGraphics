#version 450 core

layout(location = 0) in vec3 a_Position;


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