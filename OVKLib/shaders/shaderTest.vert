#version 450

// INs
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;
layout(location = 5) in mat4 modelMatrxInstanced;

//OUTs
layout(location = 0) out vec3 v_Pos;
layout(location = 1) out vec2 v_UV;
layout(location = 2) out vec3 v_Normal;
layout(location = 3) out vec3 v_DirLightPos;
layout(location = 4) out vec3 v_CamPos;
layout(location = 5) out mat4 v_ViewMatrix;


layout(set = 0, binding = 0) uniform SausageBuffer
{
    mat4  viewMat;
    mat4  projMat;
    vec4  dirLightPos;
    vec4  cameraPos;
};

void main()
{
    v_UV                = a_UV;
    v_Pos               = vec3(modelMatrxInstanced * vec4(a_Position, 1.0));
    v_Normal            = mat3(modelMatrxInstanced) * a_Normal;   
    v_CamPos            = cameraPos.xyz;
    v_DirLightPos       = dirLightPos.xyz;
    v_ViewMatrix        = viewMat;
    gl_Position         = projMat * viewMat * modelMatrxInstanced * vec4(a_Position, 1.0);
}