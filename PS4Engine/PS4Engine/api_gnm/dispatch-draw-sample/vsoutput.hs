/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2013 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __VSOUTPUT_H__
#define __VSOUTPUT_H__

#ifdef ENABLE_VRB
#define VRB vrb
#else
#define VRB
#endif

//#define ENABLE_PACKED
#ifdef ENABLE_PACKED

#pragma argument (-packing-parameter)
struct VS_OUTPUT
{
    VRB float4 Position		: S_POSITION;
	float3 vNorm			: TEXCOORD1;
	VRB float3 vPosInView	: TEXCOORD5;
    float2 TextureUV		: TEXCOORD0;
    float4 vTang			: TEXCOORD3;
};

#else

struct VS_OUTPUT
{
    VRB float4 Position		: S_POSITION;
    float2 TextureUV		: TEXCOORD0;
	float3 vNorm			: TEXCOORD1;
    float4 vTang			: TEXCOORD3;
	float3 vPosInView		: TEXCOORD5;
};

#endif

#endif