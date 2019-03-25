/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __VSOUTPUT_H__
#define __VSOUTPUT_H__

struct VS_OUTPUT
{
	float2 texCoord  : TEXCOORD0;
    float3 color	: TEXCOORD1;
	uint fake_prim_index : TEXCOORD2;
};

struct HS_OUTPUT
{
	float2 texCoord  : TEXCOORD0;
	float3 color	: TEXCOORD1;
};

struct HS_CNST_OUTPUT
{
	float edge_ts[4]			: S_EDGE_TESS_FACTOR;
    float insi_ts[2]			: S_INSIDE_TESS_FACTOR;
};

struct DS_OUTPUT
{
	float4 vPosition			: S_POSITION;
	float3 color				: TEXCOORD0;
};


#endif