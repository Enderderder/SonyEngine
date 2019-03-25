/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __STDCBUFFER_H__
#define __STDCBUFFER_H__

#include "../toolkit/shader_common/shader_base.h"

struct RenderTargetInfo
{
	unsigned int m_width;
	unsigned int m_height;
	unsigned int m_sampleIndex;
	unsigned int m_numSamples;
	unsigned int m_numFragments;
	unsigned int m_whatToShow;
};

unistruct Constants
{
	Matrix4Unaligned m_modelView;
	Matrix4Unaligned m_modelViewProjection;
	Vector4Unaligned m_lightPosition;
	Vector4Unaligned m_eyePosition;
	Vector4Unaligned m_lightColor;
	Vector4Unaligned m_ambientColor;
	Vector4Unaligned m_lightAttenuation;
	RenderTargetInfo m_rtInfo;
};

#endif
