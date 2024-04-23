#version 430 core 

#define bool uint 
#define char uint 

layout(R8UI, binding = 0) uniform uimage3D o_VoxelVolume;

in vec3 g_WorldPosition;
in vec3 g_VolumePosition;
//in vec3 g_Normal;

in vec2 g_UV;

uniform int u_VolumeSize;
uniform float u_CoverageSizeF;

uniform vec3 u_VoxelGridCenterF;

float remap(float x, float a, float b, float c, float d)
{
    return (((x - a) / (b - a)) * (d - c)) + c;
}

void main() {

	float Size = u_CoverageSizeF;
	float HalfExtent = Size / 2.0f;

	vec3 Clipspace = g_WorldPosition / HalfExtent; 

	vec3 Voxel = Clipspace;

	Voxel = Voxel * 0.5f + 0.5f;

	if (Voxel == clamp(Voxel, 0.0f, 1.0f)) {
		ivec3 VoxelSpaceCoord = ivec3(Voxel * float(u_VolumeSize));
		imageStore(o_VoxelVolume, VoxelSpaceCoord, uvec4(1));
	}
}