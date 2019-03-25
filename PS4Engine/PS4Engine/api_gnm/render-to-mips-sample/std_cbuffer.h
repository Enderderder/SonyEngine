/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __STDCBUFFER_H__
#define __STDCBUFFER_H__

#include "../toolkit/shader_common/shader_base.h"

unistruct cbMipShaderConstants
{
	Vector4Unaligned m_tintColor;
	int m_mipLevel;
};

unistruct cbMeshShaderConstants
{
	Matrix4Unaligned m_modelView;
	Matrix4Unaligned m_modelViewProjection;
	Vector3Unaligned m_lightPosition;
	float m_fPad0;
};

#endif
