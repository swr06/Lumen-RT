#version 440 core 
#define COMPUTE

#define ROUGH_REFLECTIONS

#define PI 3.14159265359

#include "TraverseBVH.glsl"
#include "Include/Utility.glsl"
#include "Include/SpatialUtility.glsl"
#include "Include/Sampling.glsl"
#include "Include/SphericalHarmonics.glsl"
#include "Include/ColorConstants.h"

layout(local_size_x = 16, local_size_y = 16) in;
layout(rgba16f, binding = 0) uniform image2D o_OutputData;

uniform sampler2D u_Depth;
uniform sampler2D u_HFNormals;
uniform sampler2D u_LFNormals;
uniform sampler2D u_PBR;
uniform sampler2D u_Albedo; 

uniform float u_Time;
uniform int u_Frame;

uniform vec2 u_Dimensions; 

uniform mat4 u_Projection;
uniform mat4 u_View;
uniform mat4 u_InverseProjection;
uniform mat4 u_InverseView;
uniform mat4 u_ViewProjection;

uniform vec3 u_SunDirection;

uniform float u_zNear;
uniform float u_zFar;

uniform sampler2D u_IndirectDiffuse;
uniform sampler2D u_MotionVectors;

uniform sampler2D u_BlueNoise;

uniform mat4 u_ShadowMatrices[5]; // <- shadow matrices 
uniform sampler2D u_ShadowTextures[5]; // <- the shadowmaps themselves 
uniform float u_ShadowClipPlanes[5]; // <- world space clip distances 

uniform samplerCube u_SkyCube;

uniform vec3 u_ProbeBoxSize;
uniform vec3 u_ProbeGridResolution;
uniform vec3 u_ProbeBoxOrigin;

uniform usampler3D u_SHDataA;
uniform usampler3D u_SHDataB;

uniform bool u_Checker;

uniform bool u_FullRT;
uniform bool u_RoughSpec;

uniform bool DO_BL_SAMPLING;


float LinearizeDepth(float depth)
{
	return (2.0 * u_zNear) / (u_zFar + u_zNear - depth * (u_zFar - u_zNear));
}

