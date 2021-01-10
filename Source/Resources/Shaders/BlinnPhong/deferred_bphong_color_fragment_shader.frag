#version 440 core

#define MAX_POINT_LIGHTS 20
#define MAX_POINT_SHADOWS 3
#define MIN_VARIANCE 0.00001
#define LIGHT_BLEED_REDUCTION_AMOUNT 1.0

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

struct SpotLight{
	vec3 position;
	vec3 direction;
	
	vec3 color;
	
	float innercutoff;
	float outercutoff;
	
	float kc;
	float kl;
	float kq;
};
// ------------------------

vec3 FragPos;
vec4 FragPosLightSpace;
in vec2 TexCoords;
float ambient_occlusion = 1.0;

// Shadow Textures -------------------------------
uniform sampler2D directional_shadow_depth_map;
uniform samplerCube point_shadow_depth_map[MAX_POINT_SHADOWS];
// -----------------------

uniform DirectionalLight directionalLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS]; // I forgot to mentiom 'uniform' and kept wandering for an hour why the screen would render black.
uniform mat4 directional_light_space_matrix;
uniform bool directional_lighting_enabled;
uniform int N_POINT;
uniform float near;
uniform float far;
uniform mat4 inv_projection;
uniform mat4 inv_view;
uniform int use_ssao;
uniform float positive_exponent;
uniform float negative_exponent;

// -----------------------

uniform float shininess = 128.0;
uniform vec3 camera_pos;

uniform sampler2D diffuse_texture;
uniform sampler2D depth_texture;
uniform sampler2D normal_texture;
uniform sampler2D ssao_texture;

float linstep(float mi, float ma, float v)
{
	return clamp ((v - mi)/(ma - mi), 0, 1);
}

float ReduceLightBleeding(float p_max, float Amount) 
{
	return linstep(Amount, 1, p_max); 
} 

vec2 warpDepth(float depth)
{
    depth = 2.0f * depth - 1.0f;
    float pos = exp(positive_exponent * depth);
    float neg = -exp(-negative_exponent * depth);
    vec2 wDepth = vec2(pos, neg);
    return wDepth;
}

float chebyshevUpperBound(vec2 moments, float mean, float minVariance, int light_type) // light_type : 0 - directional, 1 - point
{
	float shadow = 1.0;
    if(mean <= moments.x)
    {
        return 1.0;
    }
    else
    {
        float variance = moments.y - (moments.x * moments.x);
        variance = max(variance, minVariance);
        float d = mean - moments.x;
        shadow = variance / (variance + (d * d));
		if(light_type == 1)
		{
			return ReduceLightBleeding(shadow, LIGHT_BLEED_REDUCTION_AMOUNT);
		}
        else
			return shadow;
    }
}


 
float CheckDirectionalShadow(float bias, vec3 lightpos, vec3 FragPos)
{	vec3 projCoords = FragPosLightSpace.xyz;// / FragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	vec4 closest_depth = texture(directional_shadow_depth_map, projCoords.xy, 0).rgba;

	//vec4 closest_depth_r = textureGather(directional_shadow_depth_map, projCoords.xy, 0);
	//vec4 closest_depth_g = textureGather(directional_shadow_depth_map, projCoords.xy, 1);

	//vec2 avg_m = vec2((closest_depth_r.x + closest_depth_r.y + closest_depth_r.z + closest_depth_r.w) / 4.0, 
					  //(closest_depth_g.x + closest_depth_g.y + closest_depth_g.z + closest_depth_g.w) / 4.0);

	//float cup =  chebyshevUpperBound(length(FragPos - lightpos) / far, closest_depth);

	vec2 pos_moments = vec2(closest_depth.x, closest_depth.z);
	vec2 neg_moments = vec2(closest_depth.y, closest_depth.w);

	vec2 wDepth = warpDepth(length(FragPos - lightpos) / far);

	vec2 depthScale = 0.01 * vec2(positive_exponent, negative_exponent) * wDepth;
	vec2 minVariance = depthScale * depthScale;
	float pos_result = chebyshevUpperBound(pos_moments, wDepth.x, minVariance.x, 0);
	float neg_result = chebyshevUpperBound(neg_moments, wDepth.y, minVariance.y, 0);

	return min(pos_result, neg_result);
}

