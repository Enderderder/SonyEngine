﻿/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __VECTOR4UNALIGNED_H__
#define __VECTOR4UNALIGNED_H__

#include <vectormath.h>
#include <string.h>

// Vector4Unaligned is meant to store a Vector4, except that padding and alignment are
// 4-byte granular (GPU), rather than 16-byte (SSE). This is to bridge
// the SSE math library with GPU structures that assume 4-byte granularity.
// While a Vector4 has many operations, Vector4Unaligned can only be converted to and from Vector4.
struct Vector4Unaligned
{
	float x;
	float y;
	float z;
	float w;
	Vector4Unaligned &operator=(const Vector4 &rhs)
	{
		memcpy(this, &rhs, sizeof(*this));
		return *this;
	}
};

inline Vector4Unaligned ToVector4Unaligned( const Vector4& r )
{
	const Vector4Unaligned result = { r.getX(), r.getY(), r.getZ(), r.getW() };
	return result;
}

inline Vector3Unaligned ToVector3Unaligned( const Vector4& r )
{
	const Vector3Unaligned result = { r.getX(), r.getY(), r.getZ() };
	return result;
}

inline Vector4 ToVector4( const Vector4Unaligned& r )
{
	return Vector4( r.x, r.y, r.z, r.w );
}

#endif
