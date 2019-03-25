/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_TOOLKIT_HALF_H
#define _SCE_GNM_TOOLKIT_HALF_H

#include "toolkit.h"

namespace sce { namespace Gnmx { namespace Toolkit {

#if defined(_MSC_VER)

// Smallest positive half
#define HALF_MIN				(5.96046448e-08f)

// Smallest positive normalized half
#define HALF_NRM_MIN			(6.10351562e-05f)

// Largest positive half
#define HALF_MAX				(65504.0f)

// Smallest positive e for which half (1.0 + e) != half (1.0)
#define HALF_EPSILON			(0.00097656f)


#else // defined(_MSC_VER)

// Smallest positive half
#define HALF_MIN				(5.96046448e-08)

// Smallest positive normalized half
#define HALF_NRM_MIN			(6.10351562e-05)

// Largest positive half
#define HALF_MAX				(65504.0)

// Smallest positive e for which half (1.0 + e) != half (1.0)
#define HALF_EPSILON			(0.00097656)

#endif // defined(_MSC_VER)


// Number of digits in the mantissa (signiticand + hidden leading 1)
#define HALF_MANT_DIG			(11)

// Number of base 10 digits that can be represented without change
#define HALF_DIG				(2)

// Base of the exponent
#define HALF_RADIX				(2)

// Minimum negative integer such that HALF_RADIX raised to the power of one
// less than that integer is a normalized half
#define HALF_MIN_EXP			(-13)

// Maximum positive integer such that HALF_RADIX raised to the power of one
// less than that integer is a normalized half
#define HALF_MAX_EXP			(16)

// Minimum positive integer such that 10 raised to that power is a
// normalized half
#define HALF_MIN_10_EXP			(-4)

// Maximum positive integer such that 10 raised to that power is a
// normalized half
#define HALF_MAX_10_EXP			(4)

class Half 
{
public:
	union uif
	{
		uint32_t i;
		float f;
	};

	union H16
	{
		class
		{
		public:
			// Mantissa
			int16_t m:10;
			// Exponent
			int16_t e:5;
			// Sign
			int16_t s:1;
		} h;
		int16_t i;
	};

	union _F32
	{
		class
		{
		public:
			// Mantissa
			int32_t m:23;
			// Exponent
			int32_t e:8;
			// Sign
			int32_t s:1;
		} f;
		int32_t i;
	};

	Half() {}
	Half(uint16_t h);
	Half(float f);

	operator float() const;
	operator uint16_t() const { return m_data; }

	Half operator-() const;

	Half &operator=(const Half &h);
	Half &operator=(float f);
	Half &operator+=(const Half &h);
	Half &operator+=(float f);
	Half &operator-=(const Half &h);
	Half &operator-=(float f);
	Half &operator*=(const Half &h);
	Half &operator*=(float f);
	Half &operator/=(const Half &h);
	Half &operator/=(float f);

	// Returns true if this is a normalized number, a denormalized number, or
	// zero
	bool IsFinite() const;
	// Returns true if this is a normalized number
	bool IsNormalized() const;
	// Returns true if this is a denormalized number
	bool IsDenormalized() const;
	// Returns true if this is zero
	bool IsZero() const;
	// Returns true if this is Not A Number (NAN)
	bool IsNAN() const;
	// Returns true if this is a positive or negative infinity
	bool IsInf() const;
	// Returns true if this is a negative number
	bool IsNegative() const;

	// Returns a half whos value is +infinity
	static Half PosInf();
	// Returns a half whos value is -infinity
	static Half NegInf();
	// Returns a half whos bit pattern is 0111111111111111
	static Half QNAN();
	// Returns a half whos bit pattern is 0111110111111111
	static Half SNAN();

private:
	uint16_t m_data;

	static const uint16_t s_arrExpLUT[1<<9];

