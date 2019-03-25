/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __VSOUTPUT_H__
#define __VSOUTPUT_H__

struct ESGS_ENTRY
{
	uint uVertexID : PASSINDEX;
};

struct GSVS_ENTRY
{
	float4 vPosition			: S_POSITION;
	float3 color				: TEXCOORD0;
};


#endif