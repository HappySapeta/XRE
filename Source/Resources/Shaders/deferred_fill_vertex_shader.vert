#version 440 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangents;
layout (location = 4) in vec3 aBitangent;

out vec2 TexCoords;
out vec3 FragPos;

out vec3 T;
out vec3 N;

uniform mat4 view;
uniform mat4 model;
uniform mat4 projection;

uniform vec3 camera_position_vertex;

// ------------------

void main()
{	
	// Tangent Space Calculation

	mat3 normalMatrix = transpose(inverse(mat3(model)));

	T = normalize(vec3(normalMatrix * aTangents));
	N = normalize(vec3(normalMatrix * aNormal));

	T = normalize(T - dot(T, N) * N);

	//----------------------------------------------------------------------

	FragPos = vec3(model * vec4(aPos,1.0));
    TexCoords = aTexCoords;

	//----------------------------------------------------------------------
	gl_Position = projection * view * vec4(FragPos, 1.0);
}