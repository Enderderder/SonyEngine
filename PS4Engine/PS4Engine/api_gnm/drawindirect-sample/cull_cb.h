/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __CULLCBUFFER_H__
#define __CULLCBUFFER_H__

#include "../toolkit/shader_common/shader_base.h"

unistruct FastCullConstants
{
	Vector4Unaligned m_eye[2];
	Vector4Unaligned m_frustumPlane[5];
	Vector4Unaligned m_occluderPlane[4][5];
	uint m_triangles;
	uint m_occluders;
};

#endif
