#version 440 core

#define MAX_POINT_LIGHTS 20
#define MAX_POINT_SHADOWS 3

layout (location = 0) out vec3 FragColor;
layout (location = 1) out vec3 BrightColor;

struct DirectionalLight {
	vec3 position;
	vec3 direction;
	vec3 color;
};

struct PointLight {
	vec3 position;
	
	vec3 color;
		
	float kc;
	float kl;
	float kq;
};

// ------------------------

vec3 FragPos;
vec4 FragPosLightSpace;
in vec2 TexCoords;

// -----------------------

uniform DirectionalLight directionalLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];

uniform mat4 directional_light_space_matrix;
uniform bool directional_lighting_enabled;

uniform int N_POINT;

// -----------------------

uniform float near;
uniform float far;
uniform int use_ssao;

// -----------------------

// Shadow Textures -------------------------------
uniform sampler2D directional_shadow_depth_map;
uniform samplerCube point_shadow_depth_map[MAX_POINT_SHADOWS];
// -----------------------

uniform vec3 camera_pos;

uniform sampler2D diffuse_texture; //albedo
uniform sampler2D model_normal_texture;
uniform sampler2D texture_normal_texture;
uniform sampler2D position_texture;
uniform sampler2D ssao_texture;
uniform sampler2D mor_texture; // Metallic, Occlusion, Roughness

// -----------------------

const float PI = 3.14159265359;

// -----------------------

vec3 fresnelSchlick(float cosine, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosine, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float R) // Normal, Halfway, Roughness
{
	float a = R * R;
	float a2 = a * a;

	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float num = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return num/denom;
}

float GeometrySchlickGGX(float NdotV, float R)
{
	float r = (R + 1.0);
	float k = r * r / 8.0;

	float num = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float R)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, R);
	float ggx1 = GeometrySchlickGGX(NdotL, R);

	return ggx1 * ggx2;
}

