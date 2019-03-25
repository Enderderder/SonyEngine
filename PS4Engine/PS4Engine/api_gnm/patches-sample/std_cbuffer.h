/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __PATCHES_SAMPLE_STDCBUFFER_H__
#define __PATCHES_SAMPLE_STDCBUFFER_H__

#include "../toolkit/shader_common/shader_base.h"

unistruct cbSharedConstants
{
	Vector3Unaligned	g_vLightPos;
	float	g_fMeshScale;
	Vector3Unaligned	g_vLightPos2;
	float	g_fPad2;
};

#endif