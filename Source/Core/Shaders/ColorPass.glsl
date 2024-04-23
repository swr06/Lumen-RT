#version 450 core

#define PI 3.14159265359

#define DO_INDIRECT 

#include "Include/Octahedral.glsl"
#include "Include/SphericalHarmonics.glsl"
#include "Include/CookTorranceBRDF.glsl"
#include "Include/SpatialUtility.glsl"
#include "Include/Karis.glsl"
#include "Include/Utility.glsl"
#include "Include/DDA.glsl"
#include "Include/ColorConstants.h"

#include "Include/DebugIrradianceCache.glsl" 
#include "Include/ProbeDebug.glsl"

layout (location = 0) out vec3 o_Color;

layout(rgba16f, binding = 0) uniform image2D o_NormalLFe;

layout(R8UI, binding = 4) uniform uimage3D o_VoxelVolume;

in vec2 v_TexCoords;

uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_NormalLFTexture;
uniform sampler2D u_PBRTexture;
uniform sampler2D u_DepthTexture;
uniform sampler2D u_BlueNoise;
uniform sampler2D u_TransparentDepth;
uniform samplerCube u_Skymap;

uniform sampler2D u_IndirectDiffuse;
uniform sampler2D u_IndirectSpecular;

uniform sampler2D u_Volumetrics;

uniform sampler2D u_DebugTexture;

uniform samplerCube u_ProbePlayer;

uniform float u_RTAOStrength;
uniform vec3 u_ViewerPosition;
uniform vec3 u_LightDirection;
uniform mat4 u_InverseView;
uniform mat4 u_InverseProjection;
uniform mat4 u_Projection;
uniform mat4 u_ViewProjection;
uniform mat4 u_View;
uniform vec2 u_Dims;

uniform bool u_DoSSShadow;

uniform int u_Frame;

uniform int u_VoxelRange;
uniform int u_VoxelVolSize;
uniform int u_DoVoxelization;

uniform int u_DebugMode;

uniform bool u_DoVolumetrics;

uniform float u_Time;

uniform float u_zNear;
uniform float u_zFar;

uniform vec2 u_FocusPoint;

uniform mat4 u_ShadowMatrices[5]; // <- shadow matrices 
uniform sampler2DShadow u_ShadowTextures[5]; // <- the shadowmaps themselves 
uniform float u_ShadowClipPlanes[5]; // <- world space clip distances 
uniform vec2 u_ShadowBiasMult;

struct ProbeMapPixel {
	vec2 Packed;
};

layout (std430, binding = 4) buffer SSBO_ProbeMaps {
	ProbeMapPixel MapData[]; // x has luminance data, y has packed depth and depth^2
};

layout (std430, binding = 2) buffer SSBO_Player {
	float PlayerShadow; // <- Average shadow player
};


layout (std430, binding = 1) buffer EyeAdaptation_SSBO {
    float o_FocusDepth;
};


float LinearizeDepth(float depth)
{
	return (2.0 * u_zNear) / (u_zFar + u_zNear - depth * (u_zFar - u_zNear));
}

vec3 WorldPosFromDepth(float depth, vec2 txc)
{
    float z = depth * 2.0 - 1.0;
    vec4 ClipSpacePosition = vec4(txc * 2.0 - 1.0, z, 1.0);
    vec4 ViewSpacePosition = u_InverseProjection * ClipSpacePosition;
    ViewSpacePosition /= ViewSpacePosition.w;
    vec4 WorldPos = u_InverseView * ViewSpacePosition;
    return WorldPos.xyz;
}

vec3 ProjectToClipSpace(vec3 WorldPos) 
{
	vec4 ProjectedPosition = u_ViewProjection * vec4(WorldPos, 1.0f);
	ProjectedPosition.xyz /= ProjectedPosition.w;
	return ProjectedPosition.xyz;
}


vec2 GetVogelDiskSample(int sampleIndex, int sampleCount, float phi) 
{
    const float goldenAngle = 2.399963f;
    float r = sqrt((float(sampleIndex) + 0.5) / float(sampleCount));  
    float theta = float(sampleIndex) * goldenAngle + phi;
	vec2 sincos;
	sincos.x = sin(theta);
    sincos.y = cos(theta);
    return sincos.xy * r;
}


