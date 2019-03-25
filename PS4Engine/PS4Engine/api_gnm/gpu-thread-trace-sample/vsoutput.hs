/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2013 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __VSOUTPUT_H__
#define __VSOUTPUT_H__

struct VS_OUTPUT
{
    float4 Position     : S_POSITION;
    float2 TextureUV    : TEXCOORD0;
	float3 vNorm		: TEXCOORD1;
    float4 vTang		: TEXCOORD3;
	float3 vPosInView	: TEXCOORD5;
};



#endif