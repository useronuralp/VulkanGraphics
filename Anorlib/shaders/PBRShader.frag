#version 430 core

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
layout(binding = 8) uniform sampler2D u_RoughnessMetallicSampler;


layout(location = 0) out vec4 FragColor;
const float PI = 3.14159265359;

vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(u_NormalSampler, v_UV).xyz * 2.0 - 1.0;
    
    vec3 Q1  = dFdx(v_Pos);
    vec3 Q2  = dFdy(v_Pos);
    vec2 st1 = dFdx(v_UV);
    vec2 st2 = dFdy(v_UV);

    vec3 N   = normalize(v_Normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return nom / denom;
}
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{		
    vec3 albedo = pow(texture(u_DiffuseSampler, v_UV).rgb, vec3(2.2));
    vec3 roughnessMetallicTex = texture(u_RoughnessMetallicSampler, v_UV).rgb;
    float ao  = roughnessMetallicTex.r;
    float roughness = roughnessMetallicTex.g;
    float metallic = roughnessMetallicTex.b;

    vec3 lightPositions[4] = vec3[4](vec3(0.0, 50.0f, 1.0), vec3(0.0, 0.0f, 50.0), vec3(0.0, 0.0f, -50.0), vec3(0.0, -50.0f, 0.0));
    vec3 lightColors[4] = vec3[4](vec3(1.0, 1.0, 1.0), vec3(1.0, 0.0f, 0.0), vec3(0.0, 1.0f, 0.0), vec3(0.0, 0.0f, 1.0));
    vec3 directionalLightPos = vec3(0.0f, 1.0f, 0.0f);
    vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);

    vec3 N = texture(u_NormalSampler, v_UV).rgb;
    N = N * 2.0 - 1.0;
    N = normalize(v_TBN * N);

    //vec3 N = normalize(v_Normal);

    vec3 V = normalize(camPos.pos - v_Pos);
    
    vec3 F0 = vec3(0.04f); 
    F0 = mix(F0, albedo, metallic);

    // reflectance equation 
    vec3 Lo = vec3(0.0);

        vec3 L = normalize(pointLightPos.pos - v_Pos);
        vec3 H = normalize(V + L);

        float distance    = length(pointLightPos.pos - v_Pos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance     = lightColors[0]  * attenuation * 100.0;        
        
        // cook-torrance brdf
        float NDF  = DistributionGGX(N, H, roughness);        
        float G    = GeometrySmith(N, V, L, roughness);      
        vec3  F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
        
        vec3  numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular     = numerator / denominator;  

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	
            
        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * vec3(albedo) / PI + specular) * radiance * NdotL; 


    vec3 ambient = vec3(0.01f) * vec3(albedo) * ao;
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));  

    FragColor = vec4(color, 1.0);
}  