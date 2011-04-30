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

//MK
#include "llworld.h"
#include "lleventpoll.h"
#include "llagent.h"
//mk
#include "llappviewer.h"
#include "llfloaterteleporthistory.h"
#include "llfloaterworldmap.h"
#include "llsdserialize.h"
#include "lltimer.h"
#include "lluictrlfactory.h"
#include "llurldispatcher.h"
#include "llurlsimstring.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llweb.h"

// globals
LLFloaterTeleportHistory* gFloaterTeleportHistory;

// Helper functions

std::string getHistoryFileName()
{
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "");

	if (!path.empty())
	{
		path = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "teleport_history.xml");
	}
	else
	{
		llwarns << "Teleport History: Could not find the path to the history file. History not saved." << llendl;
		path = "";
	}
	return path;  
}

std::string get_timestamp()
{
	std::string timestamp;
	U32 utc_time = time_corrected();
	struct tm* internal_time;
	internal_time = utc_to_pacific_time(utc_time, gPacificDaylightTime);
	// check if we are in daylight savings time
	std::string timeZone = " PST";
	if (gPacificDaylightTime)
	{
		timeZone = " PDT";
	}
	// Make it easy to sort: use the Year-Month-Day ISO convention
#ifdef LOCALIZED_TIME
	std::string time_format = "%Y-%m-%d  " + gSavedSettings.getString("ShortTimeFormat");
	timeStructToFormattedString(internal_time, time_format, timestamp);
	timestamp += timeZone;
#else
	timestamp = llformat("%04d-%02d-%02d %02d:%02d ", internal_time->tm_year + 1900,
						 internal_time->tm_mon + 1, internal_time->tm_mday,
                         internal_time->tm_hour, internal_time->tm_min) + timeZone;
#endif
	return timestamp;
}

// LLFloaterTeleportHistory class

LLFloaterTeleportHistory::LLFloaterTeleportHistory()
:	LLFloater(std::string("teleport history")),
	mPlacesList(NULL),
	mID(0)
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
	setButtonsStatus();
	LLFloater::onFocusReceived();
}

BOOL LLFloaterTeleportHistory::postBuild()
{
	// make sure the cached pointer to the scroll list is valid
	mPlacesList=getChild<LLScrollListCtrl>("places_list");
	if (!mPlacesList)
	{
		llwarns << "Bad floater XML file: Coud not get pointer to places list" << llendl;
		return FALSE;
	}

	// setup callbacks for the scroll list
	mPlacesList->setDoubleClickCallback(onTeleport);
	childSetCommitCallback("places_list", onPlacesSelected, this);
	childSetAction("teleport", onTeleport, this);
	childSetAction("show_on_map", onShowOnMap, this);
	childSetAction("copy_slurl", onCopySLURL, this);
	childSetAction("clear", onClearHistory,this);
	childSetAction("close", onButtonClose,this);
	loadEntries();

	return TRUE;
}

void LLFloaterTeleportHistory::loadEntries()
{
	std::string filename = getHistoryFileName();
	if (filename.empty()) return;
	llifstream file;
	file.open(filename.c_str());
	if (file.is_open())
	{
		LLSDSerialize::fromXML(mTPlist, file);
	}
	file.close();

	for (S32 i = 0; i < (S32)mTPlist.size(); i++)
	{
		LLSD data = mTPlist[i];
		mPlacesList->addElement(data, ADD_TOP);
		mPlacesList->deselectAllItems(TRUE);
		mID++;
	}	
	setButtonsStatus();
}

void LLFloaterTeleportHistory::saveEntry(LLSD entry)
{
	mTPlist.append(entry);
	std::string filename = getHistoryFileName();
	if (filename.empty()) return;
	llofstream file;
	file.open(filename.c_str());
	LLSDSerialize::toPrettyXML(mTPlist, file);
	file.close();
}

void LLFloaterTeleportHistory::clearHistory()
{
	mTPlist.clear();
	saveEntry(mTPlist);
	mPlacesList->clearRows();
}

void LLFloaterTeleportHistory::addPendingEntry(const std::string& regionName, S16 x, S16 y, S16 z)
{
//MK
	if (gRRenabled && gAgent.mRRInterface.mContainsShowloc)
	{
		return;
	}
//mk

	// Set pending entry timestamp
	mPendingTimeString = get_timestamp();

	// Set pending region name
	mPendingRegionName = regionName;

	// Set pending position
	mPendingPosition = llformat("%d, %d, %d", x, y, z);

	// prepare simstring for later parsing
	mPendingSimString = regionName + llformat("/%d/%d/%d", x, y, z);
	mPendingSimString = LLWeb::escapeURL(mPendingSimString);

	// Prepare the SLURL
	mPendingSLURL = LLURLDispatcher::buildSLURL(regionName, x, y, z);
}

