/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __STDCBUFFER_H__
#define __STDCBUFFER_H__

#include "../toolkit/shader_common/shader_base.h"

unistruct Constants
{
	Matrix4Unaligned m_modelView;
	Matrix4Unaligned m_modelViewProjection;
	Matrix4Unaligned m_uv;
	Vector4Unaligned m_lightPosition;
	Vector4Unaligned m_lightColor;
	Vector4Unaligned m_ambientColor;
	Vector4Unaligned m_lightAttenuation;
};

#endif
