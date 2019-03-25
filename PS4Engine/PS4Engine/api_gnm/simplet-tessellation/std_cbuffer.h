/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __STDCBUFFER_H__
#define __STDCBUFFER_H__

#include "../toolkit/shader_common/shader_base.h"
#define NUMSEGS_X (7)
#define NUMSEGS_Y (3)
#define NUM_INPUT_CTRL_POINTS_PER_PATCH (4)
unistruct cbMeshInstance
{
	Matrix4Unaligned g_mLocToProj;
    Matrix4Unaligned g_mLocToView;
};


#endif
