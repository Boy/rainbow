/**
 * @file llfloaterteleport.h
 * @brief floater header for agentd teleports.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Linden Research, Inc.
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
// Teleport floater for agent domain TPs using URIs.
//Copyright International Business Machines Corporation 2008-9 
//Contributed to Linden Research, Inc. under the Second Life Viewer Contribution
//Agreement and licensed as above.
#ifndef LL_FLOATER_TELEPORT_H
#define LL_FLOATER_TELEPORT_H
#include "llfloater.h"
 
class LLFloaterTeleport : public LLFloater
{
public:
	LLFloaterTeleport();

	virtual ~LLFloaterTeleport();

	// by convention, this shows the floater and does instance management
	static void show(void*);
 
private:
	// when a line editor loses keyboard focus, it is committed.
	// commit callbacks are named onCommitWidgetName by convention.
	static void onCommitTeleport(LLUICtrl* ctrl, void *userdata);
 
	// by convention, button callbacks are named onClickButtonLabel
	static void onClickTeleport(void* userdata);
	static void onClickCancel(void *userdata);
 
	// no pointers to widgets here - they are referenced by name

	// assuming we just need one, which is typical
	static LLFloaterTeleport* sInstance;

};
#endif

