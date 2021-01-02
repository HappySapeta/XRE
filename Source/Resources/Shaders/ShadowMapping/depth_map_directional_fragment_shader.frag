#version 440 core

uniform float farPlane;
uniform vec3 lightPos;
uniform mat4 directional_light_space_matrix;
uniform float positive_exponent;
uniform float negative_exponent;

in vec4 FragPos;
out vec4 depth;

void main()
{	

	vec4 FragPosLightSpace = directional_light_space_matrix * FragPos;
	//float d = FragPosLightSpace.z;// / FragPosLightSpace.w;
	float d = length(FragPos.xyz - lightPos) / farPlane;
	vec2 exponents = vec2(positive_exponent, negative_exponent);
	d = d * 2.0 - 1.0;

	float pos = exp(exponents.x * d);
	float neg = -exp(-exponents.y * d);

	vec2 warp_depth = vec2(pos, neg);
	/*
	d = length(FragPos.xyz - lightPos) / farPlane;

	float m1 = d;
	float m2 = d * d;

	float dx = dFdx(depth.x);
	float dy = dFdx(depth.y);

	m2 += 0.25 * (dx * dx + dy * dy);
	*/
	depth = vec4(warp_depth, warp_depth * warp_depth);
}