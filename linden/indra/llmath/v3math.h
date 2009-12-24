/** 
 * @file v3math.h
 * @brief LLVector3 class header file.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_V3MATH_H
#define LL_V3MATH_H

#include "llerror.h"
#include "llmath.h"

#include "llsd.h"
class LLVector2;
class LLVector4;
class LLMatrix3;
class LLVector3d;
class LLQuaternion;

//  LLvector3 = |x y z w|

static const U32 LENGTHOFVECTOR3 = 3;

class LLVector3
{
	public:
		F32 mV[LENGTHOFVECTOR3];

		static const LLVector3 zero;
		static const LLVector3 x_axis;
		static const LLVector3 y_axis;
		static const LLVector3 z_axis;
		static const LLVector3 x_axis_neg;
		static const LLVector3 y_axis_neg;
		static const LLVector3 z_axis_neg;
		static const LLVector3 all_one;

		inline LLVector3();							// Initializes LLVector3 to (0, 0, 0)
		inline LLVector3(const F32 x, const F32 y, const F32 z);			// Initializes LLVector3 to (x. y, z)
		inline explicit LLVector3(const F32 *vec);				// Initializes LLVector3 to (vec[0]. vec[1], vec[2])
		explicit LLVector3(const LLVector2 &vec);				// Initializes LLVector3 to (vec[0]. vec[1], 0)
		explicit LLVector3(const LLVector3d &vec);				// Initializes LLVector3 to (vec[0]. vec[1], vec[2])
		explicit LLVector3(const LLVector4 &vec);				// Initializes LLVector4 to (vec[0]. vec[1], vec[2])
		LLVector3(const LLSD& sd);

		LLSD getValue() const;

		void setValue(const LLSD& sd);

		const LLVector3& operator=(const LLSD& sd);

		inline BOOL isFinite() const;									// checks to see if all values of LLVector3 are finite
		BOOL		clamp(F32 min, F32 max);		// Clamps all values to (min,max), returns TRUE if data changed
		BOOL		clampLength( F32 length_limit );					// Scales vector to limit length to a value

		void		quantize16(F32 lowerxy, F32 upperxy, F32 lowerz, F32 upperz);	// changes the vector to reflect quatization
		void		quantize8(F32 lowerxy, F32 upperxy, F32 lowerz, F32 upperz);	// changes the vector to reflect quatization
		void 		snap(S32 sig_digits);											// snaps x,y,z to sig_digits decimal places

		BOOL		abs();						// sets all values to absolute value of original value (first octant), returns TRUE if changed
		
		inline void	clear();						// Clears LLVector3 to (0, 0, 0)
		inline void	setZero();						// Clears LLVector3 to (0, 0, 0)
		inline void	clearVec();						// deprecated
		inline void	zeroVec();						// deprecated

		inline void	set(F32 x, F32 y, F32 z);		// Sets LLVector3 to (x, y, z, 1)
		inline void	set(const LLVector3 &vec);		// Sets LLVector3 to vec
		inline void	set(const F32 *vec);			// Sets LLVector3 to vec
		const LLVector3& set(const LLVector4 &vec);
		const LLVector3& set(const LLVector3d &vec);// Sets LLVector3 to vec

		inline void	setVec(F32 x, F32 y, F32 z);	// deprecated
		inline void	setVec(const LLVector3 &vec);	// deprecated
		inline void	setVec(const F32 *vec);			// deprecated

		const LLVector3& setVec(const LLVector4 &vec);  // deprecated
		const LLVector3& setVec(const LLVector3d &vec);	// deprecated

		F32		length() const;			// Returns magnitude of LLVector3
		F32		lengthSquared() const;	// Returns magnitude squared of LLVector3
		F32		magVec() const;			// deprecated
		F32		magVecSquared() const;	// deprecated

		inline F32		normalize();	// Normalizes and returns the magnitude of LLVector3
		inline F32		normVec();		// deprecated

		inline BOOL inRange( F32 min, F32 max ) const; // Returns true if all values of the vector are between min and max

		const LLVector3&	rotVec(F32 angle, const LLVector3 &vec);	// Rotates about vec by angle radians
		const LLVector3&	rotVec(F32 angle, F32 x, F32 y, F32 z);		// Rotates about x,y,z by angle radians
		const LLVector3&	rotVec(const LLMatrix3 &mat);				// Rotates by LLMatrix4 mat
		const LLVector3&	rotVec(const LLQuaternion &q);				// Rotates by LLQuaternion q

		const LLVector3&	scaleVec(const LLVector3& vec);				// scales per component by vec
		LLVector3			scaledVec(const LLVector3& vec) const;			// get a copy of this vector scaled by vec

		BOOL isNull() const;			// Returns TRUE if vector has a _very_small_ length
		BOOL isExactlyZero() const		{ return !mV[VX] && !mV[VY] && !mV[VZ]; }

		F32 operator[](int idx) const { return mV[idx]; }
		F32 &operator[](int idx) { return mV[idx]; }
	
		friend LLVector3 operator+(const LLVector3 &a, const LLVector3 &b);	// Return vector a + b
		friend LLVector3 operator-(const LLVector3 &a, const LLVector3 &b);	// Return vector a minus b
		friend F32 operator*(const LLVector3 &a, const LLVector3 &b);		// Return a dot b
		friend LLVector3 operator%(const LLVector3 &a, const LLVector3 &b);	// Return a cross b
		friend LLVector3 operator*(const LLVector3 &a, F32 k);				// Return a times scaler k
		friend LLVector3 operator/(const LLVector3 &a, F32 k);				// Return a divided by scaler k
		friend LLVector3 operator*(F32 k, const LLVector3 &a);				// Return a times scaler k
		friend bool operator==(const LLVector3 &a, const LLVector3 &b);		// Return a == b
		friend bool operator!=(const LLVector3 &a, const LLVector3 &b);		// Return a != b
		// less than operator useful for using vectors as std::map keys
		friend bool operator<(const LLVector3 &a, const LLVector3 &b);		// Return a < b

		friend const LLVector3& operator+=(LLVector3 &a, const LLVector3 &b);	// Return vector a + b
		friend const LLVector3& operator-=(LLVector3 &a, const LLVector3 &b);	// Return vector a minus b
		friend const LLVector3& operator%=(LLVector3 &a, const LLVector3 &b);	// Return a cross b
		friend const LLVector3& operator*=(LLVector3 &a, const LLVector3 &b);	// Returns a * b;
		friend const LLVector3& operator*=(LLVector3 &a, F32 k);				// Return a times scaler k
		friend const LLVector3& operator/=(LLVector3 &a, F32 k);				// Return a divided by scaler k
		friend const LLVector3& operator*=(LLVector3 &a, const LLQuaternion &b);	// Returns a * b;

		friend LLVector3 operator-(const LLVector3 &a);					// Return vector -a

		friend std::ostream&	 operator<<(std::ostream& s, const LLVector3 &a);		// Stream a

		static BOOL parseVector3(const std::string& buf, LLVector3* value);
};

typedef LLVector3 LLSimLocalVec;

// Non-member functions 

F32	angle_between(const LLVector3 &a, const LLVector3 &b);	// Returns angle (radians) between a and b
BOOL are_parallel(const LLVector3 &a, const LLVector3 &b, F32 epsilon=F_APPROXIMATELY_ZERO);	// Returns TRUE if a and b are very close to parallel
F32	dist_vec(const LLVector3 &a, const LLVector3 &b);		// Returns distance between a and b
F32	dist_vec_squared(const LLVector3 &a, const LLVector3 &b);// Returns distance sqaured between a and b
F32	dist_vec_squared2D(const LLVector3 &a, const LLVector3 &b);// Returns distance sqaured between a and b ignoring Z component
LLVector3 projected_vec(const LLVector3 &a, const LLVector3 &b); // Returns vector a projected on vector b
LLVector3 lerp(const LLVector3 &a, const LLVector3 &b, F32 u); // Returns a vector that is a linear interpolation between a and b

inline LLVector3::LLVector3(void)
{
	mV[0] = 0.f;
	mV[1] = 0.f;
	mV[2] = 0.f;
}

inline LLVector3::LLVector3(const F32 x, const F32 y, const F32 z)
{
	mV[VX] = x;
	mV[VY] = y;
	mV[VZ] = z;
}

inline LLVector3::LLVector3(const F32 *vec)
{
	mV[VX] = vec[VX];
	mV[VY] = vec[VY];
	mV[VZ] = vec[VZ];
}

/*
inline LLVector3::LLVector3(const LLVector3 &copy)
{
	mV[VX] = copy.mV[VX];
	mV[VY] = copy.mV[VY];
	mV[VZ] = copy.mV[VZ];
}
*/

