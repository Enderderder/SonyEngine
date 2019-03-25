/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 02.508.051
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __HISTOGRAM_STRUCTS_H__ 
#define __HISTOGRAM_STRUCTS_H__

#include "../toolkit/shader_common/shader_base.h"

enum { kNumHistogramColors = 256 };

struct HistogramConstants
{
	Matrix4Unaligned g_mWorldViewProjection;
	Vector4Unaligned g_vHistogramColor;
	uint g_histogramColorChannel;
	uint g_dataInGds;
	float g_histogramBarVerticalScale;
};

struct HistogramColorData
{
	uint pixelCount[256][3];
};


#ifdef __PSSL__
struct VS_HISTOGRAM_OUTPUT
{
	float4 Position : S_POSITION;
	float4 Color : TEXCOORD0;
};
#endif

#endif
