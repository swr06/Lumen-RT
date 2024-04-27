#version 450 core 
#define COMPUTE

// Uncomment this line to enable indirect caustics
//#define DO_INDIRECT_CAUSTICS

#define PI 3.141592653

#include "TraverseBVH.glsl"
#include "Include/SphericalHarmonics.glsl"
#include "Include/Utility.glsl"
#include "Include/SpatialUtility.glsl"
#include "Include/Sampling.glsl"
#include "Include/ColorConstants.h"

layout(local_size_x = 16, local_size_y = 16) in;
layout(rgba16f, binding = 0) uniform image2D o_OutputData;

layout(r8ui, binding = 1) uniform uimage3D o_LightCullData;
layout(r8ui, binding = 2) uniform uimage3D o_LightCullData1;
layout(r8ui, binding = 3) uniform uimage3D o_LightCullData2;
layout(r8ui, binding = 4) uniform uimage3D o_LightCullData3;

uniform mat4 u_Projection;
uniform mat4 u_View;
uniform mat4 u_InverseProjection;
uniform mat4 u_InverseView;
uniform mat4 u_ViewProjection;

uniform vec2 u_Dims;
uniform vec3 u_SunDirection;

uniform sampler2D u_DepthTexture;
uniform sampler2D u_TransparentDepth;
uniform sampler2D u_TransparentAlbedo;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_Albedo;
uniform sampler2D u_BlueNoise;
uniform samplerCube u_Skymap;

uniform int u_Frame;
uniform int u_SecondaryBounces;
uniform float u_Time;

uniform mat4 u_ShadowMatrices[5]; // <- shadow matrices 
uniform sampler2D u_ShadowTextures[5]; // <- the shadowmaps themselves 
uniform float u_ShadowClipPlanes[5]; // <- world space clip distances 

uniform vec3 u_ProbeBoxSize;
uniform vec3 u_ProbeGridResolution;
uniform vec3 u_ProbeBoxOrigin;

uniform usampler3D u_SHDataA;
uniform usampler3D u_SHDataB;

uniform float u_zNear;
uniform float u_zFar;

uniform int u_LightCullGridRes;
uniform float u_LightCullGridScale;

uniform bool u_Checker;

uniform bool u_SecondBounce;
uniform bool u_SecondBounceRT;

uniform bool u_IndirectSSCaustics;
uniform bool DO_BL_SAMPLING;

uniform bool u_GenerateLightCullGrid;
uniform bool u_DoScreentrace;

uniform sampler2D u_IndirectDiffuse; // <- Previous frame diffuse, to create a positive feedback loop 
uniform sampler2D u_MotionVectors;

uniform mat4 u_PrevProjection;
uniform mat4 u_PrevView;
uniform mat4 u_PrevInverseProjection;
uniform mat4 u_PrevInverseView;

struct SphereLight {
	vec4 PositionRadius;
	vec4 ColorEmissive;
};

layout (std430, binding = 0) buffer atomicLightIDS {
    int LightIndicesCounter[];
};

layout (std430, binding = 1) buffer SSBO_SphereLights {
	SphereLight SphereLights[];
};

