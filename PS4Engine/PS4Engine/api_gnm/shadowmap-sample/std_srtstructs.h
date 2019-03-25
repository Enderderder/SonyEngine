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
	typedef sce::Gnm::Sampler SamplerComparisonState;
#endif

struct SamplersInSRT
{
	SamplerState samp0;
	SamplerState samp1;
	SamplerComparisonState samp2;
};

struct TexturesInSRT
{
	Texture2D colorMap;
	Texture2D bumpGlossMap;
	Texture2D shadowMap;
};

struct ConstantsInSRT
{
	Matrix4Unaligned m_modelViewProjection;
	Matrix4Unaligned m_shadow_mvp;
	Matrix4Unaligned m_modelView;
	Vector4Unaligned m_lightPosition;
	Vector4Unaligned m_lightColor; // 1.3*float3(0.5,0.58,0.8);
	Vector4Unaligned m_ambientColor; // 0.3*float3(0.18,0.2,0.3);
	Vector4Unaligned m_lightAttenuation; // float4(1,0,0,0);
};

struct SRT
{
	SamplersInSRT *samplers;
	TexturesInSRT *textures;
	ConstantsInSRT *constants;
};

#endif
