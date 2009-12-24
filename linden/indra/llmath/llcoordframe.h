/** 
 * @file llcoordframe.h
 * @brief LLCoordFrame class header file.
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

#ifndef LL_COORDFRAME_H
#define LL_COORDFRAME_H

#include "v3math.h"
#include "v4math.h"
#include "llerror.h"

// XXX : The constructors of the LLCoordFrame class assume that all vectors
//		 and quaternion being passed as arguments are normalized, and all matrix 
// 		 arguments are unitary.  VERY BAD things will happen if these assumptions fail.
//		 Also, segfault hazzards exist in methods that accept F32* arguments.


class LLCoordFrame 
{
public:
	LLCoordFrame();											// Inits at zero with identity rotation
	explicit LLCoordFrame(const LLVector3 &origin);			// Sets origin, and inits rotation = Identity
	LLCoordFrame(const LLVector3 &x_axis, 
				 const LLVector3 &y_axis, 
				 const LLVector3 &z_axis);					// Sets coordinate axes and inits origin at zero
	LLCoordFrame(const LLVector3 &origin, 
				 const LLVector3 &x_axis, 
				 const LLVector3 &y_axis, 
				 const LLVector3 &z_axis);					// Sets the origin and coordinate axes
	LLCoordFrame(const LLVector3 &origin, 
				 const LLMatrix3 &rotation);				// Sets axes to 3x3 matrix
	LLCoordFrame(const LLVector3 &origin, 
				 const LLVector3 &direction);				// Sets origin and calls lookDir(direction)
	explicit LLCoordFrame(const LLQuaternion &q);			// Sets axes using q and inits mOrigin to zero 
	LLCoordFrame(const LLVector3 &origin, 
				 const LLQuaternion &q);					// Uses quaternion to init axes
	explicit LLCoordFrame(const LLMatrix4 &mat);			// Extracts frame from a 4x4 matrix
	// The folowing two constructors are dangerous due to implicit casting and have been disabled - SJB
	//LLCoordFrame(const F32 *origin, const F32 *rotation);	// Assumes "origin" is 1x3 and "rotation" is 1x9 array
	//LLCoordFrame(const F32 *origin_and_rotation);			// Assumes "origin_and_rotation" is 1x12 array

	BOOL isFinite() { return mOrigin.isFinite() && mXAxis.isFinite() && mYAxis.isFinite() && mZAxis.isFinite(); }

	void reset();
	void resetAxes();

	void setOrigin(F32 x, F32 y, F32 z);					// Set mOrigin
	void setOrigin(const LLVector3 &origin);
	void setOrigin(const F32 *origin);
	void setOrigin(const LLCoordFrame &frame);

	inline void setOriginX(F32 x) { mOrigin.mV[VX] = x; }
	inline void setOriginY(F32 y) { mOrigin.mV[VY] = y; }
	inline void setOriginZ(F32 z) { mOrigin.mV[VZ] = z; }

	void setAxes(const LLVector3 &x_axis, 					// Set axes
				 const LLVector3 &y_axis, 
				 const LLVector3 &z_axis);
	void setAxes(const LLMatrix3 &rotation_matrix);
	void setAxes(const LLQuaternion &q);
	void setAxes(const F32 *rotation_matrix);
	void setAxes(const LLCoordFrame &frame);

	void translate(F32 x, F32 y, F32 z);					// Move mOrgin
	void translate(const LLVector3 &v);
	void translate(const F32 *origin);

	void rotate(F32 angle, F32 x, F32 y, F32 z);			// Move axes
	void rotate(F32 angle, const LLVector3 &rotation_axis);
	void rotate(const LLQuaternion &q);
	void rotate(const LLMatrix3 &m);

	void orthonormalize();	// Makes sure axes are unitary and orthogonal.

	// These methods allow rotations in the LLCoordFrame's frame
	void roll(F32 angle);		// RH rotation about mXAxis, radians
	void pitch(F32 angle);		// RH rotation about mYAxis, radians
	void yaw(F32 angle);		// RH rotation about mZAxis, radians

	inline const LLVector3 &getOrigin() const { return mOrigin; }

	inline const LLVector3 &getXAxis() const  { return mXAxis; }
	inline const LLVector3 &getYAxis() const  { return mYAxis; }
	inline const LLVector3 &getZAxis() const  { return mZAxis; }

	inline const LLVector3 &getAtAxis() const   { return mXAxis; }
	inline const LLVector3 &getLeftAxis() const { return mYAxis; }
	inline const LLVector3 &getUpAxis() const   { return mZAxis; }
	
	// These return representations of the rotation or orientation of the LLFrame
	// it its absolute frame.  That is, these rotations acting on the X-axis {1,0,0}
	// will produce the mXAxis.
	//		LLMatrix3 getMatrix3() const;				// Returns axes in 3x3 matrix 
	LLQuaternion getQuaternion() const;			// Returns axes in quaternion form

	// Same as above, except it also includes the translation of the LLFrame
	//		LLMatrix4 getMatrix4() const;				// Returns position and axes in 4x4 matrix

	// Returns matrix which expresses point in local frame in the parent frame
	void getMatrixToParent(LLMatrix4 &mat) const;
	// Returns matrix which expresses point in parent frame in the local frame
	void getMatrixToLocal(LLMatrix4 &mat) const; // Returns matrix which expresses point in parent frame in the local frame

	void getRotMatrixToParent(LLMatrix4 &mat) const;

	// Copies mOrigin, then the three axes to buffer, returns number of bytes copied.
	size_t writeOrientation(char *buffer) const;
		
	// Copies mOrigin, then the three axes from buffer, returns the number of bytes copied.
	// Assumes the data in buffer is correct.
	size_t readOrientation(const char *buffer);

	LLVector3 rotateToLocal(const LLVector3 &v) const;		// Returns v' rotated to local
	LLVector4 rotateToLocal(const LLVector4 &v) const;		// Returns v' rotated to local
	LLVector3 rotateToAbsolute(const LLVector3 &v) const;	// Returns v' rotated to absolute
	LLVector4 rotateToAbsolute(const LLVector4 &v) const;	// Returns v' rotated to absolute

	LLVector3 transformToLocal(const LLVector3 &v) const;		// Returns v' in local coord
	LLVector4 transformToLocal(const LLVector4 &v) const;		// Returns v' in local coord
	LLVector3 transformToAbsolute(const LLVector3 &v) const;	// Returns v' in absolute coord
	LLVector4 transformToAbsolute(const LLVector4 &v) const;	// Returns v' in absolute coord

	// Write coord frame orientation into provided array in OpenGL matrix format.
	void getOpenGLTranslation(F32 *ogl_matrix) const;
	void getOpenGLRotation(F32 *ogl_matrix) const;
	void getOpenGLTransform(F32 *ogl_matrix) const;

	// lookDir orients to (xuv, presumed normalized) and does not affect origin
	void lookDir(const LLVector3 &xuv, const LLVector3 &up);
	void lookDir(const LLVector3 &xuv); // up = 0,0,1
	// lookAt orients to (point_of_interest - origin) and sets origin
	void lookAt(const LLVector3 &origin, const LLVector3 &point_of_interest, const LLVector3 &up);
	void lookAt(const LLVector3 &origin, const LLVector3 &point_of_interest); // up = 0,0,1

	// deprecated
	void setOriginAndLookAt(const LLVector3 &origin, const LLVector3 &up, const LLVector3 &point_of_interest)
	{
		lookAt(origin, point_of_interest, up);
	}
	
	friend std::ostream& operator<<(std::ostream &s, const LLCoordFrame &C);

	// These vectors are in absolute frame
	LLVector3 mOrigin;
	LLVector3 mXAxis;
	LLVector3 mYAxis;
	LLVector3 mZAxis;
};


#endif

