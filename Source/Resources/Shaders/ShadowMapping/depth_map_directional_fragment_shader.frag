#version 440 core

uniform float farPlane;
uniform vec3 lightPos;
uniform mat4 directional_light_space_matrix;

in vec4 FragPos;
out vec2 depth;

void main()
{	

	vec4 FragPosLightSpace = directional_light_space_matrix * FragPos;
	float d = FragPosLightSpace.z;// / FragPosLightSpace.w;

	d = length(FragPos.xyz - lightPos) / farPlane;

	float m1 = d;
	float m2 = d * d;

	float dx = dFdx(depth.x);
	float dy = dFdx(depth.y);

	m2 += 0.25 * (dx * dx + dy * dy);

	depth.r = m1;
	depth.g = m2;
}