// Destructors

// checker
inline BOOL LLVector3::isFinite() const
{
	return (llfinite(mV[VX]) && llfinite(mV[VY]) && llfinite(mV[VZ]));
}


// Clear and Assignment Functions

inline void	LLVector3::clear(void)
{
	mV[0] = 0.f;
	mV[1] = 0.f;
	mV[2] = 0.f;
}

inline void	LLVector3::setZero(void)
{
	mV[0] = 0.f;
	mV[1] = 0.f;
	mV[2] = 0.f;
}

inline void	LLVector3::clearVec(void)
{
	mV[0] = 0.f;
	mV[1] = 0.f;
	mV[2] = 0.f;
}

inline void	LLVector3::zeroVec(void)
{
	mV[0] = 0.f;
	mV[1] = 0.f;
	mV[2] = 0.f;
}

inline void	LLVector3::set(F32 x, F32 y, F32 z)
{
	mV[VX] = x;
	mV[VY] = y;
	mV[VZ] = z;
}

inline void	LLVector3::set(const LLVector3 &vec)
{
	mV[0] = vec.mV[0];
	mV[1] = vec.mV[1];
	mV[2] = vec.mV[2];
}

inline void	LLVector3::set(const F32 *vec)
{
	mV[0] = vec[0];
	mV[1] = vec[1];
	mV[2] = vec[2];
}

