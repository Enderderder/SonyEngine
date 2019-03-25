/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __STDSRTSTRUCTS_H__
#define __STDSRTSTRUCTS_H__

#include "../toolkit/shader_common/shader_base.h"

#ifndef __PSSL__
	#include <gnm.h>
	typedef sce::Gnm::Texture Texture2D;
	typedef sce::Gnm::Sampler SamplerState;
#endif

struct PSTexSamplerSrtData
{
	Texture2D texture;
	SamplerState sampler;
};

struct PSTexSamplerSrt
{
	PSTexSamplerSrtData *srtData;
};

struct PS2xTex2xSamplerSrtData
{
	Texture2D txDiffuse;
	SamplerState samLinear;
	Texture2D txShadowMap;
	SamplerState samShadow;
};

struct LightBuffer
{
	// Lighting
	// Direction vector of light ray at view spce.
	// (only a parallel light.)
	Vector4Unaligned lightPosition;
	Vector4Unaligned lightColor;
	Vector4Unaligned ambientColor;
};

struct PS2xTex2xSamplerSrt
{
	PS2xTex2xSamplerSrtData *srtData;
#ifdef __PSSL__
	RegularBuffer<LightBuffer> constants;
#else
	sce::Gnm::Buffer constants;
#endif
};

struct Matrices
{
	Matrix4Unaligned world;
	Matrix4Unaligned view;
	Matrix4Unaligned projection;
	Matrix4Unaligned shadow;
};

struct VSSrt
{
#ifdef __PSSL__
	RegularBuffer<Matrices> constants;
#else
	sce::Gnm::Buffer constants;
#endif
};


#endif