	static int16_t Convert(int32_t i);
	static int32_t Convert(int16_t i);
	static float Overflow();
};

inline Half::Half(uint16_t h)
: m_data(h)
{
}

inline Half::Half(float f)
{
    uif x;
    x.f = f;
    if (f == 0)
	{
		// Note: no sign preservation for quick 0 case
        m_data = (x.i >> 16);
	}
	else
	{
		// We extract the combined sign and exponent, e, from our
		// floating-point number, f.  Then we convert e to the sign
		// and exponent of the half number via a table lookup.
		//
		// For the most common case, where a normalized half is produced,
		// the table lookup returns a non-zero value; in this case, all
		// we have to do, is round f's mantissa to 10 bits and combine
		// the result with e.
		//
		// For all other cases (overflow, zeroes, denormalized numbers
		// resulting from underflow, infinities and NANs), the table
		// lookup returns zero, and we call a longer, non-inline function
		// to do the float-to-half conversion.


		int32_t e = (x.i >> 23) & 0x000001ff;
		e = s_arrExpLUT[e];

		if (e)
		{
			// Simple case - round the mantissa and
			// combine it with the sign and exponent.
			m_data = (uint16_t)(e + (((x.i & 0x007fffff) + 0x00001000) >> 13));
		}
		else
		{
			// Difficult case - call a function.
			m_data = Convert( int32_t(x.i) );
		}

	}
}

inline Half::operator float() const
{
    uif u;
    u.i = Convert(static_cast<int16_t>(m_data));
    return u.f;
}

inline Half Half::operator -() const
{
	// Flip the sign bit
	Half h;
	h.m_data = m_data ^ 0x8000;
	return h;
}

inline Half &Half::operator =(const Half &h)
{
	m_data = h.m_data;
	return *this;
}

inline Half &Half::operator =(float f)
{
	*this = Half(f);
	return *this;
}

inline Half &Half::operator +=(const Half &h)
{
	*this = Half(float(*this) + float(h));
	return *this;
}

inline Half &Half::operator +=(float f)
{
	*this = Half(float(*this) + f);
	return *this;
}

inline Half &Half::operator -=(const Half &h)
{
	*this = Half(float(*this) - float(h));
	return *this;
}

inline Half &Half::operator -=(float f)
{
	*this = Half(float(*this) - f);
	return *this;
}

inline Half &Half::operator *=(const Half &h)
{
	*this = Half(float(*this) * float(h));
	return *this;
}

inline Half &Half::operator *=(float f)
{
	*this = Half(float(*this) * f);
	return *this;
}

inline Half &Half::operator /=(const Half &h)
{
	*this = Half(float(*this) / float(h));
	return *this;
}

inline Half &Half::operator /=(float f)
{
	*this = Half(float(*this) / f);
	return *this;
}

inline bool	Half::IsFinite() const
{
	uint16_t e = (m_data >> 10) & 0x001f;
	return e < 31;
}

inline bool Half::IsNormalized() const
{
	uint16_t e = (m_data >> 10) & 0x001f;
	return e > 0 && e < 31;
}

inline bool Half::IsDenormalized() const
{
	uint16_t e = (m_data >> 10) & 0x001f;
	uint16_t m =  m_data & 0x3ff;
	return e == 0 && m != 0;
}

inline bool Half::IsZero() const
{
	return (m_data & 0x7fff) == 0;
}

inline bool Half::IsNAN() const
{
	uint16_t e = (m_data >> 10) & 0x001f;
	uint16_t m =  m_data & 0x3ff;
	return e == 31 && m != 0;
}

inline bool Half::IsInf() const
{
	uint16_t e = (m_data >> 10) & 0x001f;
	uint16_t m =  m_data & 0x3ff;
	return e == 31 && m == 0;
}

inline bool	Half::IsNegative() const
{
	return (m_data & 0x8000) != 0;
}

inline Half Half::PosInf()
{
	Half h;
	h.m_data = 0x7c00;
	return h;
}

inline Half Half::NegInf()
{
	Half h;
	h.m_data = 0xfc00;
	return h;
}

inline Half Half::QNAN()
{
	Half h;
	h.m_data = 0x7fff;
	return h;
}

inline Half Half::SNAN()
{
	Half h;
	h.m_data = 0x7dff;
	return h;
}

} } } // namespace sce { namespace Gnmx { namespace Toolkit {

#endif // _SCE_GNM_TOOLKIT_HALF_H

