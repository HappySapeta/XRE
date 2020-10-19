#version 330 core

in vec4 FragPos;

uniform vec3 light_pos;
uniform float far_plane;


void main()
{	
	float depth = length(FragPos.xyz - light_pos);
	depth /= far_plane;
	gl_FragDepth = depth;
}