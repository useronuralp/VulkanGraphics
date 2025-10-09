#version 450 core

layout(location = 0) in vec4 FragPos;

layout( push_constant ) uniform mvp
{
	layout (offset = 80) 
	vec4 lightPos;
	vec4 farPlane;
};

void main()
{
	float lightDistance = length(FragPos.xyz - lightPos.xyz);
	
	lightDistance = lightDistance / farPlane.x;

	gl_FragDepth = lightDistance;
}