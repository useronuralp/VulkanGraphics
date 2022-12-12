#version 450 core

layout(location = 0) out vec4 FragColor;

//layout(binding = 0) in sampler2D bloom;

void main()
{		
	vec3 color = vec3(6.8, 1.5, 0.6);
   FragColor = vec4(color, 1.0);
}  