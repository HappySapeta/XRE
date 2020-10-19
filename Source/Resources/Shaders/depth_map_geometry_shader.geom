 #version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

uniform mat4 directional_light_space_matrix;
uniform mat4 point_light_space_matrix_cube[6];
uniform bool is_directional;

out vec4 FragPos;

void main()
{
	if(is_directional)
	{
		for(int i=0; i<3; i++)
		{
			FragPos = gl_in[i].gl_Position;
			gl_Position = directional_light_space_matrix * FragPos;
			EmitVertex();
		}
		EndPrimitive();
		return;
	}

	for(int face=0; face<6; ++face)
	{
		gl_Layer = face;
		for(int i=0; i<3; i++)
		{
			FragPos = gl_in[i].gl_Position;
			
			gl_Position = point_light_space_matrix_cube[face] * FragPos;
			EmitVertex();
		}
		EndPrimitive();
	}
}