void LLFloaterTeleportHistory::addSourceEntry(const std::string& sourceSURL, const std::string& parcelName)
{
//MK
	if (gRRenabled && gAgent.mRRInterface.mContainsShowloc)
	{
		return;
	}
//mk
	std::string slurl = sourceSURL;
	size_t pos = std::string::npos;
	S32 i;

	// Find the simstring part in the passed SLURL and bail if failed
	for (i = 0; i < 4; i++)
	{
		pos = slurl.rfind('/', pos);
		if (pos == std::string::npos || pos == 0)
		{
			llwarns << "Could not parse the source SLURL (" << sourceSURL << "): TP history entry not added" << llendl;
			return;
		}
		pos--;
	}

	// Set pending SLURL
	mPendingSLURL = sourceSURL;

	// Keep the simstring
	mPendingSimString = slurl.erase(0, ++pos);

	// Extract the region name and position
	S32 x = 128, y = 128, z = 0;
	LLURLSimString::parse(mPendingSimString, &mPendingRegionName, &x, &y, &z);

	// Set pending position
	mPendingPosition = llformat("%d, %d, %d", x, y, z);

	// Set pending entry timestamp
	mPendingTimeString = get_timestamp();

	// Add this pending entry immediately, using the passed (departure) parcel name.
	addEntry(parcelName, true);
}

void LLFloaterTeleportHistory::addEntry(const std::string& parcelName, bool departure)
{
	if (mPendingRegionName.empty())
	{
		return;
	}

 	// only if the cached scroll list pointer is valid
	if (mPlacesList)
	{
		// build the list entry
		LLSD value;
		value["id"] = mID++;
		value["columns"][LIST_TYPE]["column"] = "type";
		value["columns"][LIST_TYPE]["value"] = (departure ? "D" : "A");
		value["columns"][LIST_PARCEL]["column"] = "parcel";
		value["columns"][LIST_PARCEL]["value"] = parcelName;
		value["columns"][LIST_REGION]["column"] = "region";
		value["columns"][LIST_REGION]["value"] = mPendingRegionName;
		value["columns"][LIST_POSITION]["column"] = "position";
		value["columns"][LIST_POSITION]["value"] = mPendingPosition;
		value["columns"][LIST_TIMESTAMP]["column"] = "timestamp";
		value["columns"][LIST_TIMESTAMP]["value"] = mPendingTimeString;

		// these columns are hidden and serve as data storage for simstring and SLURL
		value["columns"][LIST_SLURL]["column"] = "slurl";
		value["columns"][LIST_SLURL]["value"] = mPendingSLURL;
		value["columns"][LIST_SIMSTRING]["column"] = "simstring";
		value["columns"][LIST_SIMSTRING]["value"] = mPendingSimString;

		// add the new list entry on top of the list, deselect all and disable the buttons
		mPlacesList->addElement(value, ADD_TOP);
		mPlacesList->deselectAllItems(TRUE);
		setButtonsStatus();
		// Save the netry in the history file
		saveEntry(value);
	}

	mPendingRegionName.clear();
}

void LLFloaterTeleportHistory::setButtonsStatus()
{
	// enable or disable buttons
	bool enable = (mPlacesList && mPlacesList->getFirstSelected());
	childSetEnabled("teleport", enable);
	childSetEnabled("show_on_map", enable);
	childSetEnabled("copy_slurl", enable);
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
	if (!self) return;
	// on selection change check if we need to enable or disable buttons
	self->setButtonsStatus();
}

// static
void LLFloaterTeleportHistory::onTeleport(void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;
	if (!self) return;

	// build secondlife::/app link from simstring for instant teleport to destination
	std::string slapp = "secondlife:///app/teleport/" + self->mPlacesList->getFirstSelected()->getColumn(LIST_SIMSTRING)->getValue().asString();
	LLWebBrowserCtrl* web = NULL;
	LLURLDispatcher::dispatch(slapp, web, TRUE);
}

// static
void LLFloaterTeleportHistory::onShowOnMap(void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;
	if (!self) return;

	// get simstring from selected entry and parse it for its components
	std::string simString = self->mPlacesList->getFirstSelected()->getColumn(LIST_SIMSTRING)->getValue().asString();
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
	if (!self) return;

	// get SLURL of the selected entry and copy it to the clipboard
	std::string SLURL = self->mPlacesList->getFirstSelected()->getColumn(LIST_SLURL)->getValue().asString();
	gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(SLURL));
}

void LLFloaterTeleportHistory::onClearHistory(void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;
	if (!self) return;
	self->clearHistory();
}

void LLFloaterTeleportHistory::onButtonClose(void* data)
{
	LLFloaterTeleportHistory* self = (LLFloaterTeleportHistory*) data;
	if (!self) return;
	self->setVisible(FALSE);
}
