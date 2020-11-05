#version 440 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBitangent;


uniform mat4 view;
uniform mat4 model;
uniform mat4 projection;

// ------------------

uniform vec3 light_position_vertex[6];

// ------------------

out vec3 light_pos_tspace_0;
out vec3 light_pos_tspace_1;
out vec3 light_pos_tspace_2;
out vec3 light_pos_tspace_3;
out vec3 light_pos_tspace_4;
out vec3 light_pos_tspace_5;
out vec3 model_normal;

void main()
{	
	// Tangent Space Calculation

	mat3 normalMatrix = transpose(inverse(mat3(model)));
	vec3 T = normalize(normalMatrix * aTangents);
	vec3 N = normalize(normalMatrix * aNormal);

	T = normalize(T - dot(T, N) * N);

	vec3 B = cross(N, T);

	mat3 TBN = transpose(mat3(T, B, N));

	//----------------------------------------------------------------------

	vec3 FragPos = vec3(model * vec4(aPos,1.0));
	
	light_pos_tspace_0 = TBN * light_position_vertex[0];
	light_pos_tspace_1 = TBN * light_position_vertex[1];
	light_pos_tspace_2 = TBN * light_position_vertex[2];
	light_pos_tspace_3 = TBN * light_position_vertex[3];
	light_pos_tspace_4 = TBN * light_position_vertex[4];
	light_pos_tspace_5 = TBN * light_position_vertex[5];
	model_normal = N;

	//----------------------------------------------------------------------
	gl_Position = projection * view * vec4(FragPos, 1.0);
}