#version 450 core

layout(location = 0) out vec4 FragColor;

//layout(binding = 0) in sampler2D bloom;

void main()
{		
	vec3 color = vec3(1.3, 5, 1.3);
   FragColor = vec4(color, 1.0);
}  