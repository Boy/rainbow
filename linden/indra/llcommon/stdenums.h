/** 
 * @file stdenums.h
 * @brief Enumerations for indra.
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

#ifndef LL_STDENUMS_H
#define LL_STDENUMS_H

//----------------------------------------------------------------------------
// DEPRECATED - create new, more specific files for shared enums/constants
//----------------------------------------------------------------------------

// this enum is used by the llview.h (viewer) and the llassetstorage.h (viewer and sim) 
enum EDragAndDropType
{
	DAD_NONE			= 0,
	DAD_TEXTURE			= 1,
	DAD_SOUND			= 2,
	DAD_CALLINGCARD		= 3,
	DAD_LANDMARK		= 4,
	DAD_SCRIPT			= 5,
	DAD_CLOTHING 		= 6,
	DAD_OBJECT			= 7,
	DAD_NOTECARD		= 8,
	DAD_CATEGORY		= 9,
	DAD_ROOT_CATEGORY 	= 10,
	DAD_BODYPART		= 11,
	DAD_ANIMATION		= 12,
	DAD_GESTURE			= 13,
	DAD_LINK			= 14,
	DAD_COUNT			= 15,   // number of types in this enum
};

// Reasons for drags to be denied.
// ordered by priority for multi-drag
enum EAcceptance
{
	ACCEPT_POSTPONED,	// we are asynchronously determining acceptance
	ACCEPT_NO,			// Uninformative, general purpose denial.
	ACCEPT_NO_LOCKED,	// Operation would be valid, but permissions are set to disallow it.
	ACCEPT_YES_COPY_SINGLE,	// We'll take a copy of a single item
	ACCEPT_YES_SINGLE,		// Accepted. OK to drag and drop single item here.
	ACCEPT_YES_COPY_MULTI,	// We'll take a copy of multiple items
	ACCEPT_YES_MULTI		// Accepted. OK to drag and drop multiple items here.
};

// This is used by the DeRezObject message to determine where to put
// derezed tasks.
enum EDeRezDestination
{
	DRD_SAVE_INTO_AGENT_INVENTORY = 0,
	DRD_ACQUIRE_TO_AGENT_INVENTORY = 1,		// try to leave copy in world
	DRD_SAVE_INTO_TASK_INVENTORY = 2,
	DRD_ATTACHMENT = 3,
	DRD_TAKE_INTO_AGENT_INVENTORY = 4,		// delete from world
	DRD_FORCE_TO_GOD_INVENTORY = 5,			// force take copy
	DRD_TRASH = 6,
	DRD_ATTACHMENT_TO_INV = 7,
	DRD_ATTACHMENT_EXISTS = 8,
	DRD_RETURN_TO_OWNER = 9,				// back to owner's inventory
	DRD_RETURN_TO_LAST_OWNER = 10,			// deeded object back to last owner's inventory

	DRD_COUNT = 11
};


// This is used by the return to owner code to determine the reason
// that this object is being returned.
enum EReturnReason
{
	RR_GENERIC = 0,
	RR_SANDBOX = 1,
	RR_PARCEL_OWNER = 2,
	RR_PARCEL_AUTO = 3,
	RR_PARCEL_FULL = 4,
	RR_OFF_WORLD = 5,
	
	RR_COUNT = 6
};

// This is used for filling in the first byte of the ExtraID field of
// the ObjectProperties message.
enum EObjectPropertiesExtraID
{
	OPEID_NONE = 0,
	OPEID_ASSET_ID = 1,
	OPEID_FROM_TASK_ID = 2,

	OPEID_COUNT = 3
};

enum EAddPosition
{
	ADD_TOP,
	ADD_SORTED,
	ADD_BOTTOM
};

enum LLGroupChange
{
	GC_PROPERTIES,
	GC_MEMBER_DATA,
	GC_ROLE_DATA,
	GC_ROLE_MEMBER_DATA,
	GC_TITLES,
	GC_ALL
};

//----------------------------------------------------------------------------
// DEPRECATED - create new, more specific files for shared enums/constants
//----------------------------------------------------------------------------

#endif