float ScreenspaceRaytrace(const vec3 Origin, const vec3 Direction, float Hash, const int Steps, const int BinarySteps, const float ThresholdMultiplier, float Distance) {

    float TraceDistance = Distance;
    float StepSize = TraceDistance / Steps;
    vec3 RayPosition = Origin + Direction * StepSize * Hash * 0.66f;
    float FinalDepth = 0.0f;

	float DepthSample, DepthLinear;

	vec3 ProjectedRayScreenspace;

    for (int Step = 0 ; Step < Steps ; Step++) {

        RayPosition += StepSize * Direction;
        ProjectedRayScreenspace = ProjectToClipSpace(RayPosition); 
		
		if(abs(ProjectedRayScreenspace.x) > 1.0f || abs(ProjectedRayScreenspace.y) > 1.0f || abs(ProjectedRayScreenspace.z) > 1.0f) 
		{
			return 1.0f;
		}
		
		ProjectedRayScreenspace.xyz = ProjectedRayScreenspace.xyz * 0.5f + 0.5f; 

		// Depth texture uses nearest filtering
		DepthSample = texture(u_DepthTexture, ProjectedRayScreenspace.xy).x;
        DepthLinear = LinearizeDepth(DepthSample); 
		FinalDepth = LinearizeDepth(ProjectedRayScreenspace.z); 
		float Error = abs(DepthLinear - FinalDepth);

        if (Error < StepSize * ThresholdMultiplier * float(BinarySteps) && ProjectedRayScreenspace.z > DepthSample) 
		{
			vec3 BinaryStepVector = StepSize * Direction;

			for (int BinaryStep = 0 ; BinaryStep < BinarySteps ; BinaryStep++) 
			{
			    BinaryStepVector *= 0.5f;
			    ProjectedRayScreenspace = ProjectToClipSpace(RayPosition); 
			    ProjectedRayScreenspace = ProjectedRayScreenspace * 0.5f + 0.5f;

                FinalDepth = texture(u_DepthTexture, ProjectedRayScreenspace.xy).x;

			    if (FinalDepth < ProjectedRayScreenspace.z) 
                {
			    	RayPosition -= BinaryStepVector;
			    }

			    else
                {
			    	RayPosition += BinaryStepVector;
			    }

			}

			Error = abs(LinearizeDepth(FinalDepth) - LinearizeDepth(ProjectedRayScreenspace.z));

			if (Error < StepSize * ThresholdMultiplier) {
				float Transversal = distance(RayPosition, Origin) / TraceDistance;
				return 0.0f;
			}

			break;
        }

        if (ProjectedRayScreenspace.z > DepthSample) {  
			break;
        }

    }

	return 1.0f;
}

float SampleShadowMap(vec3 SampleUV, int Map) {

	switch (Map) {
		
		case 0 :
			return texture(u_ShadowTextures[0], SampleUV).x; break;

		case 1 :
			return texture(u_ShadowTextures[1], SampleUV).x; break;

		case 2 :
			return texture(u_ShadowTextures[2], SampleUV).x; break;

		case 3 :
			return texture(u_ShadowTextures[3], SampleUV).x; break;

		case 4 :
			return texture(u_ShadowTextures[4], SampleUV).x; break;
	}

	return texture(u_ShadowTextures[4], SampleUV).x;
}

bool IsInBox(vec3 point, vec3 Min, vec3 Max) {
  return (point.x >= Min.x && point.x <= Max.x) &&
         (point.y >= Min.y && point.y <= Max.y) &&
         (point.z >= Min.z && point.z <= Max.z);
}

