/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __VSOUTPUT_H__
#define __VSOUTPUT_H__

struct VS_OUTPUT
{
    float4 Position     : S_POSITION;
    float4 vPos			: TEXCOORD0;
    float4 vNrm			: TEXCOORD1;
};

struct VS_OUTPUT_NMAP
{
    float4 Position     : S_POSITION;
    float4 vPos			: TEXCOORD0;
    float4 vNrm			: TEXCOORD1;
	float3 vTang		: TEXCOORD2;
	float3 vBiTang		: TEXCOORD3;
};


#ifdef USE_TSPACE
#define VS_RET		VS_OUTPUT_NMAP
#else
#define VS_RET		VS_OUTPUT
#endif

#endif