// deprecated
inline void	LLVector3::setVec(F32 x, F32 y, F32 z)
{
	mV[VX] = x;
	mV[VY] = y;
	mV[VZ] = z;
}

// deprecated
inline void	LLVector3::setVec(const LLVector3 &vec)
{
	mV[0] = vec.mV[0];
	mV[1] = vec.mV[1];
	mV[2] = vec.mV[2];
}

// deprecated
inline void	LLVector3::setVec(const F32 *vec)
{
	mV[0] = vec[0];
	mV[1] = vec[1];
	mV[2] = vec[2];
}

inline F32 LLVector3::normalize(void)
{
	F32 mag = fsqrtf(mV[0]*mV[0] + mV[1]*mV[1] + mV[2]*mV[2]);
	F32 oomag;

	if (mag > FP_MAG_THRESHOLD)
	{
		oomag = 1.f/mag;
		mV[0] *= oomag;
		mV[1] *= oomag;
		mV[2] *= oomag;
	}
	else
	{
		mV[0] = 0.f;
		mV[1] = 0.f;
		mV[2] = 0.f;
		mag = 0;
	}
	return (mag);
}

// deprecated
inline F32 LLVector3::normVec(void)
{
	F32 mag = fsqrtf(mV[0]*mV[0] + mV[1]*mV[1] + mV[2]*mV[2]);
	F32 oomag;

	if (mag > FP_MAG_THRESHOLD)
	{
		oomag = 1.f/mag;
		mV[0] *= oomag;
		mV[1] *= oomag;
		mV[2] *= oomag;
	}
	else
	{
		mV[0] = 0.f;
		mV[1] = 0.f;
		mV[2] = 0.f;
		mag = 0;
	}
	return (mag);
}

