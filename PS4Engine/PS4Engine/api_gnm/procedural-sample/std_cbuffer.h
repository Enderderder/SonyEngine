/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __STDCBUFFER_H__
#define __STDCBUFFER_H__

#include "../toolkit/shader_common/shader_base.h"


unistruct cbMeshInstance
{
	Matrix4Unaligned g_mWorldViewProjection;
    Matrix4Unaligned g_mWorld;
	Vector3Unaligned	g_vLightPos;
	float	g_fTime;
	Vector3Unaligned	g_vLightPos2;
	float	g_fPad0;
	Vector3Unaligned	g_vCamPos;
	float	g_fPad1;
};

#endif
