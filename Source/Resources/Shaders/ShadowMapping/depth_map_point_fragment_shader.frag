#version 440 core

uniform float farPlane;
uniform vec3 lightPos;

in vec4 FragPos;
out vec4 depth;

void main()
{	
	float d = length(FragPos.xyz - lightPos) / farPlane;

	float m1 = d;
	float m2 = d * d;

	float dx = dFdx(depth.x);
	float dy = dFdx(depth.y);

	m2 += 0.25 * (dx * dx + dy * dy);

	depth.r = m1;
	depth.g = m2;
}