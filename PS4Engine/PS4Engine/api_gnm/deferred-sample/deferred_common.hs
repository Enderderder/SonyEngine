/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __DEFERRED_COMMON_H__
#define __DEFERRED_COMMON_H__

struct DEPTH_NORMAL_VS_IN
{
	float4 m_position : S_POSITION;
	float3 m_normal   : TEXCOORD0;
};

struct DEPTH_NORMAL_VS_OUT
{
	float4 m_position            : S_POSITION;
	float4 m_normalSpecularPower : TEXCOORD0;
};

struct DEPTH_NORMAL_PS_OUT
{
	float4 m_normalSpecularPower : S_TARGET_OUTPUT0;
};

ConstantBuffer depth_normal_cb
{
	float4x4 m_localToScreen;
	float4x4 m_localToView;
	float    m_specularPower;
};

struct MAIN_VS_IN
{
	float4 m_position : S_POSITION;
	float2 m_uv : TEXCOORD0;
};

struct MAIN_VS_OUT
{
	float4 m_position : S_POSITION;
	float2 m_uv : TEXCOORD0;
};

struct MAIN_PS_OUT
{
	float4 m_color : S_TARGET_OUTPUT0;
};

#endif
