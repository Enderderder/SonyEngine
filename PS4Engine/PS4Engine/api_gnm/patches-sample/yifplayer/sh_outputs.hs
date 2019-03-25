/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __VSOUTPUT_H__
#define __VSOUTPUT_H__


struct LS_OUTPUT
{
	// by defining uIndex as a float the compiler make this a 16 byte struct instead of 32 bytes
	// we use asfloat() to avoid conversion to float
#ifndef READ_VERTS_IN_HS
	float4 PosIndex		: PASSTHROUGH;
#else
	uint uPrimIndex		: PINDEX;
#endif
};


struct HS_OUTPUT
{
	// If ACC or regular patches
#if defined(REG_PATCHES_FOUR_THREADS) || defined(ACC_PATCHES)

	float4 corner		: CORNER;
	float4 edge_l		: EDGEL;
	float4 edge_r		: EDGER;
#endif

#ifdef ACC_PATCHES
	float4 tanu			: TANU;
	float4 tanv			: TANV;
#endif

	// If Gregory Pathces
#ifdef GREGORY_PATCHES
	// the .w components of fp, fn, ep store the corner vertex p
	float4 fp			: INNERP;
	float4 fn_neigh		: INNERN;
	float4 ep			: EDGEP;
	float3 en_neigh		: EDGEN_NEIGH;
#endif

	// If 16 thread ver. of regular patches
#ifdef REG_PATCHES_SIXTEEN_THREADS
	float3 pos			: BPOS;
#endif
};

struct SQuadTangent
{
	float4 Tan0, Tan1, Tan2;
	float3 Tan3, Tan4;
	// Tan3 in the .Ws
};

struct HS_CNST_OUTPUT
{
	float edge_ts[4]			: S_EDGE_TESS_FACTOR;
    float insi_ts[2]			: S_INSIDE_TESS_FACTOR;

#ifdef ACC_PATCHES
#ifdef HS_GENERATED_TANGENTS
	SQuadTangent quad_tangents[4]	: ACCTANGENTS;
#else
	float4 vCWts					: TANWEIGHTS;
#endif

#elif defined(REG_PATCHES_FOUR_THREADS) || defined(REG_PATCHES_SIXTEEN_THREADS) || defined(GREGORY_PATCHES)
	float2	texCoord[4]				: TEXCOORDS;
#endif
};




struct VS_OUTPUT
{
	float4 vPosition		: S_POSITION;
	float4 pos				: TEXCOORD0;			// texcoord in .w of pos and norm
    float4 norm				: TEXCOORD1;
};


#endif