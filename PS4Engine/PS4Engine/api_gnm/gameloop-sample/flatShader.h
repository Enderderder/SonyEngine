/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __FLATSHADER_H__
#define __FLATSHADER_H__

#include "../toolkit/shader_common/shader_base.h"

unistruct FlatShaderConstants
{
	Matrix4Unaligned m_modelViewProjection;
	Vector4Unaligned m_color;
};

#endif
