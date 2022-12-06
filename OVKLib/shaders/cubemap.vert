#version 450 core

layout(location = 0) in vec3 a_Position;

layout(binding = 0) uniform globalUBO
{
    mat4 ProjMat;
};

layout( push_constant ) uniform View
{
	mat4 viewMatrix;
};

const mat4 scale = mat4( 
  4.0, 0.0, 0.0, 0.0,
  0.0, 4.0, 0.0, 0.0,
  0.0, 0.0, 4.0, 0.0,
  0.0, 0.0, 0.0, 1.0 );

layout(location = 0) out vec3 v_UV;
mat4 modelMat = mat4(1.0);
void main()
{
	//modelMat = modelMat * scale;
	v_UV = a_Position;
	v_UV.z = -v_UV.z;
	gl_Position = ProjMat * viewMatrix * vec4(a_Position, 1.0);
}