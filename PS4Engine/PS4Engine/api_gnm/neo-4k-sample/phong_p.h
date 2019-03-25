/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "../toolkit/shader_common/shader_base.h"
#include "../toolkit/shader_common/illum.hs"

Texture2D texDiffuse				: register( t0 );
Texture2D texShadow					: register( t1 );
Texture2D texSpecular				: register( t2 );
Texture2D texNormal					: register( t3 );
SamplerState samLinear				: register( s0 );
SamplerComparisonState samShadow	: register( s1 );

#pragma argument(gradientadjust=auto)

unistruct constants
{
	float4	lightPosition;
	float4  lightColor;
	float4  ambientColor;
	float4  specularColor;
	bool	alphaTest;
	uint	debugView;
};

// due to 2xEQAA checkerboard rendering, barycentrics are evaluated at sample position 
struct VS_OUTPUT
{
    sample float4 pos					: S_POSITION;
    sample float2 texcoord				: TEXCOORD0;
    sample float3 normalView			: TEXCOORD1;
    sample float4 tangentViewAndFlip	: TEXCOORD2;
    sample float4 posLight				: TEXCOORD3;
	sample float3 posView				: TEXCOORD4;
	sample float4 posClip				: TEXCOORD5;
	sample float4 posClipPrev			: TEXCOORD6;
};

struct PS_OUTPUT
{
	float4 colour	: S_TARGET_OUTPUT0;
	float2 velocity	: S_TARGET_OUTPUT1;
};

PS_OUTPUT main( VS_OUTPUT input )
{
	float4 diffuseAndAlpha = texDiffuse.Sample( samLinear, input.texcoord);

	// when alpha unroll is enabled, the discard is moved to the alpha_depth_only shader.
#if DISCARD
	if(alphaTest && diffuseAndAlpha.w < 0.5)
	 	discard;
#endif

	float3 posShadow = input.posLight.xyz / input.posLight.w;
    	
	uint w, h;
	texShadow.GetDimensions(w, h);
	float2 duv = 1.0 / float2(w, h);

	#define SHADOW_EPSILON	0.00025f
	float zcmp = posShadow.z - SHADOW_EPSILON;

	float shadSum = 0;
	for(float x = -1.5; x <= 1.5; x += 1.0)
	{
		for(float y = -1.5; y <= 1.5; y += 1.0)
		{
			shadSum += texShadow.SampleCmpLOD0(samShadow, posShadow.xy + float2(x, y) * duv, zcmp);
		}
	}

	float lightAmount = shadSum  / 16.0;
	   
	float3 normal = normalize(input.normalView);
	float3 tangent = normalize(input.tangentViewAndFlip.xyz);
	float3 binormal = input.tangentViewAndFlip.w * cross(normal, tangent);

	// two-sided lighting
	if(dot(input.posView, normal) > 0)
		normal = -normal;

	float3 diffuse = lightColor.xyz * diffuseAndAlpha.xyz;
	float3 specular = specularColor.xyz * texSpecular.Sample(samLinear, input.texcoord).xyz;
	float roughness = 100;
	float3 ambient = 0.08 * diffuseAndAlpha.xyz;
	float3 V = normalize(-input.posView);
	float3 L = normalize(lightPosition.xyz-input.posView);
	
	float3 bumpNormal = float3(texNormal.Sample(samLinear, input.texcoord).xy, 0);
	bumpNormal.xy = bumpNormal.xy * 2.0f - 1.0f;
	bumpNormal.z = sqrt(1 - bumpNormal.x*bumpNormal.x - bumpNormal.y*bumpNormal.y);

	float3 bumpNormalView = normalize(bumpNormal.x*tangent + bumpNormal.y*binormal + bumpNormal.z*normal);
	float3 res = ambient + lightAmount * BRDF2_ts_nphong_nofr(bumpNormalView, normal, L, V, diffuse, specular, roughness);

	PS_OUTPUT output;
	output.colour = float4(res, diffuseAndAlpha.w);

	// compute velocity in NDC space
	float2 posNdc = input.posClip.xy/input.posClip.w;
	float2 posNdcPrev = input.posClipPrev.xy/input.posClipPrev.w;
	output.velocity = posNdc - posNdcPrev;

	return output;
}
