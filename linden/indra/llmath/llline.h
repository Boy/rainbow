// llline.h
/**
 * @file llline.cpp
 * @author Andrew Meadows
 * @brief Simple line for computing nearest approach between two infinite lines
 *
 * $LicenseInfo:firstyear=2006&license=internal$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
 * 
 * The following source code is PROPRIETARY AND CONFIDENTIAL. Use of
 * this source code is governed by the Linden Lab Source Code Disclosure
 * Agreement ("Agreement") previously entered between you and Linden
 * Lab. By accessing, using, copying, modifying or distributing this
 * software, you acknowledge that you have been informed of your
 * obligations under the Agreement and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LINE_H
#define LL_LINE_H

#include <iostream>
#include "stdtypes.h"
#include "v3math.h"

const F32 DEFAULT_INTERSECTION_ERROR = 0.000001f;

class LLLine
{
public:
	LLLine();
	LLLine( const LLVector3& first_point, const LLVector3& second_point );
	virtual ~LLLine() {};

	void setPointDirection( const LLVector3& first_point, const LLVector3& second_point );
	void setPoints( const LLVector3& first_point, const LLVector3& second_point );

	bool intersects( const LLVector3& point, F32 radius = DEFAULT_INTERSECTION_ERROR ) const;

	// returns the point on this line that is closest to some_point
	LLVector3 nearestApproach( const LLVector3& some_point ) const;

	// returns the point on this line that is closest to other_line
	LLVector3 nearestApproach( const LLLine& other_line ) const;

	friend std::ostream& operator<<( std::ostream& output_stream, const LLLine& line );

	// returns 'true' if this line intersects the plane
	// on success stores the intersection point in 'result'
	bool intersectsPlane( LLVector3& result, const LLLine& plane ) const;

	// returns 'true' if planes intersect, and stores the result 
	// the second and third arguments are treated as planes
	// where mPoint is on the plane and mDirection is the normal
	// result.mPoint will be the intersection line's closest approach 
	// to first_plane.mPoint
	static bool getIntersectionBetweenTwoPlanes( LLLine& result, const LLLine& first_plane, const LLLine& second_plane );

	const LLVector3& getPoint() const { return mPoint; }
	const LLVector3& getDirection() const { return mDirection; }

protected:
	// these are protected because some code assumes that the normal is 
	// always correct and properly normalized.
	LLVector3 mPoint;
	LLVector3 mDirection;
};


#endif
