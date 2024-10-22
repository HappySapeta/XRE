#version 440 core

layout (location = 0) out vec2 out_1;

in vec2 TexCoords;

uniform sampler2D inputTexture_1;

uniform bool horizontal;
uniform float weights[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
	vec2 tex_offset = 1.0 / textureSize(inputTexture_1,0);
	vec2 o1 = texture(inputTexture_1, TexCoords).rg * weights[0];

	if(horizontal)
	{
		for(int i=1; i<5; i++)
		{
			o1 += texture(inputTexture_1, TexCoords + vec2(tex_offset.x * i, 0.0)).rg * weights[i];
			o1 += texture(inputTexture_1, TexCoords - vec2(tex_offset.x * i, 0.0)).rg * weights[i];
		}
	}
	else
	{
		for(int i=1; i<5; i++)
		{
			o1 += texture(inputTexture_1, TexCoords + vec2(0.0, tex_offset.y * i)).rg * weights[i];
			o1 += texture(inputTexture_1, TexCoords - vec2(0.0, tex_offset.y * i)).rg * weights[i];
		}
	}

	out_1 = o1;
}