float FilterShadows(vec3 WorldPosition, vec3 N, int Samples, float ExpStep, bool d)
{
	int ClosestCascade = -1;
	float Shadow = 0.0;
	float VogelScales[5] = float[5](0.0015f, 0.0015f, 0.0015f, 0.0015f, 0.002f);
	float Biases[5] = float[5](0.025f, 0.035f, 0.045f, 0.055f, 0.065f);
	
	vec2 Hash = texture(u_BlueNoise, v_TexCoords * (u_Dims / textureSize(u_BlueNoise, 0).xy)).rg;

	Hash.xy = mod(Hash.xy + 1.61803398874f * (u_Frame % 100), 1.0f);

	float mul = 2048.0f / textureSize(u_ShadowTextures[0],0).x;

	vec4 ProjectionCoordinates;

	float HashBorder = 0.95f - Hash.y * 0.03f; 

	for (int Cascade = 0 ; Cascade < 4; Cascade++) {
	
		ProjectionCoordinates = u_ShadowMatrices[Cascade] * vec4(WorldPosition + N * Biases[Cascade] * u_ShadowBiasMult.x * 2.5f, 1.0f);

		if (abs(ProjectionCoordinates.x) < HashBorder && abs(ProjectionCoordinates.y) < HashBorder && ProjectionCoordinates.z < 1.0f 
		    && abs(ProjectionCoordinates.x) < 1.0f && abs(ProjectionCoordinates.y) < 1.0f)
		{
			bool BoxCheck = IsInBox(WorldPosition, 
									u_InverseView[3].xyz-(u_ShadowClipPlanes[Cascade]-Hash.x*0.55f),
									u_InverseView[3].xyz+(u_ShadowClipPlanes[Cascade]+Hash.x*0.5f));

			//if (BoxCheck) 
			{
				ProjectionCoordinates = ProjectionCoordinates * 0.5f + 0.5f;
				ClosestCascade = Cascade;
				break;
			}
		}
	}

	if (ClosestCascade < 0) {
		return 1.0f;
	}
	
	float Bias = 0.000005f * u_ShadowBiasMult.y;

	vec2 TexelSize = 1.0f / textureSize(u_ShadowTextures[ClosestCascade], 0).xy;

	int SampleCount = Samples;
	float iStep = 1.0f;
    
	for (int Sample = 0 ; Sample < SampleCount ; Sample++) {

		vec2 SampleUV = ProjectionCoordinates.xy + VogelScales[ClosestCascade] * GetVogelDiskSample(Sample, SampleCount, Hash.x) * iStep * mix(mul,1.,0.55f) + (Hash.xy * mix(mul,1.0f,0.8f) * TexelSize) * float(d) * 1.2f;
		
		if (SampleUV != clamp(SampleUV, 0.000001f, 0.999999f))
		{ 
			continue;
		}

		Shadow += 1.0f - SampleShadowMap(vec3(SampleUV, ProjectionCoordinates.z - Bias), ClosestCascade); 
		iStep *= ExpStep;
	}

	Shadow /= float(SampleCount);

	return 1.0f - clamp(pow(Shadow, 1.0f), 0.0f, 1.0f);
}


// Voxelization
vec3 TransformToVoxelSpace(vec3 WorldPosition) {
	WorldPosition = WorldPosition - u_InverseView[3].xyz;
	float Size = float(u_VoxelRange);
	float HalfExtent = Size / 2.0f;
	vec3 ScaledPos = WorldPosition / HalfExtent;
	vec3 Voxel = ScaledPos;
	Voxel = Voxel * 0.5f + 0.5f;
	return (Voxel * float(u_VoxelVolSize));
}

// Voxelization
bool DDA(vec3 origin, vec3 direction, int dist, out vec3 normal, out vec3 world_pos)
{
	const vec3 BLOCKNORMALS[6] = vec3[](vec3(1.0, 0.0, 0.0),vec3(-1.0, 0.0, 0.0),vec3(0.0, 1.0, 0.0),vec3(0.0, -1.0, 0.0),vec3(0.0, 0.0, 1.0),vec3(0.0, 0.0, -1.0));

	origin = TransformToVoxelSpace(origin);

	
	world_pos = origin;

	vec3 Temp;
	vec3 VoxelCoord; 
	vec3 FractPosition;

	Temp.x = direction.x > 0.0 ? 1.0 : 0.0;
	Temp.y = direction.y > 0.0 ? 1.0 : 0.0;
	Temp.z = direction.z > 0.0 ? 1.0 : 0.0;
	vec3 plane = floor(world_pos + Temp);

	for (int x = 0; x < dist; x++)
	{
		if (!InsideVolume(world_pos)) {
			break;
		}

		vec3 Next = (plane - world_pos) / direction;
		int side = 0;

		if (Next.x < min(Next.y, Next.z)) {
			world_pos += direction * Next.x;
			world_pos.x = plane.x;
			plane.x += sign(direction.x);
			side = 0;
		}

		else if (Next.y < Next.z) {
			world_pos += direction * Next.y;
			world_pos.y = plane.y;
			plane.y += sign(direction.y);
			side = 1;
		}

		else {
			world_pos += direction * Next.z;
			world_pos.z = plane.z;
			plane.z += sign(direction.z);
			side = 2;
		}

		VoxelCoord = (plane - Temp);
		int Side = ((side + 1) * 2) - 1;
		if (side == 0) {
			if (world_pos.x - VoxelCoord.x > 0.5){
				Side = 0;
			}
		}

		else if (side == 1){
			if (world_pos.y - VoxelCoord.y > 0.5){
				Side = 2;
			}
		}

		else {
			if (world_pos.z - VoxelCoord.z > 0.5){
				Side = 4;
			}
		}

		normal = BLOCKNORMALS[Side];
		uint data = imageLoad(o_VoxelVolume, ivec3(VoxelCoord.xyz)).x;

		if (data != 0)
		{
			return true; 
		}
	}

	return false;
}

