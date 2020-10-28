 #version 440 core

in vec4 frag_world_pos[3];
layout (triangle_strip, max_vertices = 18) out;

uniform int faces;
uniform mat4 light_space_matrix_cube[6];

out vec4 FragPos;

void main()
{
	for(int face=0; face<faces; ++face)
	{
		gl_Layer = face;
		for(int i=0; i<3; i++)
		{	
			FragPos = frag_world_pos[i];
			gl_Position = light_space_matrix_cube[face] * FragPos;
			EmitVertex();
		}
		EndPrimitive();
	}
}