/** 
 * @file llviewerjoystick.cpp
 * @brief Joystick / NDOF device functionality.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llviewerjoystick.h"

#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llviewercamera.h"
#include "llappviewer.h"
#include "llkeyboard.h"
#include "lltoolmgr.h"
#include "llselectmgr.h"
#include "llviewermenu.h"
#include "llagent.h"
#include "llfocusmgr.h"


// ----------------------------------------------------------------------------
// Constants

#define  X_I	1
#define  Y_I	2
#define  Z_I	0
#define RX_I	4
#define RY_I	5
#define RZ_I	3

// flycam translations in build mode should be reduced
const F32 BUILDMODE_FLYCAM_T_SCALE = 3.f;

F32  LLViewerJoystick::sLastDelta[] = {0,0,0,0,0,0,0};
F32  LLViewerJoystick::sDelta[] = {0,0,0,0,0,0,0};

// These constants specify the maximum absolute value coming in from the device.
// HACK ALERT! the value of MAX_JOYSTICK_INPUT_VALUE is not arbitrary as it 
// should be.  It has to be equal to 3000 because the SpaceNavigator on Windows 
// refuses to respond to the DirectInput SetProperty call; it always returns 
// values in the [-3000, 3000] range.
#define MAX_SPACENAVIGATOR_INPUT  3000.0f
#define MAX_JOYSTICK_INPUT_VALUE  MAX_SPACENAVIGATOR_INPUT

// -----------------------------------------------------------------------------
void LLViewerJoystick::updateEnabled(bool autoenable)
{
	if (mDriverState == JDS_UNINITIALIZED)
	{
		gSavedSettings.setBOOL("JoystickEnabled", FALSE );
	}
	else
	{
		if (isLikeSpaceNavigator() && autoenable)
		{
			gSavedSettings.setBOOL("JoystickEnabled", TRUE );
		}
	}
	if (!gSavedSettings.getBOOL("JoystickEnabled"))
	{
		mOverrideCamera = FALSE;
	}
}

void LLViewerJoystick::setOverrideCamera(bool val)
{
	if (!gSavedSettings.getBOOL("JoystickEnabled"))
	{
		mOverrideCamera = FALSE;
	}
	else
	{
		mOverrideCamera = val;
	}

	if (mOverrideCamera)
	{
		gAgent.changeCameraToDefault();
	}
}

// -----------------------------------------------------------------------------
#if LIB_NDOF
NDOF_HotPlugResult LLViewerJoystick::HotPlugAddCallback(NDOF_Device *dev)
{
	NDOF_HotPlugResult res = NDOF_DISCARD_HOTPLUGGED;
	LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
	if (joystick->mDriverState == JDS_UNINITIALIZED)
	{
        llinfos << "HotPlugAddCallback: will use device:" << llendl;
		ndof_dump(dev);
		joystick->mNdofDev = dev;
        joystick->mDriverState = JDS_INITIALIZED;
        res = NDOF_KEEP_HOTPLUGGED;
	}
	joystick->updateEnabled(true);
    return res;
}
#endif

// -----------------------------------------------------------------------------
#if LIB_NDOF
void LLViewerJoystick::HotPlugRemovalCallback(NDOF_Device *dev)
{
	LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
	if (joystick->mNdofDev == dev)
	{
        llinfos << "HotPlugRemovalCallback: joystick->mNdofDev=" 
				<< joystick->mNdofDev << "; removed device:" << llendl;
		ndof_dump(dev);
		joystick->mDriverState = JDS_UNINITIALIZED;
	}
	joystick->updateEnabled(true);
}
#endif

// -----------------------------------------------------------------------------
LLViewerJoystick::LLViewerJoystick()
:	mDriverState(JDS_UNINITIALIZED),
	mNdofDev(NULL),
	mResetFlag(false),
	mCameraUpdated(true),
	mOverrideCamera(false)
{
	for (int i = 0; i < 6; i++)
	{
		mAxes[i] = sDelta[i] = sLastDelta[i] = 0.0f;
	}
	
	memset(mBtn, 0, sizeof(mBtn));

	// factor in bandwidth? bandwidth = gViewerStats->mKBitStat
	mPerfScale = 4000.f / gSysCPU.getMhz();
}

// -----------------------------------------------------------------------------
LLViewerJoystick::~LLViewerJoystick()
{
	if (mDriverState == JDS_INITIALIZED)
	{
		terminate();
	}
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::init(bool autoenable)
{
#if LIB_NDOF
	static bool libinit = false;
	mDriverState = JDS_INITIALIZING;

	if (libinit == false)
	{
		// Note: The HotPlug callbacks are not actually getting called on Windows
		if (ndof_libinit(HotPlugAddCallback, 
						 HotPlugRemovalCallback, 
						 NULL))
		{
			mDriverState = JDS_UNINITIALIZED;
		}
		else
		{
			// NB: ndof_libinit succeeds when there's no device
			libinit = true;

			// allocate memory once for an eventual device
			mNdofDev = ndof_create();
		}
	}

	if (libinit)
	{
		if (mNdofDev)
		{
			// Different joysticks will return different ranges of raw values.
			// Since we want to handle every device in the same uniform way, 
			// we initialize the mNdofDev struct and we set the range 
			// of values we would like to receive. 
			// 
			// HACK: On Windows, libndofdev passes our range to DI with a 
			// SetProperty call. This works but with one notable exception, the
			// SpaceNavigator, who doesn't seem to care about the SetProperty
			// call. In theory, we should handle this case inside libndofdev. 
			// However, the range we're setting here is arbitrary anyway, 
			// so let's just use the SpaceNavigator range for our purposes. 
			mNdofDev->axes_min = (long)-MAX_JOYSTICK_INPUT_VALUE;
			mNdofDev->axes_max = (long)+MAX_JOYSTICK_INPUT_VALUE;

			// libndofdev could be used to return deltas.  Here we choose to
			// just have the absolute values instead.
			mNdofDev->absolute = 1;

			// init & use the first suitable NDOF device found on the USB chain
			if (ndof_init_first(mNdofDev, NULL))
			{
				mDriverState = JDS_UNINITIALIZED;
				llwarns << "ndof_init_first FAILED" << llendl;
			}
			else
			{
				mDriverState = JDS_INITIALIZED;
			}
		}
		else
		{
			mDriverState = JDS_UNINITIALIZED;
		}
	}

	// Autoenable the joystick for recognized devices if nothing was connected previously
	if (!autoenable)
	{
		autoenable = gSavedSettings.getString("JoystickInitialized").empty() ? true : false;
	}
	updateEnabled(autoenable);
	
	if (mDriverState == JDS_INITIALIZED)
	{
		// A Joystick device is plugged in
		if (isLikeSpaceNavigator())
		{
			// It's a space navigator, we have defaults for it.
			if (gSavedSettings.getString("JoystickInitialized") != "SpaceNavigator")
			{
				// Only set the defaults if we haven't already (in case they were overridden)
				setSNDefaults();
				gSavedSettings.setString("JoystickInitialized", "SpaceNavigator");
			}
		}
		else
		{
			// It's not a Space Navigator
			gSavedSettings.setString("JoystickInitialized", "UnknownDevice");
		}
	}
	else
	{
		// No device connected, don't change any settings
	}
	
	llinfos << "ndof: mDriverState=" << mDriverState << "; mNdofDev=" 
			<< mNdofDev << "; libinit=" << libinit << llendl;
#endif
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::terminate()
{
#if LIB_NDOF

	ndof_libcleanup();
	llinfos << "Terminated connection with NDOF device." << llendl;

#endif
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::updateStatus()
{
#if LIB_NDOF

	ndof_update(mNdofDev);

	for (int i=0; i<6; i++)
	{
		mAxes[i] = (F32) mNdofDev->axes[i] / mNdofDev->axes_max;
	}

	for (int i=0; i<16; i++)
	{
		mBtn[i] = mNdofDev->buttons[i];
	}
	
#endif
}

// -----------------------------------------------------------------------------
F32 LLViewerJoystick::getJoystickAxis(U32 axis) const
{
	if (axis < 6)
	{
		return mAxes[axis];
	}
	return 0.f;
}

// -----------------------------------------------------------------------------
U32 LLViewerJoystick::getJoystickButton(U32 button) const
{
	if (button < 16)
	{
		return mBtn[button];
	}
	return 0;
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentJump()
{
    gAgent.moveUp(1);
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentSlide(F32 inc)
{
	if (inc < 0)
	{
		gAgent.moveLeft(1);
	}
	else if (inc > 0)
	{
		gAgent.moveLeft(-1);
	}
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentPush(F32 inc)
{
	if (inc < 0)                            // forward
	{
		gAgent.moveAt(1, false);
	}
	else if (inc > 0)                       // backward
	{
		gAgent.moveAt(-1, false);
	}
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentFly(F32 inc)
{
	if (inc < 0)
	{
		if (gAgent.getFlying())
		{
			gAgent.moveUp(1);
		}
		else
		{
			gAgent.setFlying(true);
		}
	}
	else if (inc > 0)
	{
		// crouch
		gAgent.moveUp(-1);
	}
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::agentRotate(F32 pitch_inc, F32 yaw_inc)
{
	LLQuaternion new_rot;
	pitch_inc = gAgent.clampPitchToLimits(-pitch_inc);
	const LLQuaternion qx(pitch_inc, gAgent.getLeftAxis());
	const LLQuaternion qy(-yaw_inc, gAgent.getReferenceUpVector());
	new_rot.setQuat(qx * qy);
	gAgent.rotate(new_rot);
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::resetDeltas(S32 axis[])
{
	for (U32 i = 0; i < 6; i++)
	{
		sLastDelta[i] = -mAxes[axis[i]];
		sDelta[i] = 0.f;
	}

	sLastDelta[6] = sDelta[6] = 0.f;
	mResetFlag = false;
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::moveObjects(bool reset)
{
	static bool toggle_send_to_sim = false;

	if (!gFocusMgr.getAppHasFocus() || mDriverState != JDS_INITIALIZED
		|| !gSavedSettings.getBOOL("JoystickEnabled") || !gSavedSettings.getBOOL("JoystickBuildEnabled"))
	{
		return;
	}

	S32 axis[] = 
	{
		gSavedSettings.getS32("JoystickAxis0"),
		gSavedSettings.getS32("JoystickAxis1"),
		gSavedSettings.getS32("JoystickAxis2"),
		gSavedSettings.getS32("JoystickAxis3"),
		gSavedSettings.getS32("JoystickAxis4"),
		gSavedSettings.getS32("JoystickAxis5"),
	};

	if (reset || mResetFlag)
	{
		resetDeltas(axis);
		return;
	}

	F32 axis_scale[] =
	{
		gSavedSettings.getF32("BuildAxisScale0"),
		gSavedSettings.getF32("BuildAxisScale1"),
		gSavedSettings.getF32("BuildAxisScale2"),
		gSavedSettings.getF32("BuildAxisScale3"),
		gSavedSettings.getF32("BuildAxisScale4"),
		gSavedSettings.getF32("BuildAxisScale5"),
	};

	F32 dead_zone[] =
	{
		gSavedSettings.getF32("BuildAxisDeadZone0"),
		gSavedSettings.getF32("BuildAxisDeadZone1"),
		gSavedSettings.getF32("BuildAxisDeadZone2"),
		gSavedSettings.getF32("BuildAxisDeadZone3"),
		gSavedSettings.getF32("BuildAxisDeadZone4"),
		gSavedSettings.getF32("BuildAxisDeadZone5"),
	};

	F32 cur_delta[6];
	F32 time = gFrameIntervalSeconds;

	// avoid making ridicously big movements if there's a big drop in fps 
	if (time > .2f)
	{
		time = .2f;
	}

	// max feather is 32
	F32 feather = gSavedSettings.getF32("BuildFeathering"); 
	bool is_zero = true, absolute = gSavedSettings.getBOOL("Cursor3D");
	
	for (U32 i = 0; i < 6; i++)
	{
		cur_delta[i] = -mAxes[axis[i]];
		F32 tmp = cur_delta[i];
		if (absolute)
		{
			cur_delta[i] = cur_delta[i] - sLastDelta[i];
		}
		sLastDelta[i] = tmp;
		is_zero = is_zero && (cur_delta[i] == 0.f);
			
		if (cur_delta[i] > 0)
		{
			cur_delta[i] = llmax(cur_delta[i]-dead_zone[i], 0.f);
		}
		else
		{
			cur_delta[i] = llmin(cur_delta[i]+dead_zone[i], 0.f);
		}
		cur_delta[i] *= axis_scale[i];
		
		if (!absolute)
		{
			cur_delta[i] *= time;
		}

		sDelta[i] = sDelta[i] + (cur_delta[i]-sDelta[i])*time*feather;
	}

	U32 upd_type = UPD_NONE;
	LLVector3 v;
    
	if (!is_zero)
	{
		if (sDelta[0] || sDelta[1] || sDelta[2])
		{
			upd_type |= UPD_POSITION;
			v.setVec(sDelta[0], sDelta[1], sDelta[2]);
		}
		
		if (sDelta[3] || sDelta[4] || sDelta[5])
		{
			upd_type |= UPD_ROTATION;
		}
				
		// the selection update could fail, so we won't send 
		if (LLSelectMgr::getInstance()->selectionMove(v, sDelta[3],sDelta[4],sDelta[5], upd_type))
		{
			toggle_send_to_sim = true;
		}
	}
	else if (toggle_send_to_sim)
	{
		LLSelectMgr::getInstance()->sendSelectionMove();
		toggle_send_to_sim = false;
	}
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::moveAvatar(bool reset)
{
	if (!gFocusMgr.getAppHasFocus() || mDriverState != JDS_INITIALIZED
		|| !gSavedSettings.getBOOL("JoystickEnabled") || !gSavedSettings.getBOOL("JoystickAvatarEnabled"))
	{
		return;
	}

	S32 axis[] = 
	{
		// [1 0 2 4  3  5]
		// [Z X Y RZ RX RY]
		gSavedSettings.getS32("JoystickAxis0"),
		gSavedSettings.getS32("JoystickAxis1"),
		gSavedSettings.getS32("JoystickAxis2"),
		gSavedSettings.getS32("JoystickAxis3"),
		gSavedSettings.getS32("JoystickAxis4"),
		gSavedSettings.getS32("JoystickAxis5")
	};

	if (reset || mResetFlag)
	{
		resetDeltas(axis);
		if (reset)
		{
			// Note: moving the agent triggers agent camera mode;
			//  don't do this every time we set mResetFlag (e.g. because we gained focus)
			gAgent.moveAt(0, true);
		}
		return;
	}

	if (mBtn[1] == 1)
    {
		agentJump();
		return;
    }

	F32 axis_scale[] =
	{
		gSavedSettings.getF32("AvatarAxisScale0"),
		gSavedSettings.getF32("AvatarAxisScale1"),
		gSavedSettings.getF32("AvatarAxisScale2"),
		gSavedSettings.getF32("AvatarAxisScale3"),
		gSavedSettings.getF32("AvatarAxisScale4"),
		gSavedSettings.getF32("AvatarAxisScale5")
	};

	F32 dead_zone[] =
	{
		gSavedSettings.getF32("AvatarAxisDeadZone0"),
		gSavedSettings.getF32("AvatarAxisDeadZone1"),
		gSavedSettings.getF32("AvatarAxisDeadZone2"),
		gSavedSettings.getF32("AvatarAxisDeadZone3"),
		gSavedSettings.getF32("AvatarAxisDeadZone4"),
		gSavedSettings.getF32("AvatarAxisDeadZone5")
	};

	// time interval in seconds between this frame and the previous
	F32 time = gFrameIntervalSeconds;

	// avoid making ridicously big movements if there's a big drop in fps 
	if (time > .2f)
	{
		time = .2f;
	}

	// note: max feather is 32.0
	F32 feather = gSavedSettings.getF32("AvatarFeathering"); 
	
	F32 cur_delta[6];
	F32 val, dom_mov = 0.f;
	U32 dom_axis = Z_I;
#if LIB_NDOF
    bool absolute = (gSavedSettings.getBOOL("Cursor3D") && mNdofDev->absolute);
#else
    bool absolute = false;
#endif
	// remove dead zones and determine biggest movement on the joystick 
	for (U32 i = 0; i < 6; i++)
	{
		cur_delta[i] = -mAxes[axis[i]];
		if (absolute)
		{
			F32 tmp = cur_delta[i];
			cur_delta[i] = cur_delta[i] - sLastDelta[i];
			sLastDelta[i] = tmp;
		}

		if (cur_delta[i] > 0)
		{
			cur_delta[i] = llmax(cur_delta[i]-dead_zone[i], 0.f);
		}
		else
		{
			cur_delta[i] = llmin(cur_delta[i]+dead_zone[i], 0.f);
		}

		// we don't care about Roll (RZ) and Z is calculated after the loop
        if (i != Z_I && i != RZ_I)
		{
			// find out the axis with the biggest joystick motion
			val = fabs(cur_delta[i]);
			if (val > dom_mov)
			{
				dom_axis = i;
				dom_mov = val;
			}
		}
	}

	// forward|backward movements overrule the real dominant movement if 
	// they're bigger than its 20%. This is what you want cos moving forward
	// is what you do most. We also added a special (even more lenient) case 
	// for RX|RY to allow walking while pitching n' turning
	if (fabs(cur_delta[Z_I]) > .2f * dom_mov
		|| ((dom_axis == RX_I || dom_axis == RY_I) 
			&& fabs(cur_delta[Z_I]) > .05f * dom_mov))
    {
		dom_axis = Z_I;
	}

	sDelta[X_I] = -cur_delta[X_I] * axis_scale[X_I];
	sDelta[Y_I] = -cur_delta[Y_I] * axis_scale[Y_I];
	sDelta[Z_I] = -cur_delta[Z_I] * axis_scale[Z_I];
	cur_delta[RX_I] *= -axis_scale[RX_I] * mPerfScale;
	cur_delta[RY_I] *= -axis_scale[RY_I] * mPerfScale;
		
	if (!absolute)
	{
		cur_delta[RX_I] *= time;
		cur_delta[RY_I] *= time;
	}
	sDelta[RX_I] += (cur_delta[RX_I] - sDelta[RX_I]) * time * feather;
	sDelta[RY_I] += (cur_delta[RY_I] - sDelta[RY_I]) * time * feather;
	
    switch (dom_axis)
    {
        case X_I:                                         // move sideways
			agentSlide(sDelta[X_I]);
            break;
        
        case Z_I:                                         // forward/back
		{
            agentPush(sDelta[Z_I]);
            
            if (fabs(sDelta[Y_I])  > .1f)
			{
				agentFly(sDelta[Y_I]);
			}
		
			// too many rotations during walking can be confusing, so apply
			// the deadzones one more time (quick & dirty), at 50%|30% power
			F32 eff_rx = .3f * dead_zone[RX_I];
			F32 eff_ry = .3f * dead_zone[RY_I];
		
			if (sDelta[RX_I] > 0)
			{
				eff_rx = llmax(sDelta[RX_I] - eff_rx, 0.f);
			}
			else
			{
				eff_rx = llmin(sDelta[RX_I] + eff_rx, 0.f);
			}

			if (sDelta[RY_I] > 0)
			{
				eff_ry = llmax(sDelta[RY_I] - eff_ry, 0.f);
			}
			else
			{
				eff_ry = llmin(sDelta[RY_I] + eff_ry, 0.f);
			}
			
			
			if (fabs(eff_rx) > 0.f || fabs(eff_ry) > 0.f)
			{
				if (gAgent.getFlying())
				{
					agentRotate(eff_rx, eff_ry);
				}
				else
				{
					agentRotate(eff_rx, 2.f * eff_ry);
				}
			}
            break;
		}   
        case Y_I:                                          // up/crouch
            agentFly(sDelta[Y_I]);
            break;
            
        case RX_I:                                         // pitch
        case RY_I:                                         // turn
			agentRotate(sDelta[RX_I], sDelta[RY_I]);
            break;
        // case RZ_I: roll is unused in avatar mode
    }// switch
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::moveFlycam(bool reset)
{
	static LLQuaternion 		sFlycamRotation;
	static LLVector3    		sFlycamPosition;
	static F32          		sFlycamZoom;
	
	if (!gFocusMgr.getAppHasFocus() || mDriverState != JDS_INITIALIZED
		|| !gSavedSettings.getBOOL("JoystickEnabled") || !gSavedSettings.getBOOL("JoystickFlycamEnabled"))
	{
		return;
	}

	S32 axis[] = 
	{
		gSavedSettings.getS32("JoystickAxis0"),
		gSavedSettings.getS32("JoystickAxis1"),
		gSavedSettings.getS32("JoystickAxis2"),
		gSavedSettings.getS32("JoystickAxis3"),
		gSavedSettings.getS32("JoystickAxis4"),
		gSavedSettings.getS32("JoystickAxis5"),
		gSavedSettings.getS32("JoystickAxis6")
	};

	bool in_build_mode = LLToolMgr::getInstance()->inBuildMode();
	if (reset || mResetFlag)
	{
		sFlycamPosition = LLViewerCamera::getInstance()->getOrigin();
		sFlycamRotation = LLViewerCamera::getInstance()->getQuaternion();
		sFlycamZoom = LLViewerCamera::getInstance()->getView();
		
		resetDeltas(axis);

		return;
	}

	F32 axis_scale[] =
	{
		gSavedSettings.getF32("FlycamAxisScale0"),
		gSavedSettings.getF32("FlycamAxisScale1"),
		gSavedSettings.getF32("FlycamAxisScale2"),
		gSavedSettings.getF32("FlycamAxisScale3"),
		gSavedSettings.getF32("FlycamAxisScale4"),
		gSavedSettings.getF32("FlycamAxisScale5"),
		gSavedSettings.getF32("FlycamAxisScale6")
	};

	F32 dead_zone[] =
	{
		gSavedSettings.getF32("FlycamAxisDeadZone0"),
		gSavedSettings.getF32("FlycamAxisDeadZone1"),
		gSavedSettings.getF32("FlycamAxisDeadZone2"),
		gSavedSettings.getF32("FlycamAxisDeadZone3"),
		gSavedSettings.getF32("FlycamAxisDeadZone4"),
		gSavedSettings.getF32("FlycamAxisDeadZone5"),
		gSavedSettings.getF32("FlycamAxisDeadZone6")
	};

	F32 time = gFrameIntervalSeconds;

	// avoid making ridicously big movements if there's a big drop in fps 
	if (time > .2f)
	{
		time = .2f;
	}

	F32 cur_delta[7];
	F32 feather = gSavedSettings.getF32("FlycamFeathering");
	bool absolute = gSavedSettings.getBOOL("Cursor3D");

	for (U32 i = 0; i < 7; i++)
	{
		cur_delta[i] = -getJoystickAxis(axis[i]);


		F32 tmp = cur_delta[i];
		if (absolute)
		{
			cur_delta[i] = cur_delta[i] - sLastDelta[i];
		}
		sLastDelta[i] = tmp;

		if (cur_delta[i] > 0)
		{
			cur_delta[i] = llmax(cur_delta[i]-dead_zone[i], 0.f);
		}
		else
		{
			cur_delta[i] = llmin(cur_delta[i]+dead_zone[i], 0.f);
		}

		// we need smaller camera movements in build mode
		// NOTE: this needs to remain after the deadzone calculation, otherwise
		// we have issues with flycam "jumping" when the build dialog is opened/closed  -Nyx
		if (in_build_mode)
		{
			if (i == X_I || i == Y_I || i == Z_I)
			{
				cur_delta[i] /= BUILDMODE_FLYCAM_T_SCALE;
			}
		}

		cur_delta[i] *= axis_scale[i];
		
		if (!absolute)
		{
			cur_delta[i] *= time;
		}

		sDelta[i] = sDelta[i] + (cur_delta[i]-sDelta[i])*time*feather;
	}
	
	sFlycamPosition += LLVector3(sDelta) * sFlycamRotation;

	LLMatrix3 rot_mat(sDelta[3], sDelta[4], sDelta[5]);
	sFlycamRotation = LLQuaternion(rot_mat)*sFlycamRotation;

	if (gSavedSettings.getBOOL("AutoLeveling"))
	{
		LLMatrix3 level(sFlycamRotation);

		LLVector3 x = LLVector3(level.mMatrix[0]);
		LLVector3 y = LLVector3(level.mMatrix[1]);
		LLVector3 z = LLVector3(level.mMatrix[2]);

		y.mV[2] = 0.f;
		y.normVec();

		level.setRows(x,y,z);
		level.orthogonalize();
				
		LLQuaternion quat(level);
		sFlycamRotation = nlerp(llmin(feather*time,1.f), sFlycamRotation, quat);
	}

	if (gSavedSettings.getBOOL("ZoomDirect"))
	{
		sFlycamZoom = sLastDelta[6]*axis_scale[6]+dead_zone[6];
	}
	else
	{
		sFlycamZoom += sDelta[6];
	}

	LLMatrix3 mat(sFlycamRotation);

	LLViewerCamera::getInstance()->setView(sFlycamZoom);
	LLViewerCamera::getInstance()->setOrigin(sFlycamPosition);
	LLViewerCamera::getInstance()->mXAxis = LLVector3(mat.mMatrix[0]);
	LLViewerCamera::getInstance()->mYAxis = LLVector3(mat.mMatrix[1]);
	LLViewerCamera::getInstance()->mZAxis = LLVector3(mat.mMatrix[2]);
}

// -----------------------------------------------------------------------------
bool LLViewerJoystick::toggleFlycam()
{
	if (!gSavedSettings.getBOOL("JoystickEnabled") || !gSavedSettings.getBOOL("JoystickFlycamEnabled"))
	{
		return false;
	}
	if (!mOverrideCamera)
	{
		gAgent.changeCameraToDefault();
	}

	mOverrideCamera = !mOverrideCamera;
	if (mOverrideCamera)
	{
		moveFlycam(true);
		
	}
	else if (!LLToolMgr::getInstance()->inBuildMode())
	{
		moveAvatar(true);
	}
	else 
	{
		// we are in build mode, exiting from the flycam mode: since we are 
		// going to keep the flycam POV for the main camera until the avatar
		// moves, we need to track this situation.
		setCameraNeedsUpdate(false);
		setNeedsReset(true);
	}
	return true;
}

void LLViewerJoystick::scanJoystick()
{
	if (mDriverState != JDS_INITIALIZED || !gSavedSettings.getBOOL("JoystickEnabled"))
	{
		return;
	}

#if LL_WINDOWS
	// On windows, the flycam is updated syncronously with a timer, so there is
	// no need to update the status of the joystick here.
	if (!mOverrideCamera)
#endif
	updateStatus();

	static long toggle_flycam = 0;

	if (mBtn[0] == 1)
    {
		if (mBtn[0] != toggle_flycam)
		{
			toggle_flycam = toggleFlycam() ? 1 : 0;
		}
	}
	else
	{
		toggle_flycam = 0;
	}
	
	if (!mOverrideCamera && !LLToolMgr::getInstance()->inBuildMode())
	{
		moveAvatar();
	}
}

// -----------------------------------------------------------------------------
std::string LLViewerJoystick::getDescription()
{
	std::string res;
#if LIB_NDOF
	if (mDriverState == JDS_INITIALIZED && mNdofDev)
	{
		res = ll_safe_string(mNdofDev->product);
	}
#endif
	return res;
}

bool LLViewerJoystick::isLikeSpaceNavigator() const
{
#if LIB_NDOF	
	return (isJoystickInitialized() 
			&& (strncmp(mNdofDev->product, "SpaceNavigator", 14) == 0
				|| strncmp(mNdofDev->product, "SpaceExplorer", 13) == 0
				|| strncmp(mNdofDev->product, "SpaceTraveler", 13) == 0
				|| strncmp(mNdofDev->product, "SpacePilot", 10) == 0));
#else
	return false;
#endif
}

// -----------------------------------------------------------------------------
void LLViewerJoystick::setSNDefaults()
{
#if LL_DARWIN 
#define kPlatformScale	20.f
#else
#define kPlatformScale	1.f
#endif
	
	//gViewerWindow->alertXml("CacheWillClear");
	llinfos << "restoring SpaceNavigator defaults..." << llendl;
	
	gSavedSettings.setS32("JoystickAxis0", 1); // z (at)
	gSavedSettings.setS32("JoystickAxis1", 0); // x (slide)
	gSavedSettings.setS32("JoystickAxis2", 2); // y (up)
	gSavedSettings.setS32("JoystickAxis3", 4); // pitch
	gSavedSettings.setS32("JoystickAxis4", 3); // roll 
	gSavedSettings.setS32("JoystickAxis5", 5); // yaw
	gSavedSettings.setS32("JoystickAxis6", -1);
	
#if LL_DARWIN
	// The SpaceNavigator doesn't act as a 3D cursor on OS X. 
	gSavedSettings.setBOOL("Cursor3D", false);
#else
	gSavedSettings.setBOOL("Cursor3D", true);
#endif
	gSavedSettings.setBOOL("AutoLeveling", true);
	gSavedSettings.setBOOL("ZoomDirect", false);
	
	gSavedSettings.setF32("AvatarAxisScale0", 1.f);
	gSavedSettings.setF32("AvatarAxisScale1", 1.f);
	gSavedSettings.setF32("AvatarAxisScale2", 1.f);
	gSavedSettings.setF32("AvatarAxisScale4", .1f * kPlatformScale);
	gSavedSettings.setF32("AvatarAxisScale5", .1f * kPlatformScale);
	gSavedSettings.setF32("AvatarAxisScale3", 0.f * kPlatformScale);
	gSavedSettings.setF32("BuildAxisScale1", .3f * kPlatformScale);
	gSavedSettings.setF32("BuildAxisScale2", .3f * kPlatformScale);
	gSavedSettings.setF32("BuildAxisScale0", .3f * kPlatformScale);
	gSavedSettings.setF32("BuildAxisScale4", .3f * kPlatformScale);
	gSavedSettings.setF32("BuildAxisScale5", .3f * kPlatformScale);
	gSavedSettings.setF32("BuildAxisScale3", .3f * kPlatformScale);
	gSavedSettings.setF32("FlycamAxisScale1", 2.f * kPlatformScale);
	gSavedSettings.setF32("FlycamAxisScale2", 2.f * kPlatformScale);
	gSavedSettings.setF32("FlycamAxisScale0", 2.1f * kPlatformScale);
	gSavedSettings.setF32("FlycamAxisScale4", .1f * kPlatformScale);
	gSavedSettings.setF32("FlycamAxisScale5", .15f * kPlatformScale);
	gSavedSettings.setF32("FlycamAxisScale3", 0.f * kPlatformScale);
	gSavedSettings.setF32("FlycamAxisScale6", 0.f * kPlatformScale);
	
	gSavedSettings.setF32("AvatarAxisDeadZone0", .1f);
	gSavedSettings.setF32("AvatarAxisDeadZone1", .1f);
	gSavedSettings.setF32("AvatarAxisDeadZone2", .1f);
	gSavedSettings.setF32("AvatarAxisDeadZone3", 1.f);
	gSavedSettings.setF32("AvatarAxisDeadZone4", .02f);
	gSavedSettings.setF32("AvatarAxisDeadZone5", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone0", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone1", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone2", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone3", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone4", .01f);
	gSavedSettings.setF32("BuildAxisDeadZone5", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone0", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone1", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone2", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone3", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone4", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone5", .01f);
	gSavedSettings.setF32("FlycamAxisDeadZone6", 1.f);
	
	gSavedSettings.setF32("AvatarFeathering", 6.f);
	gSavedSettings.setF32("BuildFeathering", 12.f);
	gSavedSettings.setF32("FlycamFeathering", 5.f);
}
