/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <algorithm>
#include <gnmx.h>
#include "floating_point.h"
#include "half.h"
using namespace sce;

namespace
{
	union f32
	{
		enum {kBias = 127};
		struct 
		{
			uint32_t m_mantissa:23;
			uint32_t m_exponent:8;
			uint32_t m_sign:1;
		} bits;
		uint32_t u;
		float f;
	};

	uint32_t packBits(uint32_t value, uint32_t offset, uint32_t count, uint32_t field)
	{
		const uint32_t mask = ((1 << count) - 1) << offset;
		return (value & ~mask) | ((field << offset) & mask);
	}

	uint32_t unpackBits(uint32_t value, uint32_t offset, uint32_t count)
	{
		return (value >> offset) & ((1<<count)-1);
	}

/*
	uint32_t expandIntegerFraction(uint32_t value, uint32_t oldBits, uint32_t newBits)
	{
		const uint32_t shift = (newBits - oldBits);
		uint32_t result = value << shift;
		result |= result >> (oldBits* 1);
		result |= result >> (oldBits* 2);
		result |= result >> (oldBits* 4);
		result |= result >> (oldBits* 8);
		result |= result >> (oldBits*16);
		return result;
	}
*/
}

uint32_t Gnmx::Toolkit::packFloat(float value, uint32_t signBits, uint32_t exponentBits, uint32_t mantissaBits)
{
    if(exponentBits == 5)
    {
        if(signBits == 1 && mantissaBits == 10)
            return floatToFloat16(value);
        if(signBits == 0 && mantissaBits == 6)
            return floatToFloat11(value);
        if(signBits == 0 && mantissaBits == 5)
            return floatToFloat10(value);
    }
	if(signBits == 0)
		value = std::max(0.f, value);
	f32 in;
	in.f = value;
	const int32_t maxExponent = (1 << exponentBits) - 1;
	const uint32_t bias        = maxExponent >> 1;
	const uint32_t sign        = in.bits.m_sign;
	uint32_t mantissa          = in.bits.m_mantissa >> (23 - mantissaBits);
	int32_t exponent;
	switch(in.bits.m_exponent)
	{
	case 0x00:
		exponent = 0;
		break;
	case 0xFF:
		exponent = maxExponent;
		break;
	default:
		exponent = in.bits.m_exponent - 127 + bias;
		if(exponent < 1 )
		{
			exponent = 1;
			mantissa = 0;
		}
		if(exponent > maxExponent - 1)
		{
			exponent = maxExponent - 1;
			mantissa = (1 << 23) - 1;
		}
	}
	uint32_t result = 0;
	result = packBits(result, 0                          , mantissaBits, mantissa);
	result = packBits(result, mantissaBits               , exponentBits, exponent);
	result = packBits(result, mantissaBits + exponentBits, signBits,     sign    );
	return result;
}

float Gnmx::Toolkit::unpackFloat(uint32_t value, uint32_t signBits, uint32_t exponentBits, uint32_t mantissaBits)
{
    if(exponentBits == 5)
    {
        if(signBits == 1 && mantissaBits == 10)
            return float16ToFloat(value);
        if(signBits == 0 && mantissaBits == 6)
            return float11ToFloat(value);
        if(signBits == 0 && mantissaBits == 5)
            return float10ToFloat(value);
    }
	f32 out;
	const uint32_t maxExponent = (1 << exponentBits) - 1;
	const uint32_t bias        = maxExponent >> 1;
	const uint32_t mantissa    = unpackBits(value, 0                          , mantissaBits);
	const uint32_t exponent    = unpackBits(value, mantissaBits               , exponentBits);
	const uint32_t sign        = unpackBits(value, mantissaBits + exponentBits, signBits    );
	out.bits.m_mantissa = mantissa << (23 - mantissaBits);
	out.bits.m_exponent = (exponent == 0) ? 0 : (exponent == maxExponent) ? 0xFF : exponent - bias + 127;
	out.bits.m_sign     = sign;
	return out.f;
}

int32_t Gnmx::Toolkit::convertFloatToInt(float value)
{
	return static_cast<int32_t>(floorf(value + 0.5f));
}

uint32_t Gnmx::Toolkit::convertFloatToUint(float value)
{
	return static_cast<uint32_t>(floorf(value + 0.5f));
}

uint32_t Gnmx::Toolkit::floatToFloat10(float value)
{
    if(value < 0.f)
        value = 0.f;
    uint32_t half = floatToFloat16(value);
    return half >> 5;
}

uint32_t Gnmx::Toolkit::floatToFloat11(float value)
{
    if(value < 0.f)
        value = 0.f;
    uint32_t half = floatToFloat16(value);
    return half >> 4;
}

uint32_t Gnmx::Toolkit::floatToFloat16(float value)
{    
    Half half(value);
    return (uint16_t)half;
}

uint32_t Gnmx::Toolkit::floatToFloat32(float value)
{
    f32 f;
    f.f = value;
    return f.u;
}

 float Gnmx::Toolkit::float10ToFloat(uint32_t value)
 {
    f32 fromHalf;
    if(value >= 993)
      fromHalf.u = 4290772992;
    else
    {
        Half half(static_cast<uint16_t>(value << 5));
        fromHalf.f = (float)half;
    }
    return fromHalf.f;
 }
 
 float Gnmx::Toolkit::float11ToFloat(uint32_t value)
 {
    f32 fromHalf;
    if(value >= 1985)
      fromHalf.u = 4290772992;
    else
    {
        Half half(static_cast<uint16_t>(value << 4));
        fromHalf.f = (float)half;
    }
    return fromHalf.f;
 }

float Gnmx::Toolkit::float16ToFloat(uint32_t value)
{
    f32 fromHalf;
    Half half(static_cast<uint16_t>(value));
    fromHalf.f = (float)half;
    return fromHalf.f;
}

float Gnmx::Toolkit::float32ToFloat(uint32_t value)
{
    f32 f;
    f.u = value;
    return f.f;
}

