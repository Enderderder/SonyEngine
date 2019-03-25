﻿/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef __VECTOR2UNALIGNED_H__
#define __VECTOR2UNALIGNED_H__

#include <vectormath.h>
#include <string.h>

// Vector2Unaligned is meant to store a Vector2, except that padding and alignment are
// 4-byte granular (GPU), rather than 16-byte (SSE). This is to bridge
// the SSE math library with GPU structures that assume 4-byte granularity.
// While a Vector2 has many operations, Vector2Unaligned can only be converted to and from Vector2.
struct Vector2Unaligned
{
	float x;
	float y;
	Vector2Unaligned& operator=(const Vector2 &rhs)
	{
		memcpy(this, &rhs, sizeof(*this));
		return *this;
	}
};

inline Vector2Unaligned ToVector2Unaligned( const Vector2& r )
{
	const Vector2Unaligned result = { r.getX(), r.getY() };
	return result;
}

inline Vector2 ToVector2( const Vector2Unaligned& r )
{
	return Vector2( r.x, r.y );
}

#endif
