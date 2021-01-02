#version 440 core

# define NUM_SAMPLES 8

out float FragColor;

in vec2 TexCoords;

uniform sampler2D depth_texture;
uniform sampler2D normal_texture;
uniform sampler2D noise_texture;

uniform vec3 kernel[NUM_SAMPLES];
uniform mat4 projection;
uniform mat4 view;
uniform float radius = 0.25;
uniform float bias = 0.1;
uniform mat4 inv_projection;

const vec2 noiseScale = textureSize(depth_texture, 0) / 4.0;


vec3 ScreenToViewPos(vec2 coords)
{
	float z = texture(depth_texture, coords).r * 2.0 - 1.0;

	vec4 clipSpacePosition = vec4(coords * 2.0 - 1.0, z, 1.0);
	vec4 viewSpacePosition = inv_projection * clipSpacePosition;

	viewSpacePosition /= viewSpacePosition.w;

	return viewSpacePosition.xyz;
}

void main()
{
    vec3 FragPos = ScreenToViewPos(TexCoords).xyz;

	vec3 normal = mat3(view) * texture(normal_texture, TexCoords).xyz;
	vec3 noise = texture(noise_texture, TexCoords * noiseScale).rgb;

	vec3 T = normalize(noise - normal * dot(noise, normal));
	vec3 B = cross(normal, T);

	mat3 TBN = mat3(T,B,normal);

	float occlusion = 0.0;
	float sample_depth;
	for(int i=0; i<NUM_SAMPLES; ++i)
	{
		vec3 kernel_sample = TBN * kernel[i];
		kernel_sample = FragPos + kernel_sample * radius;

		vec4 offset = vec4(kernel_sample, 1.0);
		offset = projection * offset;
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5 + 0.5;

		sample_depth = ScreenToViewPos(offset.xy).z;

		float rangeCheck = smoothstep(0.0, 1.0, radius/abs(FragPos.z - sample_depth));
		occlusion += (sample_depth >= kernel_sample.z + bias ? 1.0 : 0.0) * rangeCheck;
	}

	occlusion = (occlusion / (NUM_SAMPLES));
	FragColor = pow(1.0 - occlusion, 2.0);
}