// LLVector3 Magnitude and Normalization Functions

inline F32	LLVector3::length(void) const
{
	return fsqrtf(mV[0]*mV[0] + mV[1]*mV[1] + mV[2]*mV[2]);
}

inline F32	LLVector3::lengthSquared(void) const
{
	return mV[0]*mV[0] + mV[1]*mV[1] + mV[2]*mV[2];
}

inline F32	LLVector3::magVec(void) const
{
	return fsqrtf(mV[0]*mV[0] + mV[1]*mV[1] + mV[2]*mV[2]);
}

inline F32	LLVector3::magVecSquared(void) const
{
	return mV[0]*mV[0] + mV[1]*mV[1] + mV[2]*mV[2];
}

inline BOOL LLVector3::inRange( F32 min, F32 max ) const
{
	return mV[0] >= min && mV[0] <= max &&
		   mV[1] >= min && mV[1] <= max &&
		   mV[2] >= min && mV[2] <= max;		
}

inline LLVector3 operator+(const LLVector3 &a, const LLVector3 &b)
{
	LLVector3 c(a);
	return c += b;
}

inline LLVector3 operator-(const LLVector3 &a, const LLVector3 &b)
{
	LLVector3 c(a);
	return c -= b;
}

inline F32  operator*(const LLVector3 &a, const LLVector3 &b)
{
	return (a.mV[0]*b.mV[0] + a.mV[1]*b.mV[1] + a.mV[2]*b.mV[2]);
}

inline LLVector3 operator%(const LLVector3 &a, const LLVector3 &b)
{
	return LLVector3( a.mV[1]*b.mV[2] - b.mV[1]*a.mV[2], a.mV[2]*b.mV[0] - b.mV[2]*a.mV[0], a.mV[0]*b.mV[1] - b.mV[0]*a.mV[1] );
}

inline LLVector3 operator/(const LLVector3 &a, F32 k)
{
	F32 t = 1.f / k;
	return LLVector3( a.mV[0] * t, a.mV[1] * t, a.mV[2] * t );
}

inline LLVector3 operator*(const LLVector3 &a, F32 k)
{
	return LLVector3( a.mV[0] * k, a.mV[1] * k, a.mV[2] * k );
}

inline LLVector3 operator*(F32 k, const LLVector3 &a)
{
	return LLVector3( a.mV[0] * k, a.mV[1] * k, a.mV[2] * k );
}

inline bool operator==(const LLVector3 &a, const LLVector3 &b)
{
	return (  (a.mV[0] == b.mV[0])
			&&(a.mV[1] == b.mV[1])
			&&(a.mV[2] == b.mV[2]));
}

inline bool operator!=(const LLVector3 &a, const LLVector3 &b)
{
	return (  (a.mV[0] != b.mV[0])
			||(a.mV[1] != b.mV[1])
			||(a.mV[2] != b.mV[2]));
}

inline bool operator<(const LLVector3 &a, const LLVector3 &b)
{
	return (a.mV[0] < b.mV[0]
			|| (a.mV[0] == b.mV[0]
				&& (a.mV[1] < b.mV[1]
					|| (a.mV[1] == b.mV[1])
						&& a.mV[2] < b.mV[2])));
}

inline const LLVector3& operator+=(LLVector3 &a, const LLVector3 &b)
{
	a.mV[0] += b.mV[0];
	a.mV[1] += b.mV[1];
	a.mV[2] += b.mV[2];
	return a;
}

inline const LLVector3& operator-=(LLVector3 &a, const LLVector3 &b)
{
	a.mV[0] -= b.mV[0];
	a.mV[1] -= b.mV[1];
	a.mV[2] -= b.mV[2];
	return a;
}

