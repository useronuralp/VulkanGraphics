#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_UV;

layout(location = 0) out vec2 v_UV;
layout(location = 1) out vec4 v_FragPosLightSpace;

layout(binding = 0) uniform ModelMat
{
    mat4 Matrix;
} Model;

layout(binding = 1) uniform ProjMatrix
{
    mat4 Matrix;
} Proj;

layout(binding = 2) uniform viewMatrix
{
    mat4 Matrix;
} View;

layout(binding = 3) uniform lightMVP
{
    mat4 Mat;
}MVP;

const mat4 bias = mat4( 
  0.5, 0.0, 0.0, 0.0,
  0.0, 0.5, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.5, 0.5, 0.0, 1.0 );

void main()
{
	v_UV = a_UV;
	gl_Position = Proj.Matrix * View.Matrix * Model.Matrix * vec4(a_Position, 1.0);

    v_FragPosLightSpace = bias * MVP.Mat * vec4(a_Position, 1.0);

    //v_UV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	//gl_Position = vec4(v_UV * 2.0f - 1.0f, 0.0f, 1.0f);
}