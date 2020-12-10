#version 440 core

#define MAX_POINT_LIGHTS 3

in vec4 FragPos;

uniform samplerCube point_shadow_framebuffer_depth_color_texture_static[MAX_POINT_LIGHTS];
uniform samplerCube point_shadow_framebuffer_depth_color_texture_dynamic[MAX_POINT_LIGHTS];
uniform float far;
uniform vec3 lightPos;

out float penumbra_value;

float fabs(float v)
{
	return v < 0 ? -v : v;
}

vec3 restrictOffset(vec3 cubemap_direction, vec2 offset)
{
	vec3 face = vec3(0.0);
	float x = cubemap_direction.x;
	float y = cubemap_direction.y;
	float z = cubemap_direction.z;


	if(x>0 && fabs(x) >= fabs(y) && fabs(x) >= fabs(z))
	{
		face = vec3(1.0, offset.x, offset.y); // positive x
	}
	else if(x<0 && fabs(x)>= fabs(y) && fabs(x) >= fabs(z))
	{
		face = vec3(-1.0, offset.x, offset.y); // negative x
	}
	else if(y>0 && fabs(y)>= fabs(x) && fabs(y) >= fabs(z))
	{
		face = vec3(offset.x, 1.0, offset.y); // positive y
	}
	else if(y<0 && fabs(y)>= fabs(x) && fabs(y) >= fabs(z))
	{
		face = vec3(offset.x, -1.0, offset.y); // negative y
	}
	else if(z>0 && fabs(z)>= fabs(y) && fabs(z) >= fabs(x))
	{
		face = vec3(offset.x, offset.y, 1.0); // positive z
	}
	else if(z<0 && fabs(z)>= fabs(y) && fabs(z) >= fabs(x))
	{
		face = vec3(offset.x, offset.y, -1.0); // negative z
	}

	return face;
}

float InterleavedGradientNoise(vec2 fPos)
{
	vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
	return fract(magic.z * fract(dot(fPos, magic.xy))); 
}

vec2 VogelDiskSample(int sampleIndex, int sampleCount, float phi)
{
	float goldenAngle = 2.4f;

	float r = sqrt(sampleIndex + 0.5f) / sqrt(sampleCount);
	float theta = sampleIndex * goldenAngle + phi;

	float s = sin(theta);
	float c = cos(theta);

	return vec2(r * c, r * s);
}

float AvgBlockersDepthToPenumbra(float z_shadowMapView, float avgBlockersDepth)
{
	float penumbra = (z_shadowMapView - avgBlockersDepth) / avgBlockersDepth;
	penumbra *= penumbra;
	return clamp(penumbra * 10.0f, 0.0, 1.0);
}

float penumbra(float gradientNoise, vec3 fragDir, float z_shadowMapView, int samplesCount, int shadowMapIndex)
{
	float p;

	float avg_blockerDepth = 0.0;
	float blockersCount = 0.0;

	for(int i=0; i<samplesCount; i++)
	{
		vec2 offset = VogelDiskSample(i, samplesCount, gradientNoise);
		vec3 sampleDir = restrictOffset(fragDir, offset) * 0.01;

		float static_depth = texture(point_shadow_framebuffer_depth_color_texture_static[shadowMapIndex], sampleDir).r;
		float dynamic_depth = texture(point_shadow_framebuffer_depth_color_texture_dynamic[shadowMapIndex], sampleDir).r;

		float sampleDepth = static_depth < dynamic_depth ? static_depth : dynamic_depth;

		if(sampleDepth < z_shadowMapView)
		{
			avg_blockerDepth += sampleDepth;
			blockersCount += 1.0f;
		}
	}

	if(blockersCount > 0.0)
	{
		avg_blockerDepth /= blockersCount;
		return AvgBlockersDepthToPenumbra(z_shadowMapView, avg_blockerDepth);
	}
	else
	{
		return 0.0;
	}

	return p;
}

float CheckPointShadow(vec3 point_light_pos,int index, float bias, vec3 FragPos)
{
	float shadow = 0.0;

	vec3 light_to_frag = FragPos - point_light_pos;
	float current_depth = length(light_to_frag) / far;
	vec3 light_to_frag_direction = normalize(light_to_frag);

	float noise = InterleavedGradientNoise(FragPos.xy);
	float p = max(penumbra(noise, light_to_frag_direction, current_depth, 8, index), 1.0);

	for(int i=0; i<8; i++)
	{
		vec2 offset = VogelDiskSample(i,16, noise);
		vec3 constrainedOffset = restrictOffset(light_to_frag_direction, offset) * 0.01 * p;

		float static_depth, dynamic_depth;

		float closest_depth;

		static_depth = texture(point_shadow_framebuffer_depth_color_texture_static[index], normalize(light_to_frag_direction + constrainedOffset)).r;
		dynamic_depth = texture(point_shadow_framebuffer_depth_color_texture_dynamic[index], normalize(light_to_frag_direction + constrainedOffset)).r;

		closest_depth = static_depth < dynamic_depth ? static_depth : dynamic_depth;
		shadow += current_depth > closest_depth - bias ? 0.0 : 1.0;
	}

	shadow /= 8.0;

	return shadow;
}

void main()
{
	for(int i=0; i<MAX_POINT_LIGHTS; i++)
	{
		float bias = 0.000001;
 		penumbra_value += CheckPointShadow(lightPos, i, bias, FragPos.xyz);
	}
	penumbra_value /= MAX_POINT_LIGHTS;
	penumbra_value = 1.0 - penumbra_value;
}