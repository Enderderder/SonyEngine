/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __VSOUTPUT_H__
#define __VSOUTPUT_H__

struct VS_OUTPUT_BG
{
    float4 Position     : S_POSITION;
	float2 vTexST		: TEXCOORD0;
};

struct VS_OUTPUT_BG2
{
    float4 Position     : S_POSITION;
	float3 vTexST		: TEXCOORD0;
};

#endif