float HASH2SEED = 0.0f;
vec2 hash2() 
{
	return fract(sin(vec2(HASH2SEED += 0.1, HASH2SEED += 0.1)) * vec2(43758.5453123, 22578.1459123));
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

vec3 WorldPosFromDepth(float depth, vec2 txc)
{
    float z = depth * 2.0 - 1.0;
    vec4 ClipSpacePosition = vec4(txc * 2.0 - 1.0, z, 1.0);
    vec4 ViewSpacePosition = u_InverseProjection * ClipSpacePosition;
    ViewSpacePosition /= ViewSpacePosition.w;
    vec4 WorldPos = u_InverseView * ViewSpacePosition;
    return WorldPos.xyz;
}

vec3 StochasticReflectionDirection(vec3 Incident, vec3 Normal, float Roughness) {
    
    //#define USE_RAW_DIRECTION 
    if (Roughness < 0.01f) {
            return reflect(Incident, Normal);
    }

    #ifdef USE_RAW_DIRECTION
        vec3 Sample = GGX_VNDF(Normal, Roughness, hash2());
        return Sample;
    #else

        float NearestDot = -100.0f;

        bool FoundValid = false;

        const vec2 TailControl = vec2(0.8f, 0.7f); // <- get the ggx tail under control and reduce variance 

        vec3 Microfacet = Normal;

        for (int i = 0 ; i < 12 ; i++) {
            
            vec3 Sample = SampleGGXVNDF(Normal, Roughness, hash2() * TailControl);

            if (dot(Sample, Normal) > 0.001f) // <- sampling the vndf can actually generate directions that are away from the normal.
            {
                Microfacet = Sample;
                break;
            }
        }

        return reflect(Incident, Microfacet);
    #endif
}

vec4 ScreenspaceRaytrace(const vec3 Origin, const vec3 Direction, const int Steps, const int BinarySteps, const float ThresholdMultiplier) {

    float TraceDistance = 32.0f;

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
        float DepthAt = texture(u_Depth, ProjectedRayScreenspace.xy).x; 

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
                float Fetch = texture(u_Depth, Projected.xy).x;
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
	
		ProjectionCoordinates = u_ShadowMatrices[Cascade] * vec4(WorldPosition + N * 0.0275f, 1.0f);

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
	
	float Bias = 0.00f;
	vec2 SampleUV = ProjectionCoordinates.xy;
	Shadow = float(ProjectionCoordinates.z - Bias > SampleShadowMap(SampleUV, ClosestCascade)); 
	return 1.0f - Shadow;
}

// Irradiance probe grid sampling 
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

// Visibility weight 
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

// Samples probes at a position in the world
vec3 SampleProbes(vec3 WorldPosition, vec3 N) {

	WorldPosition += N * 0.05f;

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

// Computes final lighting at a point 
vec3 SampleLighting(in vec2 TexCoords, in vec3 WorldPosition, in vec3 Normal, in vec3 Albedo) { // For screen traces 

	vec2 MotionVector = texture(u_MotionVectors, TexCoords).xy;

	float L = length(MotionVector);
	float Shadow = GetDirectShadow(WorldPosition, Normal);

	if (L < 0.02f) {

		vec4 DiffuseIndirect = texture(u_IndirectDiffuse, TexCoords + MotionVector).xyzw;
		return (vec3(Albedo) * SUN_COLOR_SPEC * Shadow) + (Albedo * DiffuseIndirect.xyz * DiffuseIndirect.w * DiffuseIndirect.w); 
	
	}

	else {
		
		vec3 DiffuseIndirect = SampleProbes(WorldPosition,Normal);
		float Shadow = GetDirectShadow(WorldPosition, Normal);
		return (vec3(Albedo) * SUN_COLOR_SPEC * Shadow) + (Albedo * DiffuseIndirect.xyz); 

	}

}

vec3 SampleLighting(in vec3 WorldPosition, in vec3 Normal, in vec3 Albedo) { // For global/world traces 

	vec3 DiffuseIndirect = SampleProbes(WorldPosition,Normal);
	float Shadow = GetDirectShadow(WorldPosition, Normal);
	return (vec3(Albedo) * SUN_COLOR_SPEC * Shadow) + (Albedo * 2.0f * DiffuseIndirect.xyz); 
}

float SeedFunction(float Input) {
	float Factor = 42.0f;
	return clamp(floor(Input * Factor) / float(Factor), 0.0f, 4.0f);
	//return Input;
	//return (Input * Input) * (3.0 - 2.0 * Input);
	//return Input * (2.0f - Input);
}

vec3 Sky(vec3 D, float R) {
	return pow(texture(u_SkyCube, D).xyz, 1.0.xxx) * mix(1.4f, 2.65f, R);
}


int TRACE_MODE = 0; 

void main() {
    
    ivec2 Pixel = ivec2(gl_GlobalInvocationID.xy);
	ivec2 WritePixel = ivec2(gl_GlobalInvocationID.xy);

	if (Pixel.x < 0 || Pixel.y < 0 || Pixel.x > int(u_Dimensions.x) || Pixel.y > int(u_Dimensions.y)) {
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
    vec2 HighResUV = vec2(HighResPixel) / textureSize(u_Depth, 0).xy;

	// Fetch 
    float Depth = texelFetch(u_Depth, HighResPixel, 0).x;

	if (Depth > 0.999999f || Depth == 1.0f) {
		imageStore(o_OutputData, WritePixel, vec4(0.0f));
        return;
    }

    vec3 PBR = texelFetch(u_PBR, HighResPixel, 0).xyz;
#ifndef ROUGH_REFLECTIONS
	PBR.x *= 0.0f;
#endif

	PBR.x *= float(u_RoughSpec);

	vec2 TexCoords = clamp(HighResUV, 0.0f, 1.0f);

	if (PBR.x > 0.96f) {
		vec4 SpecularHemisphere = vec4(max(texture(u_IndirectDiffuse, clamp(TexCoords, 0.01f, 0.99f)).xyz, 0.0f), 1.0f);
		imageStore(o_OutputData, WritePixel, vec4(SpecularHemisphere));
		return;
	}

	if (PBR.y > 0.04f || u_FullRT) {
		TRACE_MODE = 1;
	}

	HASH2SEED = u_Time;

	if (DO_BL_SAMPLING) {
		
		ivec2 CoordB = WritePixel % 1024;
		vec3 BlueNoise = texelFetch(u_BlueNoise, CoordB, 0).xyz;
		float SeedOffset = SeedFunction(BlueNoise.x);
		HASH2SEED *= 10.0f;
		HASH2SEED += SeedOffset * 10.0f;
	}

	else {
		HASH2SEED *= (TexCoords.x * TexCoords.y) * 64.0;
	}

	const vec3 Player = u_InverseView[3].xyz;

	vec3 WorldPosition = WorldPosFromDepth(Depth, TexCoords);
	vec3 Normal = normalize(texelFetch(u_LFNormals, HighResPixel, 0).xyz);
	vec3 NormalHF = texelFetch(u_HFNormals, HighResPixel, 0).xyz;

	vec3 Incident = normalize(WorldPosition - Player);

    vec3 RayOrigin = WorldPosition + Normal * mix(0.05f, 0.1f, clamp(PBR.x*1.4f,0.0f,1.0f));
    vec3 RayDirection = StochasticReflectionDirection(Incident, Normal, PBR.x*0.9f); 

    vec3 FinalRadiance = vec3(0.0f);
    float FinalTransversal = -1.0f;

	if (TRACE_MODE == 0 || TRACE_MODE == 1) {

		//vec4 Screentrace = ScreenspaceRaytrace(RayOrigin, RayDirection, 
		//	int(mix(32, 16, clamp(PBR.x * PBR.x * 1.2f, 0.0f, 1.0f))), // <- Step count 
		//	int(mix(8, 8, clamp(PBR.x * PBR.x * 1.25f, 0.0f, 1.0f))), // <- Binary refine step count
		//	mix(0.0045f, 0.0075f, clamp(PBR.x * PBR.x * 1.25f, 0.0f, 1.0f))); // <- Error threshold

		vec4 Screentrace = ScreenspaceRaytrace(RayOrigin, RayDirection, 
			PBR.x < 0.04f ? 32 : int(mix(20.0f, 8.0f, float(clamp(PBR.x*1.4f,0.0f,1.0f)))), // <- Step count 
			int(mix(8.0f, 6.0f, float(clamp(PBR.x*1.1f,0.0f,1.0f)))), // <- Binary refine step count
		    mix(0.0011f, 0.005f, PBR.x * PBR.x) ); // <- Error threshold

		if (IsInScreenspace(Screentrace.xy) && Screentrace.z > 0.0f) {
			    
			vec3 IntersectionPosition = RayOrigin + RayDirection * Screentrace.z;
			FinalTransversal = Screentrace.z;

			vec4 IntersectionNormal = TexelFetchNormalized(u_LFNormals, Screentrace.xy);
			vec3 IntersectionAlbedo = TexelFetchNormalized(u_Albedo, Screentrace.xy).xyz;

			FinalRadiance = SampleLighting(Screentrace.xy, IntersectionPosition, IntersectionNormal.xyz, IntersectionAlbedo) + (IntersectionNormal.w * IntersectionAlbedo);
		}

		// Screenspace trace failed, intersect full geometry.
		else if (TRACE_MODE == 1) {

			// Intersection kernel outputs 
			int IntersectedMesh = -1;
			int IntersectedTri = -1;
			vec4 TUVW = vec4(-1.0f);
			vec4 IntersectionAlbedo = vec4(0.0f);
			vec3 IntersectionNormal = vec3(-1.0f);
					
			// Intersect ray 
			IntersectRay(RayOrigin, RayDirection, TUVW, IntersectedMesh, IntersectedTri, IntersectionAlbedo, IntersectionNormal);
			
			if (dot(IntersectionNormal, RayDirection) > 0.0001f) {
				IntersectionNormal = -IntersectionNormal;
			}
			
			FinalTransversal = TUVW.x;

			if (TUVW.x > 0.0f) 
				FinalRadiance = SampleLighting(RayOrigin+RayDirection*TUVW.x, IntersectionNormal, IntersectionAlbedo.xyz) + (IntersectionAlbedo.w * IntersectionAlbedo.xyz);
			else 
				FinalRadiance = Sky(RayDirection, PBR.x);
		}

		else {
		    FinalRadiance = Screentrace.w * Sky(RayDirection, PBR.x);
			FinalTransversal = 8.0f;
		}
	}

	else if (TRACE_MODE == 2) {

		// Intersection kernel outputs 
		int IntersectedMesh = -1;
		int IntersectedTri = -1;
		vec4 TUVW = vec4(-1.0f);
		vec4 IntersectionAlbedo = vec4(0.0f);
		vec3 IntersectionNormal = vec3(-1.0f);
			
		// Intersect ray 
		IntersectRay(RayOrigin, RayDirection, TUVW, IntersectedMesh, IntersectedTri, IntersectionAlbedo, IntersectionNormal);
		FinalTransversal = TUVW.x;

		FinalRadiance = SampleLighting(RayOrigin+RayDirection*TUVW.x, IntersectionNormal, IntersectionAlbedo.xyz) + (IntersectionAlbedo.w * IntersectionAlbedo.xyz);
	}

	if (!IsValid(FinalRadiance)) {
		FinalRadiance = vec3(0.0f);
		FinalTransversal = -1.0f;
	}

	FinalRadiance = clamp(FinalRadiance, 0.0f, 6.0f);

	// xyz has radiance, w has transformed transversal 
    imageStore(o_OutputData, WritePixel, vec4(FinalRadiance, TransformReflectionTransversal(FinalTransversal)));
}