vec3 CalcDirectional(vec3 diffuse_texture_color,float specular_texture_value,vec3 normal,vec3 viewdir, vec3 lightdir, vec3 FragPos)
{
	float bias = 0.00001;
	bias = max(0.001 * (1.0 - dot(normalize(normal), normalize(-directionalLight.direction))), 0.0001);

	float directional_shadow = 0.0;
	directional_shadow = CheckDirectionalShadow(bias, directionalLight.position, FragPos);

	vec3 ambient = directionalLight.color * diffuse_texture_color * ambient_occlusion; //ambient
	
	float diff = max(dot(normalize(normal), normalize(lightdir)),0.0);
	vec3 diffuse = directionalLight.color * diff * diffuse_texture_color; //diffuse
	
	vec3 halfway = normalize(normalize(lightdir) + normalize(viewdir));
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = directionalLight.color * spec * specular_texture_value; // specular

	return (ambient * 0.0025 + diffuse * 0.8 * (directional_shadow) + specular * 1.0 * (directional_shadow)) * ambient_occlusion;
}


float samplePointShadow(float bias, float current_depth, vec3 light_to_frag_direction,int shadow_map_index)
{
	float s = 0.0;

	vec2 static_depth = texture(point_shadow_depth_map[shadow_map_index], light_to_frag_direction).rg;
	vec2 dynamic_depth = texture(point_shadow_depth_map[shadow_map_index], light_to_frag_direction).ba;
	
	vec2 closest_depth = static_depth.x < dynamic_depth.x ? static_depth : dynamic_depth; 

	s = chebyshevUpperBound(closest_depth, current_depth, MIN_VARIANCE, 1);

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

vec3 CalcPoint(PointLight pl,vec3 diffuse_texture_color, float specular_texture_value,vec3 normal, vec3 FragPos ,vec3 viewdir, vec3 lightdir,int index)
{
	float bias = 0.0001;
	//bias = max(0.01 * (1 - dot(normal, normalize(lightdir))), 0.001);

	float point_shadow = 0.0;
	if(index < MAX_POINT_SHADOWS)
	{
		point_shadow = CheckPointShadow(pl.position, index, bias, FragPos);
	}
	
	vec3 ambient = pl.color * diffuse_texture_color * ambient_occlusion; //ambient
	
	float diff = max(dot(normalize(normal), normalize(lightdir)), 0.0);
	vec3 diffuse = pl.color * diff * diffuse_texture_color ; // diffuse
	
	vec3 halfway = normalize(lightdir + viewdir);
	float spec = pow(max(dot(normalize(normal), normalize(halfway)),0.0),shininess);
	vec3 specular = pl.color * spec * vec3(specular_texture_value); // specular
	
	float distance = length(pl.position - FragPos);
	float attenuation = 1.0/(pl.kc + pl.kl * distance + pl.kq * distance * distance);

	return (ambient * 0.0005 + diffuse * attenuation * 0.7 * (point_shadow) + specular * attenuation * 0.9 * (point_shadow));
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

vec3 ScreenToWorldPos()
{
	float z = texture(depth_texture, TexCoords).r * 2.0 - 1.0;

	vec4 clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
	vec4 viewSpacePosition = inv_projection * clipSpacePosition;

	viewSpacePosition /= viewSpacePosition.w;

	return (inv_view * viewSpacePosition).xyz;
}

void main()
{
	vec3 color = vec3(0.0);

	vec3 diffuse_texture_color = texture(diffuse_texture, TexCoords).rgb;
	float specular_texture_value = texture(diffuse_texture, TexCoords).a;
	FragPos = ScreenToWorldPos();
	vec3 normal = normalize(texture(normal_texture, TexCoords).rgb * 2.0 - 1.0);
	float ssao = 1.0;

	if(use_ssao == 1)
	{
		ssao = texture(ssao_texture, TexCoords).r;
	}

	FragPosLightSpace = directional_light_space_matrix * vec4(FragPos,1.0);

	vec3 viewdir = vec3(0.0);
	vec3 lightdir = vec3(0.0);

	if(directional_lighting_enabled)
	{  
		lightdir = directionalLight.position;
		viewdir = camera_pos - FragPos;

		color += max(CalcDirectional(diffuse_texture_color, specular_texture_value, normal, viewdir, lightdir, FragPos),vec3(0.0)) * ssao;
	}

	for(int i=0; i<N_POINT; ++i)
	{
		lightdir = pointLights[i].position - FragPos;
		viewdir = camera_pos - FragPos;

		color += max(CalcPoint(pointLights[i],diffuse_texture_color, specular_texture_value, normal, FragPos, viewdir, lightdir, i),vec3(0.0)) * ssao;
	}

	//color = diffuse_texture_color;
	FragColor = color;
	BrightColor = BloomThresholdFilter(color, 6.0);
}