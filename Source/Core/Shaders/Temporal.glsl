#version 330 core 

#include "Include/Utility.glsl"
#include "Include/SpatialUtility.glsl"

// Outputs 
layout (location = 0) out vec4 o_Diffuse;
layout (location = 1) out vec2 o_Utility;
layout (location = 2) out vec2 o_Moments;
layout (location = 3) out vec4 o_Specular;
layout (location = 4) out vec4 o_Volumetrics;

in vec2 v_TexCoords;

uniform mat4 u_InverseView;
uniform mat4 u_InverseProjection;
uniform mat4 u_Projection;
uniform mat4 u_View;
uniform mat4 u_PrevProjection;
uniform mat4 u_PrevView;
uniform mat4 u_PrevInverseProjection;
uniform mat4 u_PrevInverseView;

uniform sampler2D u_DiffuseCurrent;
uniform sampler2D u_DiffuseHistory;

uniform sampler2D u_SpecularCurrent;
uniform sampler2D u_SpecularHistory;

uniform sampler2D u_VolumetricsCurrent;
uniform sampler2D u_VolumetricsHistory;

uniform sampler2D u_Depth;
uniform sampler2D u_Normals;

uniform sampler2D u_PreviousDepth;
uniform sampler2D u_PreviousNormals;

uniform sampler2D u_MotionVectors;
uniform sampler2D u_Utility;

uniform sampler2D u_MomentsHistory;

uniform sampler2D u_PBR;

uniform float u_zNear;
uniform float u_zFar;

uniform vec3 u_ViewerPosition;

uniform bool u_DoVolumetrics;

uniform bool u_Enabled;
uniform bool u_RoughSpec;

uniform bool u_ClipDiffuse;

uniform float u_ClipStrength;

// GBuffer
float LinearizeDepth(float depth)
{
	return (2.0 * u_zNear) / (u_zFar + u_zNear - depth * (u_zFar - u_zNear));
}

