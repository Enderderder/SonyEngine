/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __STDCBUFFER_H__
#define __STDCBUFFER_H__

#include "../../toolkit/shader_common/shader_base.h"

unistruct cbMeshInstance
{
	Matrix4Unaligned g_mLocToProj;
    Matrix4Unaligned g_mLocToView;
	Vector3Unaligned	g_vPad;
	float	g_fTessFactor;
};

unistruct cbExtraInst
{
	float	g_fScale;
	float	g_fOffs;
};

unistruct cbMeshInstBones
{
	Matrix4Unaligned g_mBones[256];
};

#endif
