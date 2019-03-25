/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __HTILE_H__
#define __HTILE_H__

#include "../toolkit/shader_common/shader_base.h"
#include "shared_symbols.h"

uint bitmask( uint count )
{
	return (1<<count)-1;
}

int extractsigned( uint bits, uint offset, uint count )
{
	return (bits>>offset) & bitmask(count);
}

uint extractunsigned( uint bits, uint offset, uint count )
{
	return (bits>>offset) & bitmask(count);
}

float extractfloat( uint bits, uint offset, uint count )
{
	return extractunsigned(bits,offset,count) / (float)bitmask(count);
}

struct HTileVertexInput
{
	float4 position : S_POSITION;
};

struct HTileVertexOutput
{
	float4 position : S_POSITION;
	float2 tile     : TEXCOORD0;
};

struct HTileFragmentOutput
{
	float4 color_edge   : S_TARGET_OUTPUT0;
};

uint PadTo( uint value, uint pitch )
{
	return ( value + pitch-1 ) / pitch * pitch;
}

uint getbits( uint bits, uint shift, uint a )
{
	return ((bits>>a)&1)<<shift;
}

uint notbits( uint bits, uint shift, uint a )
{
	return (((bits>>a)&1)^1)<<shift;
}

uint xorbits( uint bits, uint shift, uint a, uint b )
{
	return (((bits>>a)^(bits>>b))&1)<<shift;
}

uint xorbit3( uint bits, uint shift, uint a, uint b, uint c )
{
	return (((bits>>a)^(bits>>b)^(bits>>c))&1)<<shift;
}

uint getbit( uint word, uint bit )
{
	return (word>>bit)&1;
}

struct Encoder
{
	uint base;
	uint shift;
	uint mask;
};	

ConstantBuffer HTilePixelShaderConstantBuffer : register(c0)
{
	uint4 ps_tiles;
	uint4 ps_htileField;
	uint4 ps_format;
	uint4 ps_layout;
	uint4 ps_offset;
}

#define unsigned uint
#include "metadata.h"
#undef unsigned

#endif