// Convert texcoord -> worldspace direcction
vec3 SampleIncidentRayDirection(vec2 screenspace)
{
	vec4 clip = vec4(screenspace * 2.0f - 1.0f, -1.0, 1.0);
	vec4 eye = vec4(vec2(u_InverseProjection * clip), -1.0, 0.0);
	return normalize(vec3(u_InverseView * eye));
}


float HASH2SEED = 0.0f;
vec2 hash2() 
{
	return fract(sin(vec2(HASH2SEED += 0.1, HASH2SEED += 0.1)) * vec2(43758.5453123, 22578.1459123));
}

float SmoothMin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(a-b)/k, 0.0, 1.0);
    return mix(a, b, h) - k*h*(1.0-h);
}

const vec3 SunColor = SUN_COLOR_LIGHTING;

void main() 
{	

	vec3 RayOrigin = u_InverseView[3].xyz;
	vec3 RayDirection = normalize(SampleIncidentRayDirection(v_TexCoords));
	

	
	float BayerHash = fract(fract(mod(float(u_Frame), 256.0f) * (1.0 / 1.61803398)) + bayer32(gl_FragCoord.st));

	float SurfaceDistance = 1000000.0f;

	HASH2SEED = (v_TexCoords.x * v_TexCoords.y) * 64.0 * u_Time;

	ivec2 Pixel = ivec2(gl_FragCoord.xy);

	if (Pixel.x == 8 && Pixel.y == 8) {
		o_FocusDepth = LinearizeDepth(texture(u_DepthTexture, u_FocusPoint).x);
		PlayerShadow = FilterShadows(u_ViewerPosition + vec3(0.0f, 1.0f, 0.0f) * 0.01f, vec3(0.0f, 1.0f, 0.0f), 18, 1.035f, false);
	}

	//vec4 Volumetrics = texelFetch(u_Volumetrics, Pixel / 2, 0);

	float Depth = texelFetch(u_DepthTexture, Pixel, 0).x;

	float TransparentDepth = texelFetch(u_TransparentDepth, Pixel, 0).x;

	float LineaRayDirectionepth = LinearizeDepth(Depth);

	if (Depth > 0.999999f) {
		
		vec3 Hash = (vec3(hash2(), hash2().x) * 2.0f - 1.0f);
		vec3 SkymapSampleDir = RayDirection + Hash / float(textureSize(u_Skymap,0).x);
		o_Color = pow(texture(u_Skymap, SkymapSampleDir).xyz, vec3(2.07f)) * 2.65f; // Color tweaking, temporary, while the cloud skybox is being used. 

		// Sun Disc 
		if (dot(RayDirection, -u_LightDirection) > 0.99985f) {
			imageStore(o_NormalLFe, Pixel, vec4(vec3(0.0f), 256.0f));
			o_Color = SUN_COLOR_LIGHTING; 
		}

		// Draw probe spheres 
		if (u_DebugMode == 0) {
			DrawProbeSphereGrid(RayOrigin, RayDirection, SurfaceDistance, o_Color);
		}
		o_Color = u_DebugMode == 5 ? vec3(0.0f) : o_Color;
		return;
	}

	vec3 WorldPosition = WorldPosFromDepth(Depth,v_TexCoords).xyz;
	vec3 Normal = normalize(texelFetch(u_NormalTexture, Pixel, 0).xyz);
	vec4 NormalLF = texelFetch(u_NormalLFTexture, Pixel, 0);
	vec3 Albedo = texelFetch(u_AlbedoTexture, Pixel, 0).xyz;
	vec3 PBR = texelFetch(u_PBRTexture, Pixel, 0).xyz;

	vec3 Incident = (u_ViewerPosition - WorldPosition);
	SurfaceDistance = length(Incident);
	Incident /= SurfaceDistance;

	vec3 F0 = mix(vec3(0.04f), Albedo, PBR.y); // Fresnel  

	vec3 SpecularIndirect = vec3(0.0f);
	vec3 DiffuseIndirect = vec3(0.0f);

	#ifdef DO_INDIRECT
		const vec2 IndirectStrength = vec2(1.15f, 1.15f); // x : diffuse strength, y : specular strength

		// Sample GI
		vec4 GI = texelFetch(u_IndirectDiffuse, Pixel, 0).xyzw; 
		vec4 SpecGI = texelFetch(u_IndirectSpecular, Pixel, 0).xyzw; 

		vec3 FresnelTerm = FresnelSchlickRoughness(max(dot(Incident, Normal.xyz), 0.000001f), vec3(F0), PBR.x); // <- Not perfect accurate of course but it works and it looks decent  
		FresnelTerm = clamp(FresnelTerm, 0.0f, 1.0f);

		vec3 kS = FresnelTerm;
		vec3 kD = 1.0f - kS;
		kD *= 1.0f - PBR.y;
					
		vec2 BRDFCoord = vec2(max(dot(Incident, Normal.xyz), 0.000001f), PBR.x);
		BRDFCoord = clamp(BRDFCoord, 0.0f, 1.0f);
		vec2 BRDF = Karis(BRDFCoord.x, BRDFCoord.y);

		SpecularIndirect = SpecGI.xyz * (FresnelTerm * BRDF.x + BRDF.y) * IndirectStrength.y * (PBR.y > 0.04f ? 1.75f : 1.1f);
		
		float AO = clamp(pow(GI.w, 1.4f * u_RTAOStrength) + 0.0f, 0.0f, 1.0f);
		DiffuseIndirect = kD * GI.xyz * Albedo * IndirectStrength.x * AO;

		const mat4 ColorTweakMatrix = mat4(1.0f); //SaturationMatrix(1.1f);
		DiffuseIndirect = vec3(ColorTweakMatrix * vec4(DiffuseIndirect, 1.0f));

	#endif

	float FilteredShadows = FilterShadows(WorldPosition, Normal, 8, 1.0f, true);
	float Shadows = FilteredShadows;
	
	if (u_DoSSShadow) {
		float ScreenspaceShadow = ScreenspaceRaytrace(WorldPosition + Normal * 0.035f, -u_LightDirection, BayerHash, 17, 8, 0.0038f, 1.6);
		Shadows = min(Shadows, ScreenspaceShadow);
	}

	vec3 Direct = CookTorranceBRDF(u_ViewerPosition, WorldPosition, u_LightDirection, SunColor, Albedo, Normal, vec2(PBR.x, PBR.y), Shadows) ;
	
	vec3 EmissiveColor = Albedo * NormalLF.w;

	vec3 Combined = Direct + SpecularIndirect + DiffuseIndirect + EmissiveColor;

	// Occluded by transparent object
	if (TransparentDepth < Depth) {

	}

	o_Color = Combined.xyz;

	if (u_DebugMode == 0) {
		DrawProbeSphereGrid(RayOrigin, RayDirection, SurfaceDistance, o_Color);
	} else if (u_DebugMode == 1) {
		o_Color = GI.xyz;
	} else if (u_DebugMode == 2) {
		o_Color = vec3(AO);
	} else if (u_DebugMode == 3) {
		o_Color = SpecGI.xyz;
	} else if (u_DebugMode == 4) {
		o_Color = vec3(Shadows);
	} else if (u_DebugMode == 5) {
		o_Color = vec3(0.0f);
	} else if (u_DebugMode == 6) {
		o_Color = SampleProbes(WorldPosition, Normal, true).xyz;
	}  else if (u_DebugMode == 7) {
		o_Color = Albedo;
	}  else if (u_DebugMode == 8) {
		o_Color = Normal;
	}  else if (u_DebugMode == 9) {
		o_Color = vec3(PBR.x);
	}  else if (u_DebugMode == 10) {
		o_Color = vec3(PBR.y);
	}  else if (u_DebugMode == 11) {
		o_Color = vec3(EmissiveColor);
	}	else if (u_DebugMode == 12) {

		if (u_DoVoxelization != 1) {
			o_Color = vec3(0.);
		}

		else {
			vec3 w,n;
			bool IntersectedVoxelizedWorld = DDA(RayOrigin, RayDirection, int(64), n, w);
			o_Color = vec3(0.,0.,0);
			if (IntersectedVoxelizedWorld) {
				o_Color = n*0.5+0.5;
			}
		}
	}


	//o_Color = texture(u_DebugTexture, v_TexCoords).xyz; // / max(texture(u_DebugTexture, v_TexCoords).w, 0.0001f);
	o_Color = max(o_Color, 0.0f);
}