inline const LLVector3& operator%=(LLVector3 &a, const LLVector3 &b)
{
	LLVector3 ret( a.mV[1]*b.mV[2] - b.mV[1]*a.mV[2], a.mV[2]*b.mV[0] - b.mV[2]*a.mV[0], a.mV[0]*b.mV[1] - b.mV[0]*a.mV[1]);
	a = ret;
	return a;
}

inline const LLVector3& operator*=(LLVector3 &a, F32 k)
{
	a.mV[0] *= k;
	a.mV[1] *= k;
	a.mV[2] *= k;
	return a;
}

inline const LLVector3& operator*=(LLVector3 &a, const LLVector3 &b)
{
	a.mV[0] *= b.mV[0];
	a.mV[1] *= b.mV[1];
	a.mV[2] *= b.mV[2];
	return a;
}

inline const LLVector3& operator/=(LLVector3 &a, F32 k)
{
	F32 t = 1.f / k;
	a.mV[0] *= t;
	a.mV[1] *= t;
	a.mV[2] *= t;
	return a;
}

inline LLVector3 operator-(const LLVector3 &a)
{
	return LLVector3( -a.mV[0], -a.mV[1], -a.mV[2] );
}

inline F32	dist_vec(const LLVector3 &a, const LLVector3 &b)
{
	F32 x = a.mV[0] - b.mV[0];
	F32 y = a.mV[1] - b.mV[1];
	F32 z = a.mV[2] - b.mV[2];
	return fsqrtf( x*x + y*y + z*z );
}

inline F32	dist_vec_squared(const LLVector3 &a, const LLVector3 &b)
{
	F32 x = a.mV[0] - b.mV[0];
	F32 y = a.mV[1] - b.mV[1];
	F32 z = a.mV[2] - b.mV[2];
	return x*x + y*y + z*z;
}

inline F32	dist_vec_squared2D(const LLVector3 &a, const LLVector3 &b)
{
	F32 x = a.mV[0] - b.mV[0];
	F32 y = a.mV[1] - b.mV[1];
	return x*x + y*y;
}

inline LLVector3 projected_vec(const LLVector3 &a, const LLVector3 &b)
{
	LLVector3 project_axis = b;
	project_axis.normalize();
	return project_axis * (a * project_axis);
}

inline LLVector3 lerp(const LLVector3 &a, const LLVector3 &b, F32 u)
{
	return LLVector3(
		a.mV[VX] + (b.mV[VX] - a.mV[VX]) * u,
		a.mV[VY] + (b.mV[VY] - a.mV[VY]) * u,
		a.mV[VZ] + (b.mV[VZ] - a.mV[VZ]) * u);
}


inline BOOL	LLVector3::isNull() const
{
	if ( F_APPROXIMATELY_ZERO > mV[VX]*mV[VX] + mV[VY]*mV[VY] + mV[VZ]*mV[VZ] )
	{
		return TRUE;
	}
	return FALSE;
}

inline void update_min_max(LLVector3& min, LLVector3& max, const LLVector3& pos)
{
	for (U32 i = 0; i < 3; i++)
	{
		if (min.mV[i] > pos.mV[i])
		{
			min.mV[i] = pos.mV[i];
		}
		if (max.mV[i] < pos.mV[i])
		{
			max.mV[i] = pos.mV[i];
		}
	}
}

inline F32 angle_between(const LLVector3& a, const LLVector3& b)
{
	LLVector3 an = a;
	LLVector3 bn = b;
	an.normalize();
	bn.normalize();
	F32 cosine = an * bn;
	F32 angle = (cosine >= 1.0f) ? 0.0f :
				(cosine <= -1.0f) ? F_PI :
				(F32)acos(cosine);
	return angle;
}

inline BOOL are_parallel(const LLVector3 &a, const LLVector3 &b, F32 epsilon)
{
	LLVector3 an = a;
	LLVector3 bn = b;
	an.normalize();
	bn.normalize();
	F32 dot = an * bn;
	if ( (1.0f - fabs(dot)) < epsilon)
	{
		return TRUE;
	}
	return FALSE;
}

#endif
