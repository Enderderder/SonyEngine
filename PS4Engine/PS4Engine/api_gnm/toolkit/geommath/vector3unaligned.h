﻿/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __VECTOR3UNALIGNED_H__
#define __VECTOR3UNALIGNED_H__

#include <vectormath.h>
#include <string.h>

// Vector3Unaligned is meant to store a Vector3, except that padding and alignment are
// 4-byte granular (GPU), rather than 16-byte (SSE). This is to bridge
// the SSE math library with GPU structures that assume 4-byte granularity.
// While a Vector3 has many operations, Vector3Unaligned can only be converted to and from Vector3.
struct Vector3Unaligned
{
	float x;
	float y;
	float z;
	Vector3Unaligned& operator=(const Vector3 &rhs)
	{
		memcpy(this, &rhs, sizeof(*this));
		return *this;
	}
	Vector3Unaligned& operator=(const Vector4 &rhs)
	{
		memcpy(this, &rhs, sizeof(*this));
		return *this;
	}
};

inline Vector3Unaligned ToVector3Unaligned( const Vector3& r )
{
	const Vector3Unaligned result = { r.getX(), r.getY(), r.getZ() };
	return result;
}

inline Vector3 ToVector3( const Vector3Unaligned& r )
{
	return Vector3( r.x, r.y, r.z );
}

#endif
