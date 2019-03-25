﻿/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __SHADERBASE_H__
#define __SHADERBASE_H__


#ifndef __cplusplus

#ifdef __PSSL__
#pragma warning (disable: 5203 5206 8207 5559)
#endif

#define Vector2Unaligned		float2
#define Vector3Unaligned		float3
#define Vector4Unaligned		float4

uint FIRSTBITLOW_SLOW( uint input )
{
	for( int bit=0; bit<32; ++bit )
		if( input & (1<<bit) )
			return bit;
	return 32;
}

uint FIRSTBITHIGH_SLOW( uint input )
{
	for( int bit=31; bit>=0; --bit )
		if( input & (1<<bit) )
			return bit;
	return 32;
}

#define __CBREGISTER( i )		 : register( b ## i )

#ifdef __PSSL__
	#define Matrix4Unaligned column_major matrix
	#define unistruct	     ConstantBuffer
#else
	#define Matrix4Unaligned matrix
	#define unistruct	     cbuffer
#endif 

#else
#include <stddef.h>
#include "../geommath/geommath.h"


#define __CBREGISTER( i )
#define ATTR_OFFS			offsetof

#define unistruct	struct
#endif


#endif
