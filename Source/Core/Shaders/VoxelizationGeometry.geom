#version 450 core

layout (triangles) in;

layout (triangle_strip, max_vertices = 3) out;

in vec3 v_WorldPosition[];
in vec3 v_Normal[];
//in vec2 v_UVs[];

out vec3 g_WorldPosition;
out vec3 g_Normal;
//out vec2 g_UV;

flat out int g_DominantAxis;
uniform vec3 u_CoverageSize;

uniform vec3 u_VoxelGridCenter;



int GetLargestAxis(in vec3 x) {
	int Dominantaxis = x[1] > x[0] ? 1 : 0;
	Dominantaxis = x[2] > x[Dominantaxis] ? 2 : Dominantaxis;
	return Dominantaxis;
}

vec3 ToVoxelSpace(vec3 Position, in float HalfExtent) {
	return (Position - u_VoxelGridCenter) / HalfExtent;
}

void main() {

	float Size = u_CoverageSize.x;

	float HalfExtent = Size * 0.5f;

	vec3 FaceNormal = abs(v_Normal[0] + v_Normal[1] + v_Normal[2]);

	// Figure out index of maximum axis 
	int LargestAxis = GetLargestAxis(FaceNormal);

	for (int Vertex = 0 ; Vertex < 3 ; Vertex++) {

		g_WorldPosition = v_WorldPosition[Vertex];
		g_Normal = v_Normal[Vertex];

		// Convert to local voxel space 
		vec3 LocalSpace = ToVoxelSpace(g_WorldPosition, HalfExtent);
		
		// Simple Orthogonal projection 
		if (LargestAxis == 0)
		{
			gl_Position = vec4(LocalSpace.y, LocalSpace.z, 0.0f, 1.0f);
		} 
		
		else if (LargestAxis == 1)
		{
			gl_Position = vec4(LocalSpace.x, LocalSpace.z, 0.0f, 1.0f);
		}

		else
		{
			gl_Position = vec4(LocalSpace.x, LocalSpace.y, 0.0f, 1.0f);
		} 

		gl_Position.z = 0.0f;
		gl_Position.w = 1.0f;

		g_WorldPosition = g_WorldPosition - u_VoxelGridCenter;

		//g_UV = v_UVs[Vertex];

		g_DominantAxis = LargestAxis;


		const bool Conservative = false;

		if (Conservative) {


		}

		EmitVertex();
	}

	EndPrimitive();
}