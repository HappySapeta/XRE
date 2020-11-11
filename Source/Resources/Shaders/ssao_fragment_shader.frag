#version 440 core

# define NUM_SAMPLES 4

out float FragColor;

in vec2 TexCoords;

vec2 poissonDisk[64];

uniform sampler2D position_texture;
uniform sampler2D normal_texture;
uniform sampler2D noise_texture;

uniform vec3 kernel[NUM_SAMPLES];
uniform mat4 projection;
uniform mat4 view;
uniform float radius = 0.4;
uniform float bias = 0.1;

const vec2 noiseScale = textureSize(position_texture, 0) / 4.0;

void main()
{
	vec3 FragPos = (view * texture(position_texture, TexCoords)).xyz;
	vec3 normal = texture(normal_texture, TexCoords).xyz;
	vec3 noise = texture(noise_texture, TexCoords  * noiseScale).rgb;

	vec3 T = normalize(noise - normal * dot(noise, normal));
	vec3 B = cross(normal, T);

	mat3 TBN = mat3(T,B,normal);

	float occlusion = 0.0;


	for(int i=0; i<NUM_SAMPLES; ++i)
	{

		vec3 kernel_sample = TBN * kernel[i];
		kernel_sample = FragPos + kernel_sample * radius;

		vec4 offset = vec4(kernel_sample, 1.0);
		offset = projection * offset;
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5 + 0.5;

		float sample_depth = (view * texture(position_texture, offset.xy)).z;

		float rangeCheck = smoothstep(0.0, 1.0, radius/abs(FragPos.z - sample_depth));
		occlusion += (sample_depth >= kernel_sample.z + bias ? 1.0 : 0.0) * rangeCheck;
	}

	occlusion = (occlusion / (NUM_SAMPLES));
	FragColor = pow(1.0 - occlusion, 2.0);
}