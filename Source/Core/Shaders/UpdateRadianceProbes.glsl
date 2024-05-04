#version 450 core 
#define COMPUTE

#define PI 3.1415926535

#include "TraverseBVH.glsl"
#include "Include/Octahedral.glsl"
#include "Include/SphericalHarmonics.glsl"
#include "Include/Utility.glsl"
#include "Include/ColorConstants.h"

layout(local_size_x = 8, local_size_y = 4, local_size_z = 8) in;

layout(rgba32ui, binding = 0) uniform uimage3D o_SHOutputA; // 2 normalized floats in each channel, 8 normalized floats in a single texture 
layout(rgba32ui, binding = 1) uniform uimage3D o_SHOutputB;

layout(r11f_g11f_b10f, binding = 2) uniform image3D o_CurrentRaw;
layout(r11f_g11f_b10f, binding = 3) uniform readonly image3D u_PrevRaw;

layout(rgba32ui, binding = 4) uniform readonly uimage3D u_PrevFrameSHA; 
layout(rgba32ui, binding = 5) uniform readonly uimage3D u_PrevFrameSHB;

// Reproject lighting to voxel volume to get an approximate voxel representation of the scene
///layout(rgba16f, binding = 8) uniform image3D o_VoxelVolume; 

uniform vec3 u_BoxOrigin;
uniform vec3 u_Resolution;
uniform vec3 u_Size;

uniform mat4 u_ShadowMatrices[5]; // <- shadow matrices 
uniform sampler2D u_ShadowTextures[5]; // <- the shadowmaps themselves 
uniform float u_ShadowClipPlanes[5]; // <- world space clip distances 

uniform vec3 u_SunDirection;
uniform float u_Time;

uniform vec3 u_PreviousOrigin;

uniform bool u_Temporal;

uniform usampler3D u_PreviousSHA;
uniform usampler3D u_PreviousSHB;

uniform samplerCube u_Skymap;

uniform vec3 u_VoxelRange;
uniform vec3 u_VoxelRes;

uniform float u_MaxFrames;

struct ProbeMapPixel {
	vec2 Packed;
};

layout (std430, binding = 2) buffer SSBO_ProbeMaps {
	ProbeMapPixel MapData[]; // x has luminance data, y has packed depth and depth^2
};

ivec3 Get3DIdx(int idx, ivec3 GridSize)
{
	int z = idx / (GridSize.x * GridSize.y);
	idx -= (z * GridSize.x * GridSize.y);
	int y = idx / GridSize.x;
	int x = idx % GridSize.x;
	return ivec3(x, y, z);
}

int Get1DIdx(ivec3 index, ivec3 GridSize)
{
    return (index.z * GridSize.x * GridSize.y) + (index.y * GridSize.x) + GridSize.x;
}

int Get1DIdx(ivec2 Coord, ivec2 GridSize) {
	return (Coord.x * GridSize.x) + Coord.y;
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
	
	float Bias = 0.000125f;
	vec2 SampleUV = ProjectionCoordinates.xy;
	Shadow = float(ProjectionCoordinates.z - Bias > SampleShadowMap(SampleUV, ClosestCascade)); 
	return 1.0f - Shadow;
}

vec3 GetDirect(in vec3 WorldPosition, in vec3 Normal, in vec3 Albedo) {

	float Shadow = GetDirectShadow(WorldPosition, Normal);
	return max(vec3(Albedo) * SUN_COLOR_PROBES * Shadow * clamp(dot(Normal, -u_SunDirection), 0.0f, 1.0f), 0.0f);
}

float HASH2SEED = 0.0f;
vec2 hash2() 
{
	return fract(sin(vec2(HASH2SEED += 0.1, HASH2SEED += 0.1)) * vec2(43758.5453123, 22578.1459123));
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
	//uvec4 A = texelFetch(u_PreviousSHA, Texel, 0);
	//uvec4 B = texelFetch(u_PreviousSHB, Texel, 0);

	uvec4 A = imageLoad(u_PrevFrameSHA, Texel);
	uvec4 B = imageLoad(u_PrevFrameSHB, Texel);
	return UnpackSH(A,B);
}

