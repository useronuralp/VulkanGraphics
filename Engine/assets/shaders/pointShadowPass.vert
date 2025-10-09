#version 450 core

layout(location = 0) in vec3 a_Position;



// TO DO : TIDY UP THE PUSH_CONSTANTS
layout( push_constant ) uniform mvp
{
	mat4 modelMat;
};

void main()
{
	gl_Position = modelMat * vec4(a_Position, 1.0);
}