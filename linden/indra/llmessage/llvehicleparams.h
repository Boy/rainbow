/** 
 * @file llvehicleparams.h
 * @brief For parameter names that must be shared between the
 * scripting language and the LLVehicleAction class on the simulator.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_VEHICLE_PARAMS_H
#define LL_VEHICLE_PARAMS_H

/** 
 * The idea is that the various parameters that control vehicle
 * behavior can be tweeked by name using general-purpose script calls.
 */

typedef enum e_vehicle_param
{
	VEHICLE_TYPE_NONE,		// TYPE_0
	VEHICLE_TYPE_SLED,
	VEHICLE_TYPE_CAR,
	VEHICLE_TYPE_BOAT,
	VEHICLE_TYPE_AIRPLANE,
	VEHICLE_TYPE_BALLOON,	// TYPE_5
	VEHICLE_TYPE_6,
	VEHICLE_TYPE_7,
	VEHICLE_TYPE_8,
	VEHICLE_TYPE_9,
	VEHICLE_TYPE_10,
	VEHICLE_TYPE_11,
	VEHICLE_TYPE_12,
	VEHICLE_TYPE_13,
	VEHICLE_TYPE_14,
	VEHICLE_TYPE_15,

	// vector parameters
	VEHICLE_LINEAR_FRICTION_TIMESCALE,
	VEHICLE_ANGULAR_FRICTION_TIMESCALE,
	VEHICLE_LINEAR_MOTOR_DIRECTION,
	VEHICLE_ANGULAR_MOTOR_DIRECTION,
	VEHICLE_LINEAR_MOTOR_OFFSET,
	VEHICLE_VECTOR_PARAM_5,
	VEHICLE_VECTOR_PARAM_6,
	VEHICLE_VECTOR_PARAM_7,

	// floating point parameters
	VEHICLE_HOVER_HEIGHT,
	VEHICLE_HOVER_EFFICIENCY,
	VEHICLE_HOVER_TIMESCALE,
	VEHICLE_BUOYANCY,

	VEHICLE_LINEAR_DEFLECTION_EFFICIENCY,
	VEHICLE_LINEAR_DEFLECTION_TIMESCALE,
	VEHICLE_LINEAR_MOTOR_TIMESCALE,
	VEHICLE_LINEAR_MOTOR_DECAY_TIMESCALE,

	VEHICLE_ANGULAR_DEFLECTION_EFFICIENCY,
	VEHICLE_ANGULAR_DEFLECTION_TIMESCALE,
	VEHICLE_ANGULAR_MOTOR_TIMESCALE,
	VEHICLE_ANGULAR_MOTOR_DECAY_TIMESCALE,

	VEHICLE_VERTICAL_ATTRACTION_EFFICIENCY,
	VEHICLE_VERTICAL_ATTRACTION_TIMESCALE,

	VEHICLE_BANKING_EFFICIENCY,
	VEHICLE_BANKING_MIX,
	VEHICLE_BANKING_TIMESCALE,

	VEHICLE_FLOAT_PARAM_17,	
	VEHICLE_FLOAT_PARAM_18,	
	VEHICLE_FLOAT_PARAM_19,	

	// rotation parameters
	VEHICLE_REFERENCE_FRAME,
	VEHICLE_ROTATION_PARAM_1,
	VEHICLE_ROTATION_PARAM_2,
	VEHICLE_ROTATION_PARAM_3,

} EVehicleParam;


// some flags that effect how the vehicle moves

// zeros world-z component of linear deflection
const U32 VEHICLE_FLAG_NO_DEFLECTION_UP = 1 << 0;

// spring-loads roll only
const U32 VEHICLE_FLAG_LIMIT_ROLL_ONLY	= 1 << 1;

// hover flags
const U32 VEHICLE_FLAG_HOVER_WATER_ONLY		= 1 << 2;
const U32 VEHICLE_FLAG_HOVER_TERRAIN_ONLY	= 1 << 3;
const U32 VEHICLE_FLAG_HOVER_GLOBAL_HEIGHT 	= 1 << 4;
const U32 VEHICLE_FLAG_HOVER_UP_ONLY 		= 1 << 5;

// caps world-z component of linear motor to prevent 
// climbing up into the sky
const U32 VEHICLE_FLAG_LIMIT_MOTOR_UP		= 1 << 6;

const U32 VEHICLE_FLAG_MOUSELOOK_STEER 		= 1 << 7;
const U32 VEHICLE_FLAG_MOUSELOOK_BANK 		= 1 << 8;
const U32 VEHICLE_FLAG_CAMERA_DECOUPLED 	= 1 << 9;

#endif
