#version 440 core

layout (location = 0) out vec4 FragColor;

struct blur_options
{
	bool enabled;
	float amount;
	int kernel_size;
};

struct bloom_options
{
	bool enabled;
	float intensity;
	float threshold;
	float radius;
	vec3 color;
};

struct ssao_options
{
	bool enabled;
	float radius;
	float intensity;
};


in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D blurTexture;

uniform bool use_pfx;
uniform bool gamma_correct = true;
uniform float gamma = 2.2;
uniform float exposure = 0.8;

vec3 color_sample;
vec3 out_color;

vec3 applyHDR(vec3 in_color)
{
	return vec3(1.0) - exp(-in_color * exposure);
}

vec3 applyGammaCorrection(vec3 in_color)
{
	return pow(in_color, vec3(1.0/gamma));
}

void main()
{	
	color_sample = texture(screenTexture, TexCoords).rgb;
	vec3 bloomColor = texture(blurTexture, TexCoords).rgb;

	//color_sample += bloomColor;
	// HDR
	// reinhard tone mapping
	out_color = applyHDR(color_sample);
	out_color = applyGammaCorrection(out_color);

	FragColor = vec4(out_color,1.0);
}