#version 450 core

layout(location = 0) in vec3 a_Position;


layout(set = 0, binding = 0) uniform UBO
{
    mat4 viewMat;
    mat4 projMat;
    mat4 lightMVP;
    vec4 dirLightPosition;
    vec4 cameraPos;
};

layout( push_constant ) uniform modelMat
{
	mat4 modelMatrix;
};

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main()
{
	gl_Position = lightMVP * modelMatrix * vec4(a_Position, 1.0);
}