vec3 TangentToWorldNormal(vec3 model_normal, vec3 normal_from_texture)
{
	vec3 Q1  = dFdx(FragPos);
    vec3 Q2  = dFdy(FragPos);
    vec2 st1 = dFdx(TexCoords);
    vec2 st2 = dFdy(TexCoords);

    vec3 N   = normalize(model_normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

	return normalize(TBN * normal_from_texture);
}
float fabs(float v)
{
	return v < 0 ? -v : v;
}

vec3 BloomThresholdFilter(vec3 in_color, float threshold)
{
	float intensity = dot(in_color, vec3(0.2126, 0.7152, 0.0722));
	if(intensity > threshold)
	{
		return in_color;
	}
	else
	{
		return vec3(0.0);
	}
}

float linstep(float mi, float ma, float v)
{
	return clamp ((v - mi)/(ma - mi), 0, 1);
}

float ReduceLightBleeding(float p_max, float Amount) 
{
	return linstep(Amount, 1, p_max); 
} 


float chebyshevUpperBound(float d_blocker, vec2 d_recv)
{
	float p_max;

	if(d_blocker <= d_recv.x)
	{
		return 1.0;
	}

	float variance = d_recv.y - (d_recv.x * d_recv.x);
	variance = min(1.0f, max( 0.00001, variance) );

	float d = d_recv.x - d_blocker;
	p_max = variance / (variance + d * d);

	return ReduceLightBleeding(p_max, 1.0);
}


float CheckDirectionalShadow(float bias, vec3 lightpos, vec3 FragPos)
{		
	vec3 projCoords = FragPosLightSpace.xyz;// / FragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	vec2 closest_depth = texture(directional_shadow_depth_map, projCoords.xy, 0).rg;

	//vec4 closest_depth_r = textureGather(directional_shadow_depth_map, projCoords.xy, 0);
	//vec4 closest_depth_g = textureGather(directional_shadow_depth_map, projCoords.xy, 1);

	//vec2 avg_m = vec2((closest_depth_r.x + closest_depth_r.y + closest_depth_r.z + closest_depth_r.w) / 4.0, 
					  //(closest_depth_g.x + closest_depth_g.y + closest_depth_g.z + closest_depth_g.w) / 4.0);

	return chebyshevUpperBound(length(FragPos - lightpos) / far, closest_depth);
}

vec3 CalcDirectional(vec3 FPos, vec3 V, vec3 N, vec3 mor, vec3 F0, vec3 albedo)
{
	vec3 Lo = vec3(0.0);

	vec3 L = normalize(directionalLight.position);
	vec3 halfway = normalize(V + L);

	vec3 radiance = directionalLight.color;

	float NDF = DistributionGGX(N, halfway, mor.b);
	float G = GeometrySmith(N, V, L, mor.b);
	vec3 F = fresnelSchlick(max(dot(halfway, V), 0.0), F0);

	vec3 numerator = NDF * G * F;
	float denominator = 4 * max(dot(N, V), 0.001) * max(dot(N, L), 0.001) + 0.0001;
	vec3 specular = numerator / denominator;

	vec3 kd = vec3(1.0) - F; // kD = 1.0 - kS
		
	kd *= 1.0 - mor.r;

	float NdotL = max(dot(N, L), 0.0);
	Lo = (kd * albedo / PI + specular) * radiance * NdotL;

	return Lo;
}

float samplePointShadow(float bias, float current_depth, vec3 light_to_frag_direction,int shadow_map_index)
{
	float s = 0.0;

	vec2 static_depth = texture(point_shadow_depth_map[shadow_map_index], light_to_frag_direction).rg;
	vec2 dynamic_depth = texture(point_shadow_depth_map[shadow_map_index], light_to_frag_direction).ba;
	
	vec2 closest_depth = static_depth.x < dynamic_depth.x ? static_depth : dynamic_depth; 

	s = chebyshevUpperBound(current_depth, closest_depth);

	return s;
}

float CheckPointShadow(vec3 point_light_pos,int index, float bias, vec3 FragPos)
{
	float shadow = 0.0;

	vec3 light_to_frag = FragPos - point_light_pos;
	float current_depth = length(light_to_frag) / far;

	shadow = samplePointShadow(bias,current_depth, normalize(light_to_frag), index);

	return shadow;
}

vec3 CalcPoint(int index, vec3 FPos, vec3 V, vec3 N, vec3 mor, vec3 F0, vec3 albedo) 
{
	vec3 Lo = vec3(0.0);
	
	vec3 L = (pointLights[index].position - FPos);
	vec3 halfway = normalize(V + normalize(L));

	float dist = length(L);
	float attenuation = 1.0 / (dist * dist);
	vec3 radiance = pointLights[index].color * attenuation;

	float NDF = DistributionGGX(N, halfway, mor.b);
	float G = GeometrySmith(N, V, normalize(L), mor.b);
	vec3 F = fresnelSchlick(max(dot(halfway, V), 0.0), F0);

	vec3 numerator = NDF * G * F;
	float denominator = 4 * max(dot(N, V), 0.001) * max(dot(N, normalize(L)), 0.001) + 0.0001;
	vec3 specular = numerator / denominator;

	vec3 kd = vec3(1.0) - F; // kD = 1.0 - kS
		
	kd *= 1.0 - mor.r;

	float NdotL = max(dot(N, normalize(L)), 0.0);
	Lo = (kd * albedo / PI + specular) * radiance * NdotL;
	
	return Lo;
}


void main()
{
	vec3 color = vec3(0.0);

	vec3 albedo = pow(texture(diffuse_texture, TexCoords).rgb, vec3(2.2));
	FragPos = texture(position_texture, TexCoords).xyz;
	vec3 normal_from_texture = normalize(texture(texture_normal_texture, TexCoords).xyz * 2.0 - 1.0);
	vec3 model_normal = texture(model_normal_texture, TexCoords).rgb;
	vec3 mor = texture(mor_texture, TexCoords).rgb;
	float ssao = 1.0;

	if(use_ssao == 1)
	{
		ssao = texture(ssao_texture, TexCoords).r;
	}

	
	FragPosLightSpace = directional_light_space_matrix * vec4(FragPos,1.0);

	vec3 viewdir = normalize(camera_pos - FragPos);
	vec3 normal = TangentToWorldNormal(model_normal, normal_from_texture);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, mor.r);
	vec3 Lo = vec3(0.0);

	if(directional_lighting_enabled)
	{
		float bias = 0.000001;
		//bias = max(0.01 * (1.0 - dot(normalize(normal), normalize(-directionalLight.direction))), 0.001);
		float directional_shadow = CheckDirectionalShadow(bias, directionalLight.position, FragPos);

		Lo = CalcDirectional(FragPos, viewdir, normal, mor, F0, albedo);
		vec3 ambient = albedo * (1.0 - mor.g) * ssao;
		
		color += max((pow(ambient, vec3(0.7)) + Lo * directional_shadow), vec3(0.0));
	}

	for(int i=0; i<N_POINT; i++)
	{
		float point_shadow = 0.0;
		
		float bias = 0.0001;
		//bias = max(0.01 * (1 - dot(normal, normalize(lightdir))), 0.001);

		if(i < MAX_POINT_SHADOWS) // This could be slow
		{
			point_shadow = CheckPointShadow(pointLights[i].position, i, bias, FragPos);
		}
		
		Lo = CalcPoint(i, FragPos, viewdir, normal, mor, F0, albedo);
		vec3 ambient = albedo * (1.0 - mor.g) * ssao;
		
		color += max((ambient * (0.03) + Lo * (point_shadow)), vec3(0.0));
	}

	FragColor = color;
	BrightColor = BloomThresholdFilter(color, 1.0);
}