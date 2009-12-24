/** 
 * @file llfloaterteleporthistory.cpp
 * @author Zi Ree
 * @brief LLFloaterTeleportHistory class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
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

#include "llviewerprecompiledheaders.h"

#include "linden_common.h"

#include "llfloaterteleporthistory.h"
#include "llfloaterworldmap.h"
#include "lltimer.h"
#include "lluictrlfactory.h"
#include "llurldispatcher.h"
#include "llurlsimstring.h"
#include "llviewercontrol.h"   // gSavedSettings
#include "llviewerwindow.h"
#include "llweb.h"

#include "apr_time.h"

// globals
LLFloaterTeleportHistory* gFloaterTeleportHistory;

LLFloaterTeleportHistory::LLFloaterTeleportHistory()
:	LLFloater(std::string("teleporthistory")),
	mPlacesList(NULL),
	id(0)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_teleport_history.xml", NULL);
}

// virtual
LLFloaterTeleportHistory::~LLFloaterTeleportHistory()
{
}

// virtual
void LLFloaterTeleportHistory::onFocusReceived()
{
	// take care to enable or disable buttons depending on the selection in the places list
	if(mPlacesList->getFirstSelected())
	{
		setButtonsEnabled(TRUE);
	}
	else
	{
		setButtonsEnabled(FALSE);
	}
	LLFloater::onFocusReceived();
}

BOOL LLFloaterTeleportHistory::postBuild()
{
	// make sure the cached pointer to the scroll list is valid
	mPlacesList=getChild<LLScrollListCtrl>("places_list");
	if(!mPlacesList)
	{
		llwarns << "coud not get pointer to places list" << llendl;
		return FALSE;
	}

	// setup callbacks for the scroll list
	mPlacesList->setDoubleClickCallback(onTeleport);
	childSetCommitCallback("places_list", onPlacesSelected, this);
	childSetAction("teleport", onTeleport, this);
	childSetAction("show_on_map", onShowOnMap, this);
	childSetAction("copy_slurl", onCopySLURL, this);

	return TRUE;
}

void LLFloaterTeleportHistory::addEntry(std::string regionName, S16 x, S16 y, S16 z)
{
	// only if the cached scroll list pointer is valid
	if(mPlacesList)
	{
		// prepare display of position
		std::string position=llformat("%d, %d, %d", x, y, z);
		// prepare simstring for later parsing
		std::string simString = regionName + llformat("/%d/%d/%d", x, y, z); 
		simString = LLWeb::escapeURL(simString);

		// check if we are in daylight savings time
		std::string timeZone = "PST";
		if(is_daylight_savings()) timeZone = "PDT";

		// do all time related stuff as closely together as possible, because every other operation
		// might change the internal tm* buffer
		struct tm* internal_time;
		internal_time = utc_to_pacific_time(time_corrected(), is_daylight_savings());
		std::string timeString=llformat("%02d:%02d:%02d ", internal_time->tm_hour, internal_time->tm_min, internal_time->tm_sec)+timeZone;

		// build the list entry
		LLSD value;
		value["id"] = id;
		value["columns"][0]["column"] = "region";
		value["columns"][0]["value"] = regionName;
		value["columns"][1]["column"] = "position";
		value["columns"][1]["value"] = position;
		value["columns"][2]["column"] = "visited";
		value["columns"][2]["value"] = timeString;

		// these columns are hidden and serve as data storage for simstring and SLURL
		value["columns"][3]["column"] = "slurl";
		value["columns"][3]["value"] = LLURLDispatcher::buildSLURL(regionName, x, y, z);
		value["columns"][4]["column"] = "simstring";
		value["columns"][4]["value"] = simString;

		// add the new list entry on top of the list, deselect all and disable the buttons
		mPlacesList->addElement(value, ADD_TOP);
		mPlacesList->deselectAllItems(TRUE);
		setButtonsEnabled(FALSE);
		id++;
	}
	else
	{
		llwarns << "pointer to places list is NULL" << llendl;
	}
}

void LLFloaterTeleportHistory::setButtonsEnabled(BOOL on)
{
	// enable or disable buttons
	childSetEnabled("teleport", on);
	childSetEnabled("show_on_map", on);
	childSetEnabled("copy_slurl", on);
}

// virtual
void LLFloaterTeleportHistory::onClose(bool app_quitting)
{
	LLFloater::setVisible(FALSE);
}

// virtual
BOOL LLFloaterTeleportHistory::canClose()
{
	return !LLApp::isExiting();
}

// callbacks

// static
void LLFloaterTeleportHistory::onPlacesSelected(LLUICtrl* /* ctrl */, void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;

	// on selection change check if we need to enable or disable buttons
	if(self->mPlacesList->getFirstSelected())
	{
		self->setButtonsEnabled(TRUE);
	}
	else
	{
		self->setButtonsEnabled(FALSE);
	}
}

// static
void LLFloaterTeleportHistory::onTeleport(void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;

	// build secondlife::/app link from simstring for instant teleport to destination
	std::string slapp="secondlife:///app/teleport/" + self->mPlacesList->getFirstSelected()->getColumn(4)->getValue().asString();
	LLURLDispatcher::dispatch(slapp, FALSE);
}

// static
void LLFloaterTeleportHistory::onShowOnMap(void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;

	// get simstring from selected entry and parse it for its components
	std::string simString = self->mPlacesList->getFirstSelected()->getColumn(4)->getValue().asString();
	std::string region = "";
	S32 x = 128;
	S32 y = 128;
	S32 z = 20;

	LLURLSimString::parse(simString, &region, &x, &y, &z);

	// point world map at position
	gFloaterWorldMap->trackURL(region, x, y, z);
	LLFloaterWorldMap::show(NULL, TRUE);
}

// static
void LLFloaterTeleportHistory::onCopySLURL(void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;

	// get SLURL of the selected entry and copy it to the clipboard
	std::string SLURL=self->mPlacesList->getFirstSelected()->getColumn(3)->getValue().asString();
	gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(SLURL));
}
