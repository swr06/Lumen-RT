#version 430 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in uvec3 a_NormalTangentData;
layout (location = 2) in uint a_TexCoords;

uniform mat4 u_ModelMatrix;

out vec3 v_WorldPosition;
out vec3 v_Normal;
//out vec2 v_UVs;

void main()
{
	vec4 WorldPos = u_ModelMatrix * vec4(a_Position, 1.0f);
	v_WorldPosition = WorldPos.xyz;

	gl_Position = vec4(WorldPos.xyz, 1.0f);
	
	vec2 Data_0 = unpackHalf2x16(a_NormalTangentData.x);
	vec2 Data_1 = unpackHalf2x16(a_NormalTangentData.y);

	vec3 Normal = vec3(Data_0.x, Data_0.y, Data_1.x);

	v_Normal = Normal; 

	//v_UVs = unpackHalf2x16(a_TexCoords);
}