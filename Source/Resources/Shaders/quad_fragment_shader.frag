#version 330 core

const float offset = 1/500.0;

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
out vec4 FragColor;

uniform sampler2D screenTexture;
//uniform samplerCube screenTexture;
uniform bool use_pfx;
uniform bool gamma_correct = true;
uniform float gamma = 2.2;
uniform blur_options blur_options_i;
uniform bloom_options bloom_options_i;
uniform ssao_options ssao_options_i;

vec3 color;

void applyBloom()
{
}

void applyBlur()
{
}

void applySSAO()
{
}

void applyGammaCorrection()
{
	color = pow(color, vec3(1.0/gamma));
}

void main()
{	
	if(!use_pfx)
	{
		//color = texture(screenTexture, vec3(TexCoords.x, -1.0, TexCoords.y-0.25)).rgb;
		color = texture(screenTexture, TexCoords).rgb;
	}
	else
	{
		if(blur_options_i.enabled)
		{
			applyBlur();
		}
		if(bloom_options_i.enabled)
		{
			applyBloom();
		}
		if(ssao_options_i.enabled)
		{
			applySSAO();
		}
	}

	if(gamma_correct)
	{
		applyGammaCorrection();
	}

	FragColor = vec4(color,1.0);
}