#version 440 core

#define MAX_POINT_LIGHTS 20
#define MAX_POINT_SHADOWS 5

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

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

// -----------------------

uniform float shininess = 128.0;
uniform vec3 camera_pos;

uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D position_texture;
uniform sampler2D model_normal_texture;
uniform sampler2D texture_normal_texture;
uniform sampler2D ssao_texture;

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
	variance = min( 1.0f, max( 0.0001, variance) );

	float d = d_recv.x - d_blocker;
	p_max = variance / (variance + d * d);

	return ReduceLightBleeding(p_max, 1.0);
}

 
float CheckDirectionalShadow(float bias, vec3 lightpos, vec3 FragPos)
{		
	vec3 projCoords = FragPosLightSpace.xyz;// / FragPosLightSpace.w;
	vec3 light_to_frag = FragPos - lightpos;
	projCoords = projCoords * 0.5 + 0.5;

	vec4 closest_depth_r = textureGather(directional_shadow_depth_map, projCoords.xy, 0);
	vec4 closest_depth_g = textureGather(directional_shadow_depth_map, projCoords.xy, 1);

	vec2 avg_m = vec2((closest_depth_r.x + closest_depth_r.y + closest_depth_r.z + closest_depth_r.w) / 4.0, 
					  (closest_depth_g.x + closest_depth_g.y + closest_depth_g.z + closest_depth_g.w) / 4.0);

	return 1.0 - chebyshevUpperBound(length(FragPos - lightpos) / far, avg_m);
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

	return (ambient * 0.005 + diffuse * 0.8 * (1.0 - directional_shadow) + specular * 1.0 * (1.0 - directional_shadow)) * ambient_occlusion;
}


float CheckPointShadow(vec3 point_light_pos,int index, float bias, vec3 FragPos)
{
	vec3 light_to_frag = FragPos - point_light_pos;

	float current_depth = length(light_to_frag) / far;

	vec2 static_depth = texture(point_shadow_depth_map[index], normalize(light_to_frag)).rg;
	vec2 dynamic_depth = texture(point_shadow_depth_map[index], normalize(light_to_frag)).ba;

	vec2 closest_depth = static_depth.r < dynamic_depth.r ? static_depth : dynamic_depth;

	return (1.0 - chebyshevUpperBound(current_depth, closest_depth));
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

	return (ambient * 0.0005 + diffuse * attenuation * 0.7 * (1.0 - point_shadow) + specular * attenuation * 0.9 * (1.0 - point_shadow));
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

void main()
{
	vec3 color = vec3(0.0);

	vec3 diffuse_texture_color = texture(diffuse_texture, TexCoords).rgb;
	float specular_texture_value = texture(diffuse_texture, TexCoords).a;
	FragPos = texture(position_texture, TexCoords).xyz;
	vec3 normal_from_texture = texture(texture_normal_texture, TexCoords).xyz * 2.0 - 1.0;
	ambient_occlusion = 1.0;//texture(ssao_texture, TexCoords).r;

	vec3 normal = TangentToWorldNormal(texture(model_normal_texture, TexCoords).xyz, normalize(normal_from_texture));
	FragPosLightSpace = directional_light_space_matrix * vec4(FragPos,1.0);

	vec3 viewdir = vec3(0.0);
	vec3 lightdir = vec3(0.0);

	if(directional_lighting_enabled)
	{  
		lightdir = directionalLight.position;
		viewdir = camera_pos - FragPos;

		color += max(CalcDirectional(diffuse_texture_color, specular_texture_value, normal, viewdir, lightdir, FragPos),vec3(0.0));
	}

	for(int i=0; i<N_POINT; ++i)
	{
		lightdir = pointLights[i].position - FragPos;
		viewdir = camera_pos - FragPos;

		color += max(CalcPoint(pointLights[i],diffuse_texture_color, specular_texture_value, normal, FragPos, viewdir, lightdir, i),vec3(0.0));
	}

	//color = diffuse_texture_color;
	FragColor = vec4(color, 1.0);
	BrightColor = vec4(BloomThresholdFilter(color, 5.0), 1.0);
}