vec3 GetIncident(vec2 screenspace)
{
	vec4 clip = vec4(screenspace * 2.0f - 1.0f, -1.0, 1.0);
	vec4 eye = vec4(vec2(u_InverseProjection * clip), -1.0, 0.0);
	return vec3(u_InverseView * eye);
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

vec3 PrevWorldPosFromDepth(float depth, vec2 txc)
{
    float z = depth * 2.0 - 1.0;
    vec4 ClipSpacePosition = vec4(txc * 2.0 - 1.0, z, 1.0);
    vec4 ViewSpacePosition = u_PrevInverseProjection * ClipSpacePosition;
    ViewSpacePosition /= ViewSpacePosition.w;
    vec4 WorldPos = u_PrevInverseView * ViewSpacePosition;
    return WorldPos.xyz;
}


vec3 Reprojection(vec3 WorldPos) 
{
	vec4 ProjectedPosition = u_PrevProjection * u_PrevView * vec4(WorldPos, 1.0f);
	ProjectedPosition.xyz /= ProjectedPosition.w;
	ProjectedPosition.xyz = ProjectedPosition.xyz * 0.5f + 0.5f;
	return ProjectedPosition.xyz;

}

// Gathers statistics for a pixel using spatial data
void GatherStatistics(sampler2D Texture, ivec2 Pixel, in vec4 Center, out vec4 Min, out vec4 Max, out vec4 Mean, out vec4 DiffuseMoments, bool ConvertToYCoCg) {
	
	Min = vec4(10000.0f);
	Max = vec4(-10000.0f);
	Mean = vec4(0.0f);
	float TotalWeight = 1.0f;

	for (int x = -1 ; x <= 1 ; x++) {
		
		for (int y = -1 ; y <= 1 ; y++) {
			
			vec4 Fetch = (x == 0 && y == 0) ? Center : texelFetch(Texture, Pixel + ivec2(x, y), 0);
			
			if (ConvertToYCoCg) {
				Fetch.xyz = RGB2YCoCg(Fetch.xyz);
			}

			Min = min(Min, Fetch);
			Max = max(Max, Fetch);
			Mean += Fetch;
			DiffuseMoments += Fetch * Fetch;
			TotalWeight += 1.0f;
		}

	}

	Mean /= TotalWeight;
	DiffuseMoments /= TotalWeight;
}

void main() {

	ivec2 Pixel = ivec2(gl_FragCoord.xy);
	ivec2 HighResPixel = Pixel * 2;
	ivec2 Dimensions = textureSize(u_DiffuseHistory,0).xy;

	// GBuffer
	vec3 Incident = GetIncident(v_TexCoords);
	float Depth = texelFetch(u_Depth, HighResPixel, 0).x;
	vec3 Normals = normalize(texelFetch(u_Normals, HighResPixel, 0).xyz);
	vec3 PBR = texelFetch(u_PBR, HighResPixel, 0).xyz;

	PBR.x *= float(u_RoughSpec);

	float LinearDepth = LinearizeDepth(Depth);
	vec2 DepthGradient = vec2(dFdx(LinearDepth), dFdy(LinearDepth));

	vec3 WorldPosition = WorldPosFromDepth(Depth, v_TexCoords).xyz;
	float Distance = distance(WorldPosition, u_ViewerPosition);

	// Motion vectors
	vec2 MotionVector = texelFetch(u_MotionVectors, HighResPixel, 0).xy;
	float MotionLength = length(MotionVector);

	// Reproject surface 
	vec2 Reprojected = MotionVector + v_TexCoords;

	// Sample current frame 
	vec4 CurrentDiffuse = texelFetch(u_DiffuseCurrent, Pixel, 0);
	vec4 CurrentSpecular = texelFetch(u_SpecularCurrent, Pixel, 0);
	vec4 CurrentVolumetrics = texelFetch(u_VolumetricsCurrent, Pixel, 0);
	
	o_Diffuse = CurrentDiffuse;
	o_Specular = CurrentSpecular;
	o_Volumetrics = CurrentVolumetrics;

	float DiffuseLuminance = Luminance(CurrentDiffuse.xyz);
	float DiffuseFrames = 1.0f;

	// Generate moments 
	vec2 DiffuseMoments;
	DiffuseMoments = vec2(DiffuseLuminance, DiffuseLuminance * DiffuseLuminance);
	DiffuseMoments = max(DiffuseMoments, 0.0f);

	if (!u_Enabled) {
		o_Diffuse = CurrentDiffuse;
		o_Specular = CurrentSpecular;
		o_Volumetrics = CurrentVolumetrics;
		o_Utility = vec2(0.0f);
		o_Moments = DiffuseMoments;
		return;
	}

	bool DisocclusionSurface = true;

	bool NormalDisocclusion = false;

	// Diffuse 
	if (IsInScreenspaceBiased(Reprojected)) 
	{
		
		//vec4 Variance = sqrt(abs(MomentsDiff - MeanDiff * MeanDiff));

		ivec2 ReprojectedPixel = ivec2(Reprojected.xy * vec2(Dimensions));
		float ReprojectedDepth = texture(u_PreviousDepth, Reprojected.xy).x;
		vec3 ReprojectedNormals = normalize(texture(u_PreviousNormals, Reprojected.xy).xyz);

		vec3 ReprojectedPosition = PrevWorldPosFromDepth(ReprojectedDepth, Reprojected.xy);
		
		float DistanceRelaxFactor = abs(length(DepthGradient)) * 50.0f;

		float BlendFactor = 0.0f;

		float DError = distance(ReprojectedPosition, WorldPosition); // exp(-distance(ReprojectedPosition, WorldPosition) / DistanceRelaxFactor);
		float DTolerance = MotionLength <= 0.000125f ? 32.0f : (MotionLength > 0.001f ? 1.0f : 2.7f);

		float NError = clamp(dot(ReprojectedNormals, Normals), 0.0f, 1.0f);
		float NTolerance = MotionLength <= 0.00125f ? -0.01f : 0.8f;

		if (DError < DTolerance && (NError > NTolerance || (!NormalDisocclusion)))
		{
			DisocclusionSurface = false;

			DiffuseFrames = min((texelFetch(u_Utility, ReprojectedPixel.xy, 0).x * 255.0f), 128.0f) + 1.0f;
			BlendFactor = 1.0f - (1.0f / DiffuseFrames);

			vec4 History = texture(u_DiffuseHistory, Reprojected.xy);

			//float ClipStrength = 0.5f;
			//History.xyz = (ClipToAABB(History.xyz, MeanDiff.xyz - Variance.xyz * ClipStrength, MeanDiff.xyz + Variance.xyz * ClipStrength));

			if (u_ClipDiffuse) {
				vec4 MinDiff, MaxDiff, MeanDiff, MomentsDiff;
				GatherStatistics(u_DiffuseCurrent, Pixel, CurrentDiffuse, MinDiff, MaxDiff, MeanDiff, MomentsDiff, false);

				float Bias = u_ClipStrength;
				History.xyz = ClipToAABB(History.xyz, max(MinDiff.xyz - Bias, 0.0f), MaxDiff.xyz + Bias);
			}

			//o_Diffuse.xyz = InverseReinhard(mix(Reinhard(CurrentDiffuse.xyz), Reinhard(History.xyz), BlendFactor));
			o_Diffuse.xyz = mix(CurrentDiffuse.xyz, History.xyz, BlendFactor);
				
			vec2 HistoryMoments = texture(u_MomentsHistory, Reprojected.xy).xy;
			o_Moments = mix(DiffuseMoments, HistoryMoments, BlendFactor);

			if (DError < DTolerance * 0.8f && (NError > clamp(NTolerance * 1.2f,0.2f,1.0f) || (!NormalDisocclusion))) {
				float MotionWeight = MotionLength > 0.001f ? clamp(exp(-length(MotionVector * vec2(Dimensions))) * 0.6f + 0.7f, 0.0f, 1.0f) : 1.0f;
				//History.w = ClipToAABB(History.w, MinDiff.w - 0.001f, MaxDiff.w + 0.001f);
				o_Diffuse.w = mix(CurrentDiffuse.w, History.w, min(0.9f * MotionWeight, 0.9f));
			}
		}

	}

	// Specular 

	float SpecularFrames = 1.0f;

	if (!DisocclusionSurface)
	{
		vec4 MinSpec, MaxSpec, MeanSpec, MomentsSpec;

		GatherStatistics(u_SpecularCurrent, Pixel, CurrentSpecular, MinSpec, MaxSpec, MeanSpec, MomentsSpec, true);

		vec4 Variance = sqrt(abs(MomentsSpec - MeanSpec * MeanSpec));
		float Transversal = UntransformReflectionTransversal(MeanSpec.w);

		if (Transversal < 0.0f) {
			Transversal = 10.0f;
		}

		vec3 Incident = normalize(u_ViewerPosition - WorldPosition.xyz);
		vec3 ReflectedPosition = (WorldPosition.xyz) - Incident * Transversal;
		vec3 ReprojectedReflection = PBR.x > 0.96f ? Reprojection(WorldPosition).xyz : Reprojection(ReflectedPosition).xyz;
		float SpecMotionLength = length(ReprojectedReflection.xy-v_TexCoords);

		if (IsInScreenspaceBiased(ReprojectedReflection.xy)) {
			vec4 HistorySpecular = CatmullRom(u_SpecularHistory, ReprojectedReflection.xy);

			float ClipStrength = max(mix(1.0f, 5.0f, clamp(pow(PBR.x,1.7f),0.0f,1.0f)) * (MotionLength > 0.0025f ? 0.4f : 16.0f), 0.6f);
			
			if (PBR.x < 0.11f) {
				ClipStrength = min(ClipStrength, mix(0.25f, 0.8f, clamp(PBR.x / 0.11f, 0.0f, 1.0f)));
			}
			
			HistorySpecular.xyz = YCoCg2RGB(ClipToAABB(RGB2YCoCg(HistorySpecular.xyz), MeanSpec.xyz - Variance.xyz * ClipStrength, MeanSpec.xyz + Variance.xyz * ClipStrength));

			float TransversalError = abs(UntransformReflectionTransversal(HistorySpecular.w) - Transversal);

			SpecularFrames = min((texture(u_Utility,ReprojectedReflection.xy).y * 255.0f), 28.0f) + 1.0f;
			float SpecBlendFactor = 1.0f - (1.0f / SpecularFrames);

			//if ((TransversalError < mix(4.0f, 16.0f, clamp(PBR.x*1.5f,0.,1.))) || PBR.x < 0.95)
			{
				o_Specular.xyz = InverseReinhard(mix(Reinhard(CurrentSpecular.xyz), Reinhard(HistorySpecular.xyz), SpecBlendFactor));
				o_Specular.w = mix(MeanSpec.w, HistorySpecular.w, SpecBlendFactor);
			}
		}

		if (!IsValid(o_Specular)) {
			o_Specular = vec4(0.0f);
		}
	}

	// Volumetrics 
	if (u_DoVolumetrics && !DisocclusionSurface) {
		
		float PlaneDistance = clamp(IsSky(Depth) ? 48.0f : Distance, 0.0f, 40.0f);
		vec3 ReprojectedPlane = Reprojection(u_ViewerPosition + (Incident * PlaneDistance));

		if (IsInScreenspaceBiased(ReprojectedPlane.xy)) {

			vec4 MinVol, MaxVol, MeanVol, MomentsVol;

			bool ShouldClip = MotionLength > 0.00005;

			vec4 PrevVolumetrics = texture(u_VolumetricsHistory, ReprojectedPlane.xy);

			if (ShouldClip) {
				GatherStatistics(u_VolumetricsCurrent, Pixel, CurrentSpecular, MinVol, MaxVol, MeanVol, MomentsVol, false);

				//vec4 VolVariance = sqrt(abs(MomentsVol - MeanVol * MeanVol));

				float Bias = MotionLength > 0.0001f ? 0.0325f : 0.0325f;

				PrevVolumetrics = ClipToAABB(PrevVolumetrics, MinVol - Bias, MaxVol + Bias);
			}

			float BlendFactorVol = 0.9f;

			if (MotionLength > 0.0001f) {
				BlendFactorVol = 0.65f;
			}

			o_Volumetrics = mix(CurrentVolumetrics, PrevVolumetrics, BlendFactorVol);
		}
	}

	o_Utility.x = DiffuseFrames / 255.0f;
	o_Utility.y = SpecularFrames / 255.0f;
	o_Utility = clamp(o_Utility, 0.0f, 100.0f);
}