float GetVisibility(ivec3 Texel, vec3 WorldPosition, vec3 Normal) {

	vec3 TexCoords = vec3(Texel) / u_Resolution;
	vec3 Clip = TexCoords * 2.0f - 1.0f;
	vec3 ProbePosition = u_PreviousOrigin + Clip * u_Size;

	vec3 Vector = ProbePosition - WorldPosition;
	float Length = length(Vector);
	Vector /= Length;

	float Weight = pow(clamp(dot(Normal, Vector), 0.0f, 1.0f), 2.0f);
	return Weight;
}

vec3 SampleProbes(vec3 WorldPosition, vec3 N) {

	WorldPosition += N * 0.45f;

	vec3 SamplePoint = (WorldPosition - u_PreviousOrigin) / u_Size; 
	SamplePoint = SamplePoint * 0.5 + 0.5; 

	if (SamplePoint == clamp(SamplePoint, 0.0f, 1.0f)) {
		
		vec3 VolumeCoords = SamplePoint * (u_Resolution);
		
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


vec3 SampleProbes(vec3 WorldPosition, vec3 N, out vec3 Min, out vec3 Max) {

	WorldPosition += N * 0.45f;

	vec3 SamplePoint = (WorldPosition - u_PreviousOrigin) / u_Size; 
	SamplePoint = SamplePoint * 0.5 + 0.5; 

	vec3 MinB = vec3(-1000.0f);
	vec3 MaxB = vec3(1000.0f);

	if (SamplePoint == clamp(SamplePoint, 0.0f, 1.0f)) {
		
		MinB *= -1.0f;
		MaxB *= -1.0f;

		vec3 VolumeCoords = SamplePoint * (u_Resolution);
		
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
			vec3 Radiosity = SampleSH(sh[i]);
			MinB = min(MinB, Radiosity);
			MaxB = max(MaxB, Radiosity);
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

vec4 Reproject(vec3 WorldPosition) {

	vec3 SamplePoint = (WorldPosition - u_PreviousOrigin) / u_Size; 
	SamplePoint = SamplePoint * 0.5 + 0.5;
	
	if (SamplePoint.x > 0.0f && SamplePoint.x < 1.0f &&
		SamplePoint.y > 0.0f && SamplePoint.y < 1.0f &&
		SamplePoint.z > 0.0f && SamplePoint.z < 1.0f) {
	
		SamplePoint *= u_Resolution;
		SamplePoint = SamplePoint + 0.5f;
		return vec4(SamplePoint, 5.0f);
	}

	return vec4(-10.0f);
}

vec3 SampleCone(vec2 Xi, float CosThetaMax) 
{
    float CosTheta = (1.0f - Xi.x) + Xi.x * CosThetaMax;
    float SinTheta = sqrt(1.0f - CosTheta * CosTheta);
    float phi = Xi.y * PI * 2.0f;
    vec3 L;
    L.x = SinTheta * cos(phi);
    L.y = SinTheta * sin(phi);
    L.z = CosTheta;
    return L;
}

vec3 SampleDirectionCone(vec3 L) {

	const vec3 Basis = vec3(0.0f, 1.0f, 1.0f);
	vec3 T = normalize(cross(L, Basis));
	vec3 B = cross(T, L);
	mat3 TBN = mat3(T, B, L);
	const float CosTheta = 0.987525f; 
	vec3 ConeSample = TBN * SampleCone(hash2(), CosTheta);
	return normalize(ConeSample);
}

vec3 CosWeightedHemisphere(const vec3 n, vec2 r) 
{
	float PI2 = 2.0f * 3.1415926f;
	vec3  uu = normalize(cross(n, vec3(0.0,1.0,1.0)));
	vec3  vv = cross(uu, n);
	float ra = sqrt(r.y);
	float rx = ra * cos(PI2 * r.x); 
	float ry = ra * sin(PI2 * r.x);
	float rz = sqrt(1.0 - r.y);
	vec3  rr = vec3(rx * uu + ry * vv + rz * n );
    return normalize(rr);
}

vec3 LambertBRDF(vec3 Hash)
{
    float phi = 2.0f * PI * Hash.x; // 0 -> 2 pi
    float cosTheta = 2.0f * Hash.y - 1.0f; // -1 -> 1
    float u = Hash.z;
    float theta = acos(cosTheta);
    float r = pow(u, 1.0 / 3.0); // -> bias 
    float x = r * sin(theta) * cos(phi);
    float y = r * sin(theta) * sin(phi);
    float z = r * cos(theta);
    return vec3(x, y, z);
}

// Take luminance of radiance as probability distribution
vec3 ImportanceSample(int PixelStartOffset) {

	bool ShouldImportanceSample = false;

	if (!ShouldImportanceSample) {
		vec3 HashL = vec3(hash2(), hash2().x);
		vec3 LambertSample = LambertBRDF(HashL);
		LambertSample = normalize(LambertSample);

		return LambertSample;
	}

	float Hash = hash2().x * 2.;

	float Sum = 0.0f;

	for (int x = 0 ; x < 8 ; x++) {
		
		for (int y = 0 ; y < 8 ; y++) {

			int SamplePixelOffset = Get1DIdx(ivec2(x,y), ivec2(8)) + PixelStartOffset;
			vec2 Packed = MapData[SamplePixelOffset].Packed;

			Sum += Packed.x;

			if (Sum >= Hash) {
					
				vec2 UV = vec2(x,y) / vec2(8.0f);

				vec3 ImportantDirection = normalize(OctahedronToUnitVector(UV));
				return SampleDirectionCone(ImportantDirection);
			}
		}

	}

	vec3 HashL = vec3(hash2(), hash2().x);
    vec3 LambertSample = LambertBRDF(HashL);
    LambertSample = normalize(LambertSample);

	return LambertSample;
}

void main() {

	ivec3 Pixel = ivec3(gl_GlobalInvocationID.xyz);

	vec3 TexCoords = vec3(Pixel) / u_Resolution;

    HASH2SEED = ((TexCoords.x * TexCoords.y * TexCoords.z) * 128.0 * u_Time) + (TexCoords.z * TexCoords.y); hash2();

	vec3 Clip = TexCoords * 2.0f - 1.0f;
	vec3 RayOrigin = u_BoxOrigin + Clip * u_Size;
	const int PerProbePixelCount = 8 * 8;
	int ProbeMapPixelStartOffset = (Get1DIdx(Pixel,ivec3(u_Resolution)) * PerProbePixelCount);

	vec3 Unjittered = RayOrigin;

	//RayOrigin += vec3(hash2(), hash2().x) * 0.125f;

	vec4 Reprojected = Reproject(Unjittered);

	vec3 DiffuseDirection = ImportanceSample(ProbeMapPixelStartOffset);
    
	// Intersect ray 

    // Outputs 
    int IntersectedMesh = -1;
    int IntersectedTri = -1;
	vec4 TUVW = vec4(-1.0f);
	vec4 Albedo = vec4(0.0f);
	vec3 iNormal = vec3(-1.0f);
	
	IntersectRay(RayOrigin, DiffuseDirection, TUVW, IntersectedMesh, IntersectedTri, Albedo, iNormal);

	if (dot(iNormal, DiffuseDirection) > 0.0001f) {
		iNormal = -iNormal;
	}

	// Integrate radiance for point 
	vec3 iWorldPos = (RayOrigin + DiffuseDirection * TUVW.x);

	float LambertSky = clamp(dot(DiffuseDirection, vec3(0.0f, 1.0f, 0.0f)), 0.0f, 1.0f);

	vec3 FinalRadiance = TUVW.x < 0.0f ? texture(u_Skymap, DiffuseDirection).xyz * 2.2f * LambertSky : 
						 (GetDirect(iWorldPos, iNormal, Albedo.xyz) + (Albedo.xyz*Albedo.w*1.0f));

	//// Write voxel data
	//if (TUVW.x > 0.0f) {
	//	vec3 VoxelPositionNormalized = (iWorldPos.xyz - u_BoxOrigin) / u_Size;
	//	ivec3 VoxelCoords = ivec3(VoxelPositionNormalized * u_VoxelRange);
	//	vec4 Prev = imageLoad(o_VoxelVolume, VoxelCoords);
	//	imageStore(o_VoxelVolume, VoxelCoords, mix(vec4(Albedo.xyz, 1.0f),Prev,0.95f)); 
	//}

	float Packed = 1.0f;

	if (TUVW.x > 0.0f) {
		//vec3 Dither = (hash2().x > 0.5f ? -1.0f : 1.0f) * vec3(hash2(), hash2().x) * 0.1f;
		vec3 Bounce = SampleProbes((iWorldPos + iNormal * 0.001f),iNormal);
		const float AttenuationBounce = 0.999f; 
		FinalRadiance += Bounce * AttenuationBounce * Albedo.xyz; // Multiply by albedo for throughput 
	}

	// Write map data 
	vec2 Octahedral = UnitVectorToOctahedron(DiffuseDirection);
	ivec2 OctahedralMapPixel = ivec2((Octahedral * vec2(7.0f)));
	int PixelOffset = ProbeMapPixelStartOffset + Get1DIdx(OctahedralMapPixel, ivec2(8));

	float Depth = TUVW.x < 0.0f ? 256.0f : TUVW.x;
	float DepthSqr = Depth * Depth;

	// Write sh -> 
	SH FinalSH = EncodeSH(FinalRadiance, DiffuseDirection);

	// Succcessful reprojection
	int AccumulatedFrames = 0;

	if (Reprojected.w > 0.01f)
	{
		SH PreviousSH; 

		uvec4 A = texelFetch(u_PreviousSHA, ivec3(Reprojected.xyz), 0);
		uvec4 B = texelFetch(u_PreviousSHB, ivec3(Reprojected.xyz), 0);

		AccumulatedFrames = int(B.w) + 1;

		float TemporalAlpha = u_Temporal ? min((1.0f - (1.0f / float(AccumulatedFrames))), 1.0f - (1.0f / u_MaxFrames)) : 0.0f;

		ProbeMapPixel PrevProbeData = MapData[PixelOffset];
		vec2 WriteMoments = mix(vec2(Depth, DepthSqr), PrevProbeData.Packed.xy, TemporalAlpha);
		MapData[PixelOffset] = ProbeMapPixel(WriteMoments);

		PreviousSH = UnpackSH(A,B);

		FinalSH.L00 = mix(FinalSH.L00, PreviousSH.L00, TemporalAlpha);
		FinalSH.L11 = mix(FinalSH.L11, PreviousSH.L11, TemporalAlpha);
		FinalSH.L10 = mix(FinalSH.L10, PreviousSH.L10, TemporalAlpha);
		FinalSH.L1_1 = mix(FinalSH.L1_1, PreviousSH.L1_1, TemporalAlpha);
		FinalRadiance = mix(FinalRadiance, imageLoad(u_PrevRaw,ivec3(Reprojected.xyz)).xyz, TemporalAlpha);
	}

	else {
		MapData[PixelOffset] = ProbeMapPixel(vec2(Depth, DepthSqr));
	}

	uvec4 PackedA, PackedB;

	PackSH(FinalSH, PackedA, PackedB);
	
	PackedB.w = uint(AccumulatedFrames);

	imageStore(o_SHOutputA, Pixel, PackedA);
	imageStore(o_SHOutputB, Pixel, PackedB);
	imageStore(o_CurrentRaw, Pixel, vec4(FinalRadiance,0.));
}