vec3 Reprojection(vec3 WorldPos) 
{
	vec4 ProjectedPosition = u_PrevProjection * u_PrevView * vec4(WorldPos, 1.0f);
	ProjectedPosition.xyz /= ProjectedPosition.w;
	ProjectedPosition.xyz = ProjectedPosition.xyz * 0.5f + 0.5f;
	return ProjectedPosition.xyz;
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

vec3 ProjectToScreenSpace(vec3 WorldPos) 
{
	vec4 ProjectedPosition = u_ViewProjection * vec4(WorldPos, 1.0f);
	ProjectedPosition.xyz /= ProjectedPosition.w;
	ProjectedPosition.xyz = ProjectedPosition.xyz * 0.5f + 0.5f;
	return ProjectedPosition.xyz;
}

vec3 ProjectToClipSpace(vec3 WorldPos) 
{
	vec4 ProjectedPosition = u_ViewProjection * vec4(WorldPos, 1.0f);
	ProjectedPosition.xyz /= ProjectedPosition.w;
	return ProjectedPosition.xyz;
}

float LinearizeDepth(float depth)
{
	return (2.0 * u_zNear) / (u_zFar + u_zNear - depth * (u_zFar - u_zNear));
}

float HASH2SEED = 0.0f;
vec2 hash2() 
{
	return fract(sin(vec2(HASH2SEED += 0.1, HASH2SEED += 0.1)) * vec2(43758.5453123, 22578.1459123));
}

vec4 ScreenspaceRaytrace(sampler2D DepthTex, const vec3 Origin, const vec3 Direction, const int Steps, const int BinarySteps, const float ThresholdMultiplier, float TraceDistance) {

    float StepSize = TraceDistance / Steps;

	vec2 Hash = hash2();
    vec3 RayPosition = Origin + Direction * Hash.x;

    vec3 FinalProjected = vec3(0.0f);
    float FinalDepth = 0.0f;

    bool FoundIntersection = false;
    int SkyHits = 0;

    for (int Step = 0 ; Step < Steps ; Step++) {

        vec3 ProjectedRayScreenspace = ProjectToClipSpace(RayPosition); 
		
		if(abs(ProjectedRayScreenspace.x) > 1.0f || abs(ProjectedRayScreenspace.y) > 1.0f || abs(ProjectedRayScreenspace.z) > 1.0f) 
		{
			return vec4(vec3(-1.0f),SkyHits>4);
		}
		
		ProjectedRayScreenspace.xyz = ProjectedRayScreenspace.xyz * 0.5f + 0.5f; 

		// Depth texture uses nearest filtering
        float DepthAt = texture(DepthTex, ProjectedRayScreenspace.xy).x; 

        if (DepthAt == 1.0f) {
            SkyHits += 1;
        }

		float CurrentRayDepth = LinearizeDepth(ProjectedRayScreenspace.z); 
		float Error = abs(LinearizeDepth(DepthAt) - CurrentRayDepth);

        if (Error < StepSize * ThresholdMultiplier * 8.0f && ProjectedRayScreenspace.z > DepthAt) 
		{

			vec3 BinaryStepVector = (Direction * StepSize) / 2.0f;
            RayPosition -= (Direction * StepSize) * 0.5f; // <- Step back a bit 
			    
            for (int BinaryStep = 0 ; BinaryStep < BinarySteps ; BinaryStep++) {
			    		
			    BinaryStepVector /= 2.0f;

			    vec3 Projected = ProjectToClipSpace(RayPosition); 
			    Projected = Projected * 0.5f + 0.5f;
                FinalProjected = Projected;

				// Depth texture uses nearest filtering
                float Fetch = texture(DepthTex, Projected.xy).x;
                FinalDepth = Fetch;

			    float BinaryDepthAt = LinearizeDepth(Fetch); 
			    float BinaryRayDepth = LinearizeDepth(Projected.z); 

			    if (BinaryDepthAt < BinaryRayDepth) 
                {
			    	RayPosition -= BinaryStepVector;
			    }

			    else
                {
			    	RayPosition += BinaryStepVector;
			    }
			}

			Error = abs(LinearizeDepth(FinalDepth) - LinearizeDepth(FinalProjected.z));

			if (Error < StepSize * ThresholdMultiplier) {
				FoundIntersection = true; 
			}

            break;
        }

        if (ProjectedRayScreenspace.z > DepthAt) {  
            FoundIntersection = false;
            break;
        }

        RayPosition += StepSize * Direction;
    }

    if (!FoundIntersection) {
        return vec4(vec3(-1.0f), SkyHits>4);
    }

	float T = distance(RayPosition, Origin);

	vec3 Pos = Origin + Direction * T;

	if (distance(RayPosition, Pos) > 0.025f) {
		return vec4(vec3(-1.0f), SkyHits>4);
	}

    return vec4(FinalProjected.xy, FinalDepth == 1.0f ? -1.0f : T, FinalDepth == 1.0f ? 1.0f : float(SkyHits>4));
}

float[8] Trilinear(vec3 BoxMin, vec3 BoxMax, vec3 p) {
    float Weights[8];
    vec3 Extent = BoxMax - BoxMin;
    float InverseVolume = 1.0 / (Extent.x * Extent.y * Extent.z);
    Weights[0] = (BoxMax.x - p.x) * (BoxMax.y - p.y) * (BoxMax.z - p.z) * InverseVolume;
    Weights[1] = (BoxMax.x - p.x) * (p.y - BoxMin.y) * (BoxMax.z - p.z) * InverseVolume;
    Weights[2] = (p.x - BoxMin.x) * (p.y - BoxMin.y) * (BoxMax.z - p.z) * InverseVolume;
    Weights[3] = (p.x - BoxMin.x) * (BoxMax.y - p.y) * (BoxMax.z - p.z) * InverseVolume;
    Weights[4] = (BoxMax.x - p.x) * (BoxMax.y - p.y) * (p.z - BoxMin.z) * InverseVolume;
    Weights[5] = (BoxMax.x - p.x) * (p.y - BoxMin.y) * (p.z - BoxMin.z) * InverseVolume;
    Weights[6] = (p.x - BoxMin.x) * (p.y - BoxMin.y) * (p.z - BoxMin.z) * InverseVolume;
    Weights[7] = (p.x - BoxMin.x) * (BoxMax.y - p.y) * (p.z - BoxMin.z) * InverseVolume;
    return Weights;
}

SH GetSH(ivec3 Texel) {
	uvec4 A = texelFetch(u_SHDataA, Texel, 0);
	uvec4 B = texelFetch(u_SHDataB, Texel, 0);
	return UnpackSH(A,B);
}

float GetVisibility(ivec3 Texel, vec3 WorldPosition, vec3 Normal) {
	
	vec3 TexCoords = vec3(Texel) / u_ProbeGridResolution;
	vec3 Clip = TexCoords * 2.0f - 1.0f;
	vec3 ProbePosition = u_ProbeBoxOrigin + Clip * u_ProbeBoxSize;

	vec3 Vector = ProbePosition - WorldPosition;
	float Length = length(Vector);
	Vector /= Length;

	float Weight = pow(clamp(dot(Normal, Vector), 0.0f, 1.0f), 1.75f);
	return Weight;
}

vec3 SampleProbes(vec3 WorldPosition, vec3 N) {

	WorldPosition += N * 0.4f;

	vec3 SamplePoint = (WorldPosition - u_ProbeBoxOrigin) / u_ProbeBoxSize; 
	SamplePoint = SamplePoint * 0.5 + 0.5; 

	if (SamplePoint == clamp(SamplePoint, 0.0f, 1.0f)) {
		
		vec3 VolumeCoords = SamplePoint * (u_ProbeGridResolution);
		
		vec3 MinSampleBox = floor(VolumeCoords);
		vec3 MaxSampleBox = ceil(VolumeCoords);

		float Alpha = 0.0f;
		float Trilinear[8] = Trilinear(MinSampleBox, MaxSampleBox, VolumeCoords);
		ivec3 TexelCoordinates[8];
		SH sh[8];

		TexelCoordinates[0] = ivec3(vec3(MinSampleBox.x, MinSampleBox.y, MinSampleBox.z)); 
		TexelCoordinates[1] = ivec3(vec3(MinSampleBox.x, MaxSampleBox.y, MinSampleBox.z));
		TexelCoordinates[2] = ivec3(vec3(MaxSampleBox.x, MaxSampleBox.y, MinSampleBox.z)); 
		TexelCoordinates[3] = ivec3(vec3(MaxSampleBox.x, MinSampleBox.y, MinSampleBox.z));
		TexelCoordinates[4] = ivec3(vec3(MinSampleBox.x, MinSampleBox.y, MaxSampleBox.z));
		TexelCoordinates[5] = ivec3(vec3(MinSampleBox.x, MaxSampleBox.y, MaxSampleBox.z));
		TexelCoordinates[6] = ivec3(vec3(MaxSampleBox.x, MaxSampleBox.y, MaxSampleBox.z)); 
		TexelCoordinates[7] = ivec3(vec3(MaxSampleBox.x, MinSampleBox.y, MaxSampleBox.z));

		for (int i = 0 ; i < 8 ; i++) {
			sh[i] = GetSH(TexelCoordinates[i]);
			float ProbeVisibility = GetVisibility(TexelCoordinates[i], WorldPosition, N);
			Alpha += Trilinear[i] * (1.0f - ProbeVisibility);
			Trilinear[i] *= ProbeVisibility;
		}

		float WeightSum = 1.0f - Alpha;

		SH FinalSH = GenerateEmptySH();

		for (int i = 0 ; i < 8 ; i++) {
			ScaleSH(sh[i], vec3(Trilinear[i] / max(WeightSum, 0.000001f)));
			FinalSH = AddSH(FinalSH, sh[i]);
		}

		return max(SampleSH(FinalSH, N), 0.0f);
	}

	return vec3(0.0f);
}


float SampleShadowMap(vec2 SampleUV, int Map) {

	switch (Map) {
		
		case 0 :
			return TexelFetchNormalized(u_ShadowTextures[0], SampleUV).x; break;

		case 1 :
			return TexelFetchNormalized(u_ShadowTextures[1], SampleUV).x; break;

		case 2 :
			return TexelFetchNormalized(u_ShadowTextures[2], SampleUV).x; break;

		case 3 :
			return TexelFetchNormalized(u_ShadowTextures[3], SampleUV).x; break;

		case 4 :
			return TexelFetchNormalized(u_ShadowTextures[4], SampleUV).x; break;
	}

	return TexelFetchNormalized(u_ShadowTextures[4], SampleUV).x;
}

bool IsInBox(vec3 point, vec3 Min, vec3 Max) {
  return (point.x >= Min.x && point.x <= Max.x) &&
         (point.y >= Min.y && point.y <= Max.y) &&
         (point.z >= Min.z && point.z <= Max.z);
}

float GetDirectShadow(vec3 WorldPosition, vec3 N)
{
	int ClosestCascade = -1;
	float Shadow = 0.0;
	float VogelScales[5] = float[5](0.001f, 0.0015f, 0.002f, 0.00275f, 0.00325f);
	
	vec2 TexelSize = 1.0 / textureSize(u_ShadowTextures[ClosestCascade], 0);

	vec4 ProjectionCoordinates;

	float HashBorder = 1.0f; 

	for (int Cascade = 0 ; Cascade < 4; Cascade++) {
	
		ProjectionCoordinates = u_ShadowMatrices[Cascade] * vec4(WorldPosition + N * 0.035f, 1.0f);

		if (ProjectionCoordinates.z < 1.0f && abs(ProjectionCoordinates.x) < 1.0f && abs(ProjectionCoordinates.y) < 1.0f)
		{
			bool BoxCheck = IsInBox(WorldPosition, 
									u_InverseView[3].xyz-(u_ShadowClipPlanes[Cascade]),
									u_InverseView[3].xyz+(u_ShadowClipPlanes[Cascade]));

			//if (BoxCheck) 
			{
				ProjectionCoordinates = ProjectionCoordinates * 0.5f + 0.5f;
				ClosestCascade = Cascade;
				break;
			}
		}
	}

	if (ClosestCascade < 0) {
		return 0.0f;
	}
	
	float Bias = 0.00001f;
	vec2 SampleUV = ProjectionCoordinates.xy;
	Shadow = float(ProjectionCoordinates.z - Bias > SampleShadowMap(SampleUV, ClosestCascade)); 
	return 1.0f - Shadow;
}

vec3 GetDirect(in vec3 WorldPosition, in vec3 Normal, in vec3 Albedo) {

	float Shadow = GetDirectShadow(WorldPosition, Normal);
	return vec3(Albedo) * SUN_COLOR_DIFF * Shadow * clamp(dot(Normal, -u_SunDirection), 0.0f, 1.0f);
}

float SeedFunction(float Input) {
	float Factor = 48.0f;
	return clamp(floor(Input * Factor) / float(Factor), 0.0f, 4.0f);
	//return Input;
	//return (Input * Input) * (3.0 - 2.0 * Input);
	//return Input * (2.0f - Input);
}

ivec3 TransformToVoxelSpaceI(vec3 WorldPosition)
{
	WorldPosition = WorldPosition - u_InverseView[3].xyz;
	float HalfExtent = u_LightCullGridScale / 2.0f;
	vec3 ScaledPos = WorldPosition / HalfExtent;
	vec3 Voxel = ScaledPos;
	Voxel = Voxel * 0.5f + 0.5f;
	return ivec3(Voxel * u_LightCullGridRes);
}

int Idx3DToLinear(int x, int y, int z) {
	return (z * u_LightCullGridRes * u_LightCullGridRes) + (y * u_LightCullGridRes) + x;
}

bool DO_SECOND_BOUNCE = u_SecondBounce;
bool RT_SECOND_BOUNCE = u_SecondBounceRT;
bool DO_SCREENTRACE = u_DoScreentrace;

void main() {

	ivec2 Pixel = ivec2(gl_GlobalInvocationID.xy);
	ivec2 WritePixel = ivec2(gl_GlobalInvocationID.xy);

	if (Pixel.x < 0 || Pixel.y < 0 || Pixel.x > int(u_Dims.x) || Pixel.y > int(u_Dims.y)) {
		return;
	}

	// Handle checkerboard 
	if (u_Checker) {
		Pixel.x *= 2;
		bool IsCheckerStep = Pixel.x % 2 == int(Pixel.y % 2 == (u_Frame % 2));
		Pixel.x += int(IsCheckerStep);
	}

	// 1/2 res on each axis
    ivec2 HighResPixel = Pixel * 2;
    vec2 HighResUV = vec2(HighResPixel) / textureSize(u_DepthTexture, 0).xy;

	// Fetch 
    float Depth = texelFetch(u_DepthTexture, HighResPixel, 0).x;

	if (Depth > 0.999999f || Depth == 1.0f) {
		imageStore(o_OutputData, WritePixel, vec4(0.0f));
        return;
    }

	vec2 TexCoords = HighResUV;

	HASH2SEED = u_Time;

	if (DO_BL_SAMPLING) {
		
		ivec2 CoordB = WritePixel % 1024;
		vec3 BlueNoise = texelFetch(u_BlueNoise, CoordB, 0).xyz;
		float SeedOffset = SeedFunction(BlueNoise.x);
		HASH2SEED *= 10.0f;
		HASH2SEED += SeedOffset * 1.0f;
		hash2();
	}

	else {
		HASH2SEED *= (TexCoords.x * TexCoords.y) * 64.0;
	}

	if (false) {
		imageStore(o_OutputData, WritePixel, vec4(hash2().xxx, 1));
		return;
	}

	const vec3 Player = u_InverseView[3].xyz;

	vec3 WorldPosition = WorldPosFromDepth(Depth, TexCoords);
	vec3 Normal = normalize(texelFetch(u_NormalTexture, HighResPixel, 0).xyz);

	vec3 EyeVector = normalize(WorldPosition - Player);
	vec3 Reflected = reflect(EyeVector, Normal);

	vec3 RayOrigin = WorldPosition + Normal * 0.05f;
	vec3 RayDirection = CosWeightedHemisphere(Normal, hash2());
	ivec3 VoxelCoord = TransformToVoxelSpaceI(WorldPosition);

	if (u_GenerateLightCullGrid) {
		int Numlights = LightIndicesCounter[Idx3DToLinear(VoxelCoord.x, VoxelCoord.y, VoxelCoord.z)];

		if (Numlights > 0) {

			int SampleLight = int(round(hash2().x * float(Numlights - 1)));
			uint LightIdx = 0; ;

			if (SampleLight == 0) {
				LightIdx = imageLoad(o_LightCullData, ivec3(VoxelCoord.xyz)).x;
			} else if (SampleLight == 1) {
				LightIdx = imageLoad(o_LightCullData1, ivec3(VoxelCoord.xyz)).x;
			} else if (SampleLight == 2) {
				LightIdx = imageLoad(o_LightCullData2, ivec3(VoxelCoord.xyz)).x;
			} else if (SampleLight == 3) {
				LightIdx = imageLoad(o_LightCullData3, ivec3(VoxelCoord.xyz)).x;
			}

			LightIdx--;

			if (LightIdx >= 0) {

				SphereLight Light = SphereLights[LightIdx];

				RayDirection = normalize(Light.PositionRadius.xyz - RayOrigin);
			}
		}
	}
	
	// First bounce intersection outputs 
	vec4 TUVW = vec4(-1.0f); 
	vec4 Albedo = vec4(0.0f);
	vec3 iNormal = vec3(-1.0f);
		
	vec3 FinalRadiance = vec3(0.0f);
	
	vec4 Screentrace = vec4(-10.0f);
	
	if (DO_SCREENTRACE) 
	{ 
		Screentrace = ScreenspaceRaytrace(u_DepthTexture, RayOrigin, RayDirection, 16, 8, 0.00113f, 32.0f);
	}

	if (IsInScreenspace(Screentrace.xy) && Screentrace.z > 0.0f) {
			    
		vec3 IntersectionPosition = RayOrigin + RayDirection * Screentrace.z;
		TUVW.x = Screentrace.z;

		vec4 NormalFetchSS = TexelFetchNormalized(u_NormalTexture, Screentrace.xy);

		iNormal = NormalFetchSS.xyz;
		Albedo = vec4(TexelFetchNormalized(u_Albedo, Screentrace.xy).xyz, NormalFetchSS.w);

		//Albedo.xyz = pow(Albedo.xyz, vec3(1.0f / 2.2f));

		FinalRadiance = GetDirect(IntersectionPosition, iNormal.xyz, Albedo.xyz) + (NormalFetchSS.w * Albedo.xyz);
	}

	// Screenspace trace failed, intersect full geometry.
	else if (true) {

		int IntersectedMesh = -1;
		int IntersectedTri = -1;
			
		// Intersect ray 
		IntersectRayIgnoreTransparentFast(RayOrigin, RayDirection, TUVW, IntersectedMesh, IntersectedTri, Albedo, iNormal);

		if (dot(iNormal, RayDirection) > 0.0001f) {
			iNormal = -iNormal;
		}

		FinalRadiance = TUVW.x < 0.0f ? texture(u_Skymap, RayDirection).xyz * 2.0f :
						 (GetDirect((RayOrigin + RayDirection * TUVW.x), iNormal, Albedo.xyz) + Albedo.xyz * Albedo.w);
	}

	float AO = TUVW.x > 0.0f ? pow(clamp(TUVW.x / 2.4f, 0.0f, 1.0f), 1.23f) : 1.0f;

	// Handle multibounce lighting 
	if (DO_SECOND_BOUNCE) {

		vec3 Bounced = vec3(0.0f);
		vec3 HitPosition = RayOrigin + RayDirection * TUVW.x;

		if (RT_SECOND_BOUNCE) {
			
			int SecondaryBounces = u_SecondaryBounces;

			vec3 Throughput = Albedo.xyz;

			for (int i = 0 ; i < SecondaryBounces ; i++) {

				int SecondIntersectedMesh = -1;
				int SecondIntersectedTri = -1;
				vec4 SecondTUVW = vec4(-1.0f);
				vec4 SecondAlbedo = vec4(0.0f);
				vec3 SecondiNormal = vec3(-1.0f);

				vec3 SecondRayOrigin = HitPosition+iNormal*0.02f;
				vec3 SecondRayDirection = CosWeightedHemisphere(iNormal,hash2());
				IntersectRayFast(SecondRayOrigin, SecondRayDirection, SecondTUVW, SecondIntersectedMesh, SecondIntersectedTri, SecondAlbedo, SecondiNormal);
				
				if (dot(SecondiNormal, SecondRayDirection) > 0.0001f) {
					SecondiNormal = -SecondiNormal;
				}

				Bounced += (TUVW.x < 0.0f ? texture(u_Skymap, SecondRayDirection).xyz * 2.2f : GetDirect((SecondRayOrigin + SecondRayDirection * SecondTUVW.x), SecondiNormal, SecondAlbedo.xyz) + SecondAlbedo.w * SecondAlbedo.xyz) * Throughput;
				
				if (TUVW.x < 0.0f) {
					break;
				}
				
				Throughput *= SecondAlbedo.xyz;
				HitPosition = SecondRayOrigin + SecondRayDirection * SecondTUVW.x;
				iNormal = SecondiNormal;
			}

			FinalRadiance += Bounced;
		}

		else {
			
			vec3 SamplePointP = ((HitPosition + iNormal * 0.01f) - u_ProbeBoxOrigin) / u_ProbeBoxSize; 
			SamplePointP = SamplePointP * 0.5 + 0.5; 

			if (SamplePointP == clamp(SamplePointP, 0.00001f, 0.99999f)) {
				const float Strength = 1.0f; 

				vec3 InterpolatedRadiance = SampleProbes(HitPosition + iNormal * 0.01f, iNormal);

				// Probe gi tends to leak at edges
				float LeakTransversalWeight = pow(TUVW.x > 0.0f ? pow(clamp(TUVW.x / 1.125f, 0.0f, 1.0f), 1.0f) : 1.0f, 1.7f);
				Bounced = clamp(InterpolatedRadiance * Strength, 0.0f, 20.0f) * LeakTransversalWeight;
			}

			else {
				
				bool SampleScreen = false;
				bool SampleApprox = true;

				if (SampleScreen) {
					
					vec3 SecondRayOrigin = HitPosition+iNormal*0.02f;
					vec3 SecondRayDirection = CosWeightedHemisphere(iNormal,hash2());
					vec4 SecondScreentrace;
					
					SecondScreentrace = ScreenspaceRaytrace(u_DepthTexture, SecondRayOrigin, SecondRayDirection, 6, 4, 0.006f, 16.0f);

					if (IsInScreenspace(SecondScreentrace.xy) && SecondScreentrace.z > 0.0f) {
						
						vec3 IntersectionPosition = SecondRayOrigin + SecondRayDirection * SecondScreentrace.z;

						vec3 Reprojected = Reprojection(IntersectionPosition);

						if (IsInScreenspace(Reprojected.xy)) {

							SampleApprox = false;

							Bounced = texture(u_IndirectDiffuse, Reprojected.xy).xyz * 0.5f;
							
						}

						//vec4 NormalFetchSS = TexelFetchNormalized(u_NormalTexture, SecondScreentrace.xy);
						//vec4 SAlbedo = vec4(TexelFetchNormalized(u_Albedo, SecondScreentrace.xy).xyz, NormalFetchSS.w);

						//Albedo.xyz = pow(Albedo.xyz, vec3(1.0f / 2.2f));

						//Bounced = GetDirect(IntersectionPosition, NormalFetchSS.xyz, SAlbedo.xyz) + (NormalFetchSS.w * SAlbedo.xyz);
						Bounced *= PI;

					}

				}

				if (SampleApprox) {

					//Point not in probe range, sample probes approximately 
					vec3 NudgedPosition = clamp(SamplePointP, 0.0002f, 0.9998f); 
					NudgedPosition = u_ProbeBoxOrigin + (u_ProbeBoxSize * (2.0f * NudgedPosition - 1.0f));
					
					float DistanceError = distance(NudgedPosition, HitPosition);
					
					if (DistanceError <= 16.0f) {
						float Weight = 1.0f;// max(1.0f - clamp(DistanceError / 16.0f, 0.0f, 1.0f), 0.15f); 
						vec3 InterpolatedRadiance = SampleProbes(NudgedPosition, iNormal);
						Bounced = Weight * InterpolatedRadiance * 1.0f;
					}
					
					else {
						Bounced = vec3(0.07f) + (texture(u_Skymap, vec3(0.0f, 1.0f, 0.0f)).xyz * 0.03f);
					}

				}
			}

			float ShotRayProbability = clamp(dot(Normal, RayDirection), 0.0f, 1.0f);

			vec3 Throughput = Albedo.xyz;

			// Multiply by PI to correct for radiance brightness loss due to SH
			// (Hemispherical harmonic basis)
			FinalRadiance += Bounced * Throughput * PI;

			//if (RayProbability > 0.000001f) {
			//	vec3 Throughput = Albedo.xyz; 
			//	FinalRadiance += Bounced * clamp(Throughput, 0.0f, 5.0f);
			//}
			//
			//else {
			//	FinalRadiance += Bounced * Albedo.xyz;
			//}
		}


	}


	// Handle glass indirect caustics in screenspace 

	#ifdef DO_INDIRECT_CAUSTICS
		vec3 TintColor = vec3(1.0f);
		
		vec4 Glasstrace = vec4(-1.0f);

		vec3 GDirection = mix(Normal, CosWeightedHemisphere(Normal, hash2()), 0.9f);
		Glasstrace = ScreenspaceRaytrace(u_TransparentDepth, RayOrigin, GDirection, 6, 3, 0.005f, 5.0f);

		if (IsInScreenspace(Glasstrace.xy) && Glasstrace.z > 0.0f && Glasstrace.xy == clamp(Glasstrace.xy, 0.001f, 0.999f)) {
			vec3 TintAlbedo = TexelFetchNormalized(u_TransparentAlbedo, Glasstrace.xy).xyz;
			TintColor = pow(TintAlbedo, vec3(1.2)) * 2.8f;
		}

		FinalRadiance *= TintColor;
	#endif

	if (!IsValid(FinalRadiance)) {
		FinalRadiance = vec3(0.0f);
	}

	if (!IsValid(AO)) {
		AO = 1.0f;
	}

	imageStore(o_OutputData, WritePixel, vec4(FinalRadiance, AO));
}
