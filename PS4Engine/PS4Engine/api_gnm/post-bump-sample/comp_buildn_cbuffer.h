/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __COMPBUILDCBUFFER_H__
#define __COMPBUILDCBUFFER_H__

#define TILEX	16
#define TILEY	16

#define TILEX_NOIDIV	14
#define TILEY_NOIDIV	14

#define NR_PIXELS	(TILEX*TILEY)


#include "../toolkit/shader_common/shader_base.h"


unistruct cbCompBuildNorm
{
	Matrix4Unaligned g_mInvProj;
};


#endif
