#version 440 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBitangent;

out vec2 TexCoords;
out vec3 FragPos;
out vec3 vNormal;
out vec4 FragPosLightSpace;

uniform mat4 view;
uniform mat4 model;
uniform mat4 projection;
uniform mat4 point_light_space_projection;
uniform mat4 directional_light_space_matrix;


// Tangent Space Data
mat3 TBN;

uniform vec3 camera_position_vertex;
uniform vec3 light_position_vertex[6];

out vec3 light_pos_tspace[6];
out vec3 camera_position_tspace;
out vec3 frag_pos_tspace;

// ------------------

void main()
{
	//----------------------------------------------------------------------

	FragPos = vec3(model * vec4(aPos,1.0));
    TexCoords = aTexCoords;
    FragPosLightSpace = directional_light_space_matrix * vec4(FragPos,1.0);

	//----------------------------------------------------------------------

	mat3 normalMatrix = transpose(inverse(mat3(model)));
	vec3 T = normalize(normalMatrix * aTangents);
	vec3 N = normalize(normalMatrix * aNormal);

	T = normalize(T - dot(T, N) * N);

	vec3 B = cross(N, T);

	mat3 TBN = transpose(mat3(T, B, N));

	for(int i=0; i< 6; i++)
	{
		light_pos_tspace[i] = TBN * light_position_vertex[i];
	}

	camera_position_tspace = TBN * camera_position_vertex;
	frag_pos_tspace = TBN * FragPos;

	gl_Position = projection * view * vec4(FragPos, 1.0);
}