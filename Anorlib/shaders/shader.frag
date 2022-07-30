#version 450

layout(location = 0) in vec3 v_Pos;
layout(location = 1) in vec2 v_UV;
layout(location = 2) in vec3 v_Normal;
layout(location = 3) in mat3 v_TBN;

layout(binding = 3) uniform DirectionalLightPosition
{
    vec3 pos;
} directionalLightPos;


layout(binding = 4) uniform CameraPosition
{
    vec3 pos;
} camPos;

layout(binding = 5) uniform PointLightPosition
{
    vec3 pos;
} pointLightPos;

layout(binding = 6) uniform sampler2D u_DiffuseSampler;
layout(binding = 7) uniform sampler2D u_NormalSampler;
layout(binding = 8) uniform sampler2D u_SpecularSampler;



layout(location = 0) out vec4 outColor;

vec3 CalcDirLight(vec3 directionalLightPos, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(directionalLightPos - (v_TBN * v_Pos));

    // diffuse shading
    float diff = max(dot(lightDir, normal), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    // combine results
    vec3 ambient = 0.1 * vec3(texture(u_DiffuseSampler, v_UV));
    vec3 diffuse = diff * vec3(texture(u_DiffuseSampler, v_UV));
    vec3 specular = spec * vec3(texture(u_SpecularSampler, v_UV));
    return (ambient + diffuse + specular);
}

// calculates the color when using a point light.
vec3 CalcPointLight(vec3 lightPosition, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    float constant = 1.0;
    float linear = 0.1;
    float quadratic = 0.032;

    vec3 lightDir = normalize(lightPosition - fragPos);
    vec3 lightColor = vec3(1.0, 0.0, 0.0);
    // diffuse shading
    float diff = max(dot(lightDir, normal), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    // attenuation
    float distance = length(lightPosition - fragPos);
    float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));  
    
    // combine results
    vec3 ambient = 0.1 * vec3(texture(u_DiffuseSampler, v_UV)) * lightColor;
    vec3 diffuse = diff * vec3(texture(u_DiffuseSampler, v_UV)) * lightColor;
    vec3 specular = spec * vec3(texture(u_SpecularSampler, v_UV)) * lightColor;
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

void main()
{
    // obtain normal from normal map in range [0,1]
    vec3 normal = texture(u_NormalSampler, v_UV).rgb;
    //transform normal vector to range [-1,1]
    normal = normalize(normal * 2.0 - 1.0);  // this normal is in tangent space
   
    vec3 FinalColor = vec3(0.0);
    vec3 viewDir = (v_TBN * camPos.pos) - (v_TBN * v_Pos);
    FinalColor += CalcDirLight((v_TBN * directionalLightPos.pos), normal, viewDir);
    FinalColor += CalcPointLight((v_TBN * pointLightPos.pos), normal, v_TBN * v_Pos, viewDir);

    outColor = vec4(FinalColor, 1.0);


}