/** 
 * @file RRInterface.cpp
 * @author Marine Kelley
 * @brief Implementation of the RLV features
 *
 * RLV Source Code
 * The source code in this file ("Source Code") is provided by Marine Kelley
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Marine Kelley.  Terms of
 * the GPL can be found in doc/GPL-license.txt in the distribution of the
 * original source of the Second Life Viewer, or online at 
 * http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL SOURCE CODE FROM MARINE KELLEY IS PROVIDED "AS IS." MARINE KELLEY 
 * MAKES NO WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING 
 * ITS ACCURACY, COMPLETENESS OR PERFORMANCE.
 */

#include "llviewerprecompiledheaders.h"

#include "llagent.h"
#include "llcachename.h"
#include "lldrawpoolalpha.h"
#include "llfloaterchat.h"
#include "llfloaterdaycycle.h"
#include "llfloaterenvsettings.h"
#include "llfloatermap.h"
#include "llfloaterpostprocess.h"
#include "hbfloaterrlv.h"
#include "llfloatersettingsdebug.h"
#include "llfloaterwater.h"
#include "llfloaterwindlight.h"
#include "llfloaterworldmap.h"
#include "llfocusmgr.h"
#include "llhudtext.h"
#include "llinventorybridge.h"
#include "llinventoryview.h"
#include "lloverlaybar.h"
#include "llselectmgr.h"
#include "llstartup.h"
#include "lltracker.h"
#include "llurlsimstring.h"
#include "llviewermenu.h"
#include "llviewermessage.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llwaterparammanager.h"
#include "llwlparammanager.h"
#include "llworld.h"
#include "pipeline.h"

#include "RRInterface.h"

// Global and static variables initialization.
BOOL gRRenabled = TRUE;
BOOL RRInterface::sRRNoSetEnv = FALSE;
BOOL RRInterface::sRestrainedLoveDebug = FALSE;

#if !defined(max)
#define max(a, b)	((a) > (b) ? (a) : (b))
#endif

// --
// Local functions
std::string dumpList2String (std::deque<std::string> list, std::string sep, int size = -1)
{
	bool found_one = false;
	if (size < 0) size = (int)list.size();
	std::string res = "";
	for (int i = 0; i < (int)list.size() && i < size; ++i) {
		if (found_one) res += sep;
		found_one = true;
		res += list[i];
	}
	return res;
}

int match (std::deque<std::string> list, std::string str, bool& exact_match)
{
	// does str contain list[0]/list[1]/.../list[n] ?
	// yes => return the size of the list
	// no  => try again after removing the last element
	// return 0 if never found
	// Exception : if str starts with a "~" character, the match must be exact
	// exact_match is set to true for strict matching, false otherwise.
	unsigned int size = list.size();
	std::string dump;
	exact_match = false;
	while (size > 0) {
		dump = dumpList2String (list, "/", (int)size);
		if (str == dump) {
			exact_match = true;
			return (int)size;
		} else if (str != "" && str[0] == '~') {
			return 0;
		} else if (str.find (dump) != -1) {
			return (int)size;
		}
		size--;
	}
	return 0;
}

std::deque<std::string> getSubList (std::deque<std::string> list, int min, int max = -1)
{
	if (min < 0) min = 0;
	if (max < 0) max = list.size() - 1;
	std::deque<std::string> res;
	for (int i = min; i <= max; ++i) {
		res.push_back (list[i]);
	}
	return res;
}

bool findMultiple (std::deque<std::string> list, std::string str)
{
	// returns true if all the tokens in list are contained into str
	unsigned int size = list.size();
	for (unsigned int i = 0; i < size; i++) {
		if (str.find (list[i]) == -1) return false;
	}
	return true;
}

void refreshCachedVariable (std::string var)
{
	// Call this function when adding/removing a restriction only, i.e. in this file
	// Test the cached variables in the code of the viewer itself
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) return;

	BOOL contained = gAgent.mRRInterface.contains (var);
	if (var == "detach" || var.find ("detach:") == 0 || var.find ("addattach") == 0 || var.find ("remattach") == 0) {
		contained = gAgent.mRRInterface.contains("detach")
		|| gAgent.mRRInterface.containsSubstr("detach:")
		|| gAgent.mRRInterface.containsSubstr("addattach")
		|| gAgent.mRRInterface.containsSubstr("remattach");
		gAgent.mRRInterface.mContainsDetach = contained;
		gAgent.mRRInterface.mHasLockedHuds = gAgent.mRRInterface.hasLockedHuds();
		if (gAgent.mRRInterface.mHasLockedHuds) {
			// To force the viewer to render the HUDs again, just in case
			LLPipeline::sShowHUDAttachments = TRUE;
		}
	}
	else if (var == "showinv")				gAgent.mRRInterface.mContainsShowinv = contained;
	else if (var == "unsit")				gAgent.mRRInterface.mContainsUnsit = contained;
	else if (var == "fartouch")				gAgent.mRRInterface.mContainsFartouch = contained;
	else if (var == "touchfar")				gAgent.mRRInterface.mContainsFartouch = contained;
	else if (var == "showworldmap")			gAgent.mRRInterface.mContainsShowworldmap = contained;
	else if (var == "showminimap")			gAgent.mRRInterface.mContainsShowminimap = contained;
	else if (var == "showloc")				gAgent.mRRInterface.mContainsShowloc = contained;
	else if (var == "shownames")			gAgent.mRRInterface.mContainsShownames = contained;
	else if (var == "setenv")				gAgent.mRRInterface.mContainsSetenv = contained;
	else if (var == "setdebug")				gAgent.mRRInterface.mContainsSetdebug = contained;
	else if (var == "fly")					gAgent.mRRInterface.mContainsFly = contained;
	else if (var == "edit")					gAgent.mRRInterface.mContainsEdit = (gAgent.mRRInterface.containsWithoutException ("edit")); // || gAgent.mRRInterface.containsSubstr ("editobj"));
	else if (var == "rez")					gAgent.mRRInterface.mContainsRez = contained;
	else if (var == "showhovertextall")		gAgent.mRRInterface.mContainsShowhovertextall = contained;
	else if (var == "showhovertexthud")		gAgent.mRRInterface.mContainsShowhovertexthud = contained;
	else if (var == "showhovertextworld")	gAgent.mRRInterface.mContainsShowhovertextworld = contained;
	else if (var == "defaultwear")			gAgent.mRRInterface.mContainsDefaultwear = contained;
	else if (var == "permissive")			gAgent.mRRInterface.mContainsPermissive = contained;

	gAgent.mRRInterface.refreshTPflag(true);
}

std::string getFirstName (std::string fullName)
{
	int ind = fullName.find (" ");
	if (ind != -1) return fullName.substr (0, ind);
	else return fullName;
}

std::string getLastName (std::string fullName)
{
	int ind = fullName.find (" ");
	if (ind != -1) return fullName.substr (ind+1);
	else return fullName;
}

void updateAllHudTexts ()
{
	LLHUDText::TextObjectIterator text_it;
	
	for (text_it = LLHUDText::sTextObjects.begin(); 
		text_it != LLHUDText::sTextObjects.end(); 
		++text_it)
	{
		LLHUDText *hudText = *text_it;
		if (hudText && hudText->mLastMessageText != "") {
			// do not update the floating names of the avatars around
			LLViewerObject* obj = hudText->getSourceObject();
			if (obj && !obj->isAvatar()) {
				hudText->setStringUTF8(hudText->mLastMessageText);
			}
		}
	}
}

void updateOneHudText (LLUUID uuid)
{
	LLViewerObject* obj = gObjectList.findObject(uuid);
	if (obj) {
		if (obj->mText.notNull()) {
			LLHUDText *hudText = obj->mText.get();
			if (hudText && hudText->mLastMessageText != "") {
				hudText->setStringUTF8(hudText->mLastMessageText);
			}
		}
	}
}
// --





RRInterface::RRInterface():
	mInventoryFetched(FALSE),
	mAllowCancelTp(TRUE),
	mSitTargetId(),
	mLastLoadedPreset(),
	mReattaching(FALSE),
	mReattachTimeout(FALSE),
	mSnappingBackToLastStandingLocation(FALSE)
{
	mAllowedS32 = ",";

	mAllowedU32 = 
	",AvatarSex"			// 0 female, 1 male
	",RenderResolutionDivisor"	// simulate blur, default is 1
	",";

	mAllowedF32 = ",";
	mAllowedBOOLEAN = ",";
	mAllowedSTRING = ",";
	mAllowedVEC3 = ",";
	mAllowedVEC3D = ",";
	mAllowedRECT = ",";
	mAllowedCOL4 = ",";
	mAllowedCOL3 = ",";
	mAllowedCOL4U = ",";

	mAssetsToReattach.clear();

	mJustDetached.uuid.setNull();
	mJustDetached.attachpt = "";
	mJustReattached.uuid.setNull();
	mJustReattached.attachpt = "";

	// Calling gSavedSettings here crashes the viewer when compiled with VS2005.
	// OK under Linux. Moved this initialization to llstartup.cpp as a consequence.
	// sRestrainedLoveDebug = gSavedSettings.getBOOL("RestrainedLoveDebug");
}

RRInterface::~RRInterface()
{
}

void RRInterface::refreshTPflag(bool save)
{
	static BOOL last_value = gSavedPerAccountSettings.getBOOL("RestrainedLoveTPOK");
	BOOL new_value = TRUE;
	if (gAgent.mRRInterface.mContainsUnsit || gAgent.mRRInterface.contains("tplm") || gAgent.mRRInterface.contains("tploc")) {
		new_value = FALSE;
	}
	if (new_value != last_value) {
		last_value = new_value;
		gSavedPerAccountSettings.setBOOL("RestrainedLoveTPOK", new_value);
		if (save) {
			gSavedPerAccountSettings.saveToFile(gSavedSettings.getString("PerAccountSettingsFile"), TRUE);
		}
	}
}

std::string RRInterface::getVersion ()
{
	return RR_VIEWER_NAME" viewer v"RR_VERSION" ("RR_SLV_VERSION")"; // there is no '+' between the string and the macro
}

std::string RRInterface::getVersion2 ()
{
	return RR_VIEWER_NAME_NEW" viewer v"RR_VERSION" ("RR_SLV_VERSION")"; // there is no '+' between the string and the macro
}

BOOL RRInterface::isAllowed (LLUUID object_uuid, std::string action, BOOL log_it)
{
	BOOL debug = sRestrainedLoveDebug && log_it;
	if (debug) {
		llinfos << object_uuid.asString() << "      " << action << llendl;
	}
	RRMAP::iterator it = mSpecialObjectBehaviours.find (object_uuid.asString());
	while (it != mSpecialObjectBehaviours.end() &&
			it != mSpecialObjectBehaviours.upper_bound(object_uuid.asString()))
	{
		if (debug) {
			llinfos << "  checking " << it->second << llendl;
		}
		if (it->second == action) {
			if (debug) {
				llinfos << "  => forbidden. " << llendl;
			}
			return FALSE;
		}
		it++;
	}
	if (debug) {
		llinfos << "  => allowed. " << llendl;
	}
	return TRUE;
}

BOOL RRInterface::contains (std::string action)
{
	RRMAP::iterator it = mSpecialObjectBehaviours.begin ();
	LLStringUtil::toLower(action);
//	llinfos << "looking for " << action << llendl;
	while (it != mSpecialObjectBehaviours.end()) {
		if (it->second == action) {
//			llinfos << "found " << it->second << llendl;
			return TRUE;
		}
		it++;
	}
	return FALSE;
}

BOOL RRInterface::containsSubstr (std::string action)
{
	RRMAP::iterator it = mSpecialObjectBehaviours.begin ();
	LLStringUtil::toLower(action);
//	llinfos << "looking for " << action << llendl;
	while (it != mSpecialObjectBehaviours.end()) {
		if (it->second.find (action) != -1) {
//			llinfos << "found " << it->second << llendl;
			return TRUE;
		}
		it++;
	}
	return FALSE;
}

BOOL RRInterface::containsWithoutException (std::string action, std::string except /* = "" */)
{
	// action is a restriction like @sendim, which can accept exceptions (@sendim:except_uuid=add)
	// action_sec is the same action, with "_sec" appended (like @sendim_sec)
	
	LLStringUtil::toLower(action);
	std::string action_sec = action + "_sec";
	LLUUID uuid;
	
	// 1. If except is empty, behave like contains(), but looking for both action and action_sec
	if (except == "") {
		return (contains (action) || contains (action_sec));
	}

	// 2. For each action_sec, if we don't find an exception tied to the same object, return TRUE
	// if @permissive is set, then even action needs the exception to be tied to the same object, not just action_sec
	// (@permissive restrains the scope of all the exceptions to their own objects)
	RRMAP::iterator it = mSpecialObjectBehaviours.begin ();
	while (it != mSpecialObjectBehaviours.end()) {
		if ((it->second == action_sec || it->second == action) && mContainsPermissive) {
			uuid.set (it->first);
			if (isAllowed (uuid, action + ":" + except, FALSE) && isAllowed (uuid, action_sec + ":" + except, FALSE)) { // we use isAllowed because we need to check the object, but it really means "does not contain"
				return TRUE;
			}
		}
		it++;
	}
	
	// 3. If we didn't return yet, but the map contains action, just look for except_uuid without regard to its object, if none is found return TRUE
	if (contains (action)) {
		if (!contains (action + ":" + except) && !contains (action_sec + ":" + except)) {
			return TRUE;
		}
	}
	
	// 4. Finally return FALSE if we didn't find anything
	return FALSE;
}

FolderLock RRInterface::isFolderLockedWithoutException (LLInventoryCategory* cat, std::string attach_or_detach)
{
	if (cat == NULL) return FolderLock_unlocked;

	if (sRestrainedLoveDebug) {
		llinfos << "isFolderLockedWithoutException(" << cat->getName() << ", " << attach_or_detach << ")" << llendl;
	}
	// For each object that is locking this folder, check whether it also issues exceptions to this lock
	std::deque<std::string> commands_list;
	std::string command;
	std::string behav;
	std::string option;
	std::string param;
	std::string this_command;
	std::string this_behav;
	std::string this_option;
	std::string this_param;
	bool this_object_locks;
	FolderLock current_lock = FolderLock_unlocked;
	for (RRMAP::iterator it = mSpecialObjectBehaviours.begin (); it != mSpecialObjectBehaviours.end(); ++it) {
		LLUUID uuid = LLUUID(it->first);
		command = it->second;
		if (sRestrainedLoveDebug) {
			llinfos << "command = " << command << llendl;
		}
		// param will always be equal to "n" in this case since we added it to command, but we don't care about this here
		if (parseCommand (command+"=n", behav, option, param)) // detach=n, recvchat=n, recvim=n, unsit=n, recvim:<uuid>=add, clear=tplure:
		{
			// find whether this object has issued a "{attach/detach}[all]this" command on a folder that is either this one, or a parent
			this_object_locks = false;
			if (behav == attach_or_detach+"this") {
				if (getCategoryUnderRlvShare(option) == cat) {
					this_object_locks = true;
				}
			}
			else if (behav == attach_or_detach+"allthis") {
				if (isUnderFolder(getCategoryUnderRlvShare(option), cat)) {
					this_object_locks = true;
				}
			}

			// This object has issued such a command, check whether it has issued an exception to it as well
			if (this_object_locks) {
				commands_list = getListOfRestrictions(uuid);
				FolderLock this_lock = isFolderLockedWithoutExceptionAux(cat, attach_or_detach, commands_list);
				if (this_lock == FolderLock_locked_without_except) return FolderLock_locked_without_except;
				else current_lock = this_lock;
				if (sRestrainedLoveDebug) {
					llinfos << "this_lock=" << this_lock << llendl;
				}
				//for (unsigned int i = 0; i < commands_list.size(); ++i)
				//{
				//	this_command = commands_list[i];
				//	// this_param will always be equal to "n" in this case since we added it to this_command, but we don't care about this here
				//	if (parseCommand (this_command+"=n", this_behav, this_option, this_param)) // detach=n, recvchat=n, recvim=n, unsit=n, recvim:<uuid>=add, clear=tplure:
				//	{
				//		if (this_behav == attach_or_detach+"this_except") {
				//			if (getCategoryUnderRlvShare(this_option) == cat) {
				//				return FolderLock_locked_with_except;
				//			}
				//		}
				//		else if (this_behav == attach_or_detach+"allthis_except") {
				//			if (isUnderFolder(getCategoryUnderRlvShare(this_option), cat)) {
				//				return FolderLock_locked_with_except;
				//			}
				//		}
				//	}
				//}
				// Return locked since the folder is locked and we didn't find any exception
				//return FolderLock_locked_without_except;
			}
		}
	}

	// Finally, return unlocked since we didn't find any lock on this folder
//	return FolderLock_unlocked;
	return current_lock;
}

FolderLock RRInterface::isFolderLockedWithoutExceptionAux (LLInventoryCategory* cat, std::string attach_or_detach, std::deque<std::string> list_of_restrictions)
{
	// list_of_restrictions contains the list of restrictions issued by one particular object, at least one is supposed to be a "{attach|detach}[all]this"
	// For each folder from cat up to the root folder, check :
	// - if we are on cat and we find "{attach|detach}this_except", there is an exception, keep looking up
	// - if we are on cat and we find "{attach|detach}this", there is no exception, return FolderLock_locked_without_except
	// - if we are on a parent and we find "{attach|detach}allthis_except", there is an exception, keep looking up
	// - if we are on a parent and we find "{attach|detach}allthis", if we found an exception return FolderLock_locked_with_except, else return FolderLock_locked_without_except
	// - finally, if we are on the root, return FolderLocked_unlocked (whether there was an exception or not)
	if (cat == NULL) {
		return FolderLock_unlocked;
	}

	if (sRestrainedLoveDebug) {
		llinfos << "isFolderLockedWithoutExceptionAux(" << cat->getName() << ", " << attach_or_detach << ", [" << dumpList2String(list_of_restrictions, ",") << "])" << llendl;
	}

	FolderLock current_lock = FolderLock_unlocked;
	std::string command;
	std::string behav;
	std::string option;
	std::string param;

	LLInventoryCategory* it = NULL;
	LLInventoryCategory* cat_option = NULL;
	LLUUID root_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_CATEGORY);

	const LLUUID& cat_id = cat->getUUID();
	it = gInventory.getCategory (cat_id);
	
	do {
		if (sRestrainedLoveDebug) {
			llinfos << "it=" << it->getName() << llendl;
		}

		for (unsigned int i = 0; i < list_of_restrictions.size(); ++i)
		{
			command = list_of_restrictions[i];
			if (sRestrainedLoveDebug) {
				llinfos << "command2=" << command << llendl;
			}

			// param will always be equal to "n" in this case since we added it to command, but we don't care about this here
			if (parseCommand (command+"=n", behav, option, param)) // detach=n, recvchat=n, recvim=n, unsit=n, recvim:<uuid>=add, clear=tplure:
			{
				cat_option = getCategoryUnderRlvShare(option);
				if (cat_option == it) {
					if (it == cat) {
						if (behav == attach_or_detach+"this_except" || behav == attach_or_detach+"allthis_except") {
							current_lock = FolderLock_locked_with_except;
						}
						else if (behav == attach_or_detach+"this"|| behav == attach_or_detach+"allthis") {
							return FolderLock_locked_without_except;
						}
					}
					else {
						if (behav == attach_or_detach+"allthis_except") {
							current_lock = FolderLock_locked_with_except;
						}
						else if (behav == attach_or_detach+"allthis") {
							if (current_lock == FolderLock_locked_with_except) return FolderLock_locked_with_except;
							else return FolderLock_locked_without_except;
						}
					}
				}
			}
		}

		const LLUUID& parent_id = it->getParentUUID();
		it = gInventory.getCategory (parent_id);
	} while (it && it->getUUID() != root_id);
	return FolderLock_unlocked; // this should never happen since list_of_commands is supposed to contain at least one "{attach|detach}[all]this" restriction
}

BOOL RRInterface::add (LLUUID object_uuid, std::string action, std::string option)
{
	if (sRestrainedLoveDebug) {
		llinfos << object_uuid.asString() << "       " << action << "      " << option << llendl;
	}

	std::string canon_action = action;
	if (!option.empty()) action += ":" + option;

	if (isAllowed (object_uuid, action)) {
		// Notify if needed
		notify (object_uuid, action, "=n");

		if (gSavedSettings.controlExists("RestrainedLoveBlacklist")) {
			// Check the action against the blacklist
			std::string blacklist = "," + gSavedSettings.getString("RestrainedLoveBlacklist") + ",";
			if (blacklist.find("," + canon_action + ",") != std::string::npos)
			{
				llinfos << "Blacklisted RestrainedLove command: " << action << " for object " << object_uuid.asString() << llendl;
				return TRUE;
			}
		}

		// Actions to do BEFORE inserting the new behav
		if (action=="showinv") {
			//LLInventoryView::cleanup ();
			for (int i=0; i<LLInventoryView::sActiveViews.count(); ++i) {
				if (LLInventoryView::sActiveViews.get(i)->getVisible()) {
					LLInventoryView::sActiveViews.get(i)->setVisible (false);
				}
			}
		}
		else if (action=="showminimap") {
			gFloaterMap->setVisible(FALSE);
		}
		else if (action=="shownames") {
			LLFloaterChat::getInstance()->childSetVisible("active_speakers_panel", FALSE);
		}
		else if (action=="fly") {
 			gAgent.setFlying (FALSE);
   		}
		else if (action=="showworldmap" || action == "showloc") {
			if (gFloaterWorldMap->getVisible()) {
				LLFloaterWorldMap::toggle(NULL);
			}
		}
		else if (action=="edit") {
			LLPipeline::setRenderBeacons(FALSE);
			LLPipeline::setRenderScriptedBeacons(FALSE);
			LLPipeline::setRenderScriptedTouchBeacons(FALSE);
			LLPipeline::setRenderPhysicalBeacons(FALSE);
			LLPipeline::setRenderSoundBeacons(FALSE);
			LLPipeline::setRenderParticleBeacons(FALSE);
			LLPipeline::setRenderHighlights(FALSE);
			LLDrawPoolAlpha::sShowDebugAlpha = FALSE;
		}
		else if (action=="setenv") {
			if (sRRNoSetEnv) {
				return TRUE;
			}
			LLFloaterEnvSettings::instance()->close();
			LLFloaterWater::instance()->close();
			LLFloaterPostProcess::instance()->close();
			LLFloaterDayCycle::instance()->close();
			LLFloaterWindLight::instance()->close();
			gSavedSettings.setBOOL("VertexShaderEnable", TRUE);
			gSavedSettings.setBOOL("WindLightUseAtmosShaders", TRUE);
		}
		else if (action=="setdebug") {
			if (!sRRNoSetEnv) {
				gSavedSettings.setBOOL("VertexShaderEnable", TRUE);
				gSavedSettings.setBOOL("WindLightUseAtmosShaders", TRUE);
			}
		}

		// Insert the new behav
		mSpecialObjectBehaviours.insert(std::pair<std::string, std::string>(object_uuid.asString(), action));
		refreshCachedVariable(action);
		HBFloaterRLV::setDirty(NULL);

		// Actions to do AFTER inserting the new behav
		if (action=="showhovertextall" || action=="showloc" || action=="shownames"
			|| action=="showhovertexthud" || action=="showhovertextworld" ) {
			updateAllHudTexts();
		}
		if (canon_action == "showhovertext") {
			updateOneHudText(LLUUID(option));
		}

		// Update the stored last standing location, to allow grabbers to transport a victim inside a cage while sitting, and restrict them
		// before standing up. If we didn't do this, the avatar would snap back to a safe location when being unsitted by the grabber,
		// which would be rather silly.
		if (action == "standtp") {
			gAgent.mRRInterface.mLastStandingLocation = LLVector3d(gAgent.getPositionGlobal());
		}

		return TRUE;
	}
	return FALSE;
}

BOOL RRInterface::remove (LLUUID object_uuid, std::string action, std::string option)
{
	if (sRestrainedLoveDebug) {
		llinfos << object_uuid.asString() << "       " << action << "      " << option << llendl;
	}

	std::string canon_action = action;
	if (option!="") action += ":" + option;
	
	// Notify if needed
	notify (object_uuid, action, "=y");
	
	// Actions to do BEFORE removing the behav

	// Remove the behav
	RRMAP::iterator it = mSpecialObjectBehaviours.find (object_uuid.asString());
	while (it != mSpecialObjectBehaviours.end() &&
			it != mSpecialObjectBehaviours.upper_bound(object_uuid.asString()))
	{
		if (sRestrainedLoveDebug) {
			llinfos << "  checking " << it->second << llendl;
		}
		if (it->second == action) {
			mSpecialObjectBehaviours.erase(it);
			if (sRestrainedLoveDebug) {
				llinfos << "  => removed. " << llendl;
			}
			refreshCachedVariable(action);
			HBFloaterRLV::setDirty(NULL);

			// Actions to do AFTER removing the behav
			if (action=="showhovertextall" || action=="showloc" || action=="shownames"
				|| action=="showhovertexthud" || action=="showhovertextworld" ) {
				updateAllHudTexts();
			}
			if (canon_action == "showhovertext") {
				updateOneHudText(LLUUID(option));
			}

			return TRUE;
		}
		it++;
	}

	return FALSE;
}

BOOL RRInterface::clear (LLUUID object_uuid, std::string command)
{
	if (sRestrainedLoveDebug) {
		llinfos << object_uuid.asString() << "   /   " << command << llendl;
	}

	// Notify if needed
	notify (object_uuid, "clear" + (command!="" ? ":" + command : ""), "");
	
	RRMAP::iterator it;
	it = mSpecialObjectBehaviours.begin ();
	while (it != mSpecialObjectBehaviours.end()) {
		if (sRestrainedLoveDebug) {
			llinfos << "  checking " << it->second << llendl;
		}
		if (it->first==object_uuid.asString() && (command=="" || it->second.find (command)!=-1)) {
			if (sRestrainedLoveDebug) {
				llinfos << it->second << " => removed. " << llendl;
			}
			std::string tmp = it->second;
			mSpecialObjectBehaviours.erase(it);
			refreshCachedVariable(tmp);
			it = mSpecialObjectBehaviours.begin ();
		}
		else {
			it++;
		}
	}
	HBFloaterRLV::setDirty(NULL);
	updateAllHudTexts();
	return TRUE;
}

void RRInterface::replace (LLUUID what, LLUUID by)
{
	RRMAP::iterator it;
	LLUUID uuid;
	it = mSpecialObjectBehaviours.begin ();
	while (it != mSpecialObjectBehaviours.end()) {
		uuid.set (it->first);
		if (uuid == what) {
			// found the UUID to replace => add a copy of the command with the new UUID
			mSpecialObjectBehaviours.insert(std::pair<std::string, std::string>(by.asString(), it->second));
		}
		it++;
	}
	// and then clear the old UUID
	clear (what, "");
	HBFloaterRLV::setDirty(NULL);
}


BOOL RRInterface::garbageCollector (BOOL all) {
	RRMAP::iterator it;
	BOOL res=FALSE;
	LLUUID uuid;
	LLViewerObject *objp=NULL;
	it = mSpecialObjectBehaviours.begin ();
	while (it != mSpecialObjectBehaviours.end()) {
		uuid.set (it->first);
		if (all || !uuid.isNull ()) {
//			if (sRestrainedLoveDebug) {
//				llinfos << "testing " << it->first << llendl;
//			}
			objp = gObjectList.findObject(uuid);
			if (!objp) {
				if (sRestrainedLoveDebug) {
					llinfos << it->first << " not found => cleaning... " << llendl;
				}
				clear (uuid);
				res=TRUE;
				it=mSpecialObjectBehaviours.begin ();
			} else {
				it++;
			}
		} else {
			if (sRestrainedLoveDebug) {
				llinfos << "ignoring " << it->second << llendl;
			}
			it++;
		}
    }
    return res;
}

std::deque<std::string> RRInterface::parse (std::string str, std::string sep)
{
	int ind;
	int length = sep.length();
	std::deque<std::string> res;
	
	do {
		ind=str.find(sep);
		if (ind!=-1) {
			res.push_back (str.substr (0, ind));
			str=str.substr (ind+length);
		}
		else {
			res.push_back (str);
		}
	} while (ind!=-1);
	
	return res;
}


void RRInterface::notify (LLUUID object_uuid, std::string action, std::string suffix)
{
	// scan the list of restrictions, when finding "notify" say the restriction on the specified channel
	RRMAP::iterator it;
	int length = 7; // size of "notify:"
	int size;
	std::deque<std::string> tokens;
	LLUUID uuid;
	std::string rule;
	it = mSpecialObjectBehaviours.begin ();
	
	while (it != mSpecialObjectBehaviours.end()) {
		uuid.set (it->first);
		rule = it->second; // we are looking for rules like "notify:2222;tp", if action contains "tp" then notify the scripts on channel 2222
		if (rule.find("notify:") == 0) {
			// found a possible notification to send
			rule = rule.substr(length); // keep right part only (here "2222;tp")
			tokens = parse (rule, ";");
			size = tokens.size();
			if (size == 1 || (size > 1 && action.find(tokens[1]) != -1)) {
				answerOnChat(tokens[0], "/" + action + suffix); // suffix can be "=n", "=y" or whatever else we want, "/" is needed to avoid some clever griefing
			}
		}
		it++;
	}
}


BOOL RRInterface::parseCommand (std::string command, std::string& behaviour, std::string& option, std::string& param)
{
	int ind = command.find("=");
	behaviour=command;
	option="";
	param="";
	if (ind!=-1) {
		behaviour=command.substr(0, ind);
		param=command.substr(ind+1);
		ind=behaviour.find(":");
		if (ind!=-1) {
			option=behaviour.substr(ind+1);
			behaviour=behaviour.substr(0, ind); // keep in this order (option first, then behav) or crash
		}
		return TRUE;
	}
	return FALSE;
}

BOOL RRInterface::handleCommand (LLUUID uuid, std::string command)
{
	// 1. check the command is actually a single one or a list of commands separated by ","
	if (command.find (",")!=-1) {
		BOOL res=TRUE;
		std::deque<std::string> list_of_commands=parse (command, ",");
		for (unsigned int i=0; i<list_of_commands.size (); ++i) {
			if (!handleCommand (uuid, list_of_commands.at(i))) res=FALSE;
		}
		return res;
	}
	
	// 2. this is a single command, possibly inside a 1-level recursive call (unimportant)
	// if the viewer is not fully initialized and the user does not have control of their avatar,
	// don't execute the command but retain it for later, when it is fully initialized
	// If there is another object still waiting to be automatically reattached, retain all RLV commands
	// as well to avoid an infinite loop if the one it will kick off is also locked.
	// This is valid as the object that would possibly be kicked off by the one to reattach, will have
	// its restrictions wiped out by the garbage collector
	if (LLStartUp::getStartupState() != STATE_STARTED
		|| (!mAssetsToReattach.empty() && !mReattachTimeout)) {
		Command cmd;
		cmd.uuid=uuid;
		cmd.command=command;
		if (sRestrainedLoveDebug) {
			llinfos << "Retaining command : " << command << llendl;
		}
		mRetainedCommands.push_back (cmd);
		return TRUE;
	}
	
	// 3. parse the command, which is of one of these forms :
	// behav=param
	// behav:option=param
	std::string behav;
	std::string option;
	std::string param;
	LLStringUtil::toLower(command);
	if (parseCommand (command, behav, option, param)) // detach=n, recvchat=n, recvim=n, unsit=n, recvim:<uuid>=add, clear=tplure:
	{
		if (sRestrainedLoveDebug) {
			llinfos << "[" << uuid.asString() << "]  [" << behav << "]  [" << option << "] [" << param << "]" << llendl;
		}
		if (behav=="version") return answerOnChat (param, getVersion ());
		else if (behav=="versionnew") return answerOnChat (param, getVersion2 ());
		else if (behav=="versionnum") return answerOnChat (param, RR_VERSION_NUM);
		else if (behav=="getoutfit") return answerOnChat (param, getOutfit (option));
		else if (behav=="getattach") return answerOnChat (param, getAttachments (option));
		else if (behav=="getstatus") return answerOnChat (param, getStatus (uuid, option));
		else if (behav=="getstatusall") {
			uuid.setNull();
			return answerOnChat (param, getStatus (uuid, option));
		}
		else if (behav=="getinv") return answerOnChat (param, getInventoryList (option));
		else if (behav=="getinvworn") return answerOnChat (param, getInventoryList (option, TRUE));
		else if (behav=="getsitid") return answerOnChat (param, getSitTargetId ().asString());
		else if (behav=="getpath") return answerOnChat (param, getFullPath (getItem(uuid), option, false)); // option can be empty (=> find path to object) or the name of an attach pt or the name of a clothing layer
		else if (behav=="getpathnew") return answerOnChat (param, getFullPath (getItem(uuid), option)); // option can be empty (=> find path to object) or the name of an attach pt or the name of a clothing layer
		else if (behav=="findfolder") return answerOnChat (param, getFullPath (findCategoryUnderRlvShare (option)));
		else if (behav.find ("getenv_") == 0) return answerOnChat (param, getEnvironment (behav));
		else if (behav.find ("getdebug_") == 0) return answerOnChat (param, getDebugSetting (behav));
		else if (behav=="getgroup") {
			LLUUID group_id = gAgent.getGroupID();
			std::string group_name = "none";
			if (group_id.notNull()) {
				gCacheName->getGroupName(group_id, group_name);
			}
			return answerOnChat (param, group_name);
		}
		else {
			if (param=="n" || param=="add") add (uuid, behav, option);
			else if (param=="y" || param=="rem") remove (uuid, behav, option);
			else if (behav=="clear") clear (uuid, param);
			else if (param=="force") force (uuid, behav, option);
			else return FALSE;
		}
	}
	else // clear
	{
		if (sRestrainedLoveDebug) {
			llinfos << uuid.asString() << "       " << behav << llendl;
		}
		if (behav=="clear") clear (uuid);
		else return FALSE;
	}
	return TRUE;
}

BOOL RRInterface::fireCommands ()
{
	BOOL ok=TRUE;
	if (mRetainedCommands.size ()) {
		if (sRestrainedLoveDebug) {
			llinfos << "Firing commands : " << mRetainedCommands.size () << llendl;
		}
		Command cmd;
		while (!mRetainedCommands.empty ()) {
			cmd=mRetainedCommands[0];
			ok=ok & handleCommand (cmd.uuid, cmd.command);
			mRetainedCommands.pop_front ();
		}
	}
	return ok;
}

static void force_sit(LLUUID object_uuid)
{
	// Note : Although it would make sense that only the UUID should be needed, we actually need to send an
	// offset to the sim, therefore we need the object to be known to the viewer. In other words, issuing @sit=force
	// right after a teleport is not going to work because the object will not have had time to rez.
	LLViewerObject *object = gObjectList.findObject(object_uuid);
	if (object) {
		LLVOAvatar* avatar = gAgent.getAvatarObject();
		if (avatar && gAgent.mRRInterface.mContainsUnsit && avatar->mIsSitting) {
			// Do not allow a script to force the avatar to sit somewhere if already forced to stay sitting here
			return;
		}
		if (gAgent.mRRInterface.contains ("sit"))
		{
			return;
		}
		if (gAgent.mRRInterface.contains ("sittp")) {
			// Do not allow a script to force the avatar to sit somewhere far when under @sittp
			LLVector3 pos = object->getPositionRegion();
			pos -= gAgent.getPositionAgent ();
			if (pos.magVec () >= 1.5)
			{
				return;
			}
		}
		if (gAgent.getAvatarObject() && !gAgent.getAvatarObject()->mIsSitting)
		{
			// We are now standing, and we want to sit down => store our current location so that we can snap back here when we stand up, if under @standtp
			gAgent.mRRInterface.mLastStandingLocation = LLVector3d(gAgent.getPositionGlobal());
		}
		gMessageSystem->newMessageFast(_PREHASH_AgentRequestSit);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_TargetObject);
		gMessageSystem->addUUIDFast(_PREHASH_TargetID, object->mID);
		gMessageSystem->addVector3Fast(_PREHASH_Offset,
			gAgent.calcFocusOffset(object, gAgent.getPositionAgent(), (S32)0.0f, (S32)0.0f));
		object->getRegion()->sendReliableMessage();
	}
}


BOOL RRInterface::force (LLUUID object_uuid, std::string command, std::string option)
{
	if (sRestrainedLoveDebug) {
		llinfos << command << "     " << option << llendl;
	}
	if (command=="sit") { // sit:UUID
		BOOL allowed_to_sittp=TRUE;
		if (!isAllowed (object_uuid, "sittp")) {
			allowed_to_sittp=FALSE;
			remove (object_uuid, "sittp", "");
		}
		LLUUID uuid (option);
		force_sit (uuid);
		if (!allowed_to_sittp) add (object_uuid, "sittp", "");
	}
	else if (command=="unsit") { // unsit
		if (sRestrainedLoveDebug) {
			llinfos << "trying to unsit" << llendl;
		}
		if (gAgent.getAvatarObject() &&
			gAgent.getAvatarObject()->mIsSitting) {
			if (sRestrainedLoveDebug) {
				llinfos << "found avatar object" << llendl;
			}
			if (gAgent.mRRInterface.mContainsUnsit) {
				if (sRestrainedLoveDebug) {
					llinfos << "prevented from unsitting" << llendl;
				}
				return TRUE;
			}
			if (sRestrainedLoveDebug) {
				llinfos << "unsitting agent" << llendl;
			}
			LLOverlayBar::onClickStandUp(NULL);
			send_agent_update(TRUE, TRUE);
		}
	}
	else if (command=="remoutfit") { // remoutfit:shoes
		if (option=="") {
			removeWearableFromAvatar (WT_GLOVES);
			removeWearableFromAvatar (WT_JACKET);
			removeWearableFromAvatar (WT_PANTS);
			removeWearableFromAvatar (WT_SHIRT);
			removeWearableFromAvatar (WT_SHOES);
			removeWearableFromAvatar (WT_SKIRT);
			removeWearableFromAvatar (WT_SOCKS);
			removeWearableFromAvatar (WT_UNDERPANTS);
			removeWearableFromAvatar (WT_UNDERSHIRT);
			removeWearableFromAvatar (WT_ALPHA);
			removeWearableFromAvatar (WT_TATTOO);
		}
		else {
			EWearableType type = getOutfitLayerAsType (option);
			if (type != WT_INVALID) {
				 // clothes only, not skin, eyes, hair or shape
				if (LLWearable::typeToAssetType(type) == LLAssetType::AT_CLOTHING) {
					removeWearableFromAvatar (type); // remove by layer
				}
			}
			else forceDetachByName (option, FALSE, TRUE); // remove by category (in RLV share)
		}
	}
	else if (command=="detach" || command=="remattach") { // detach:chest=force OR detach:restraints/cuffs=force (@remattach is a synonym)
		LLViewerJointAttachment* attachpt = findAttachmentPointFromName (option, TRUE); // exact name
		if (attachpt != NULL || option == "") return forceDetach (option); // remove by attach pt
		else forceDetachByName (option, FALSE, TRUE);
	}
	else if (command=="detachme") { // detachme=force to detach this object specifically
		return forceDetachByUuid (object_uuid.asString()); // remove by uuid
	}
	else if (command=="detachthis") { // detachthis=force to detach the folder containing this object
		std::string pathes_str = getFullPath (getItem(object_uuid), option);
		std::deque<std::string> pathes = parse (pathes_str, ",");
		BOOL res = TRUE;
		for (unsigned int i = 0; i < pathes.size(); ++i) {
			res &= forceDetachByName (pathes.at(i), FALSE, TRUE);
		}
		return res;
	}
	else if (command=="detachall") { // detachall:cuffs=force to detach a folder and its subfolders
		return forceDetachByName (option, TRUE, TRUE);
	}
	else if (command=="detachallthis") { // detachallthis=force to detach the folder containing this object and also its subfolders
		std::string pathes_str = getFullPath (getItem(object_uuid), option);
		std::deque<std::string> pathes = parse (pathes_str, ",");
		BOOL res = TRUE;
		for (unsigned int i = 0; i < pathes.size(); ++i) {
			res &= forceDetachByName (pathes.at(i), TRUE, TRUE);
		}
		return res;
	}
	else if (command=="tpto") { // tpto:X/Y/Z=force (X, Y, Z are GLOBAL coordinates)
		BOOL allowed_to_tploc=TRUE;
		BOOL allowed_to_unsit=TRUE;
		BOOL allowed_to_sittp=TRUE;
		BOOL res;
		if (!isAllowed (object_uuid, "tploc")) {
			allowed_to_tploc=FALSE;
			remove (object_uuid, "tploc", "");
		}
		if (!isAllowed (object_uuid, "unsit")) {
			allowed_to_unsit=FALSE;
			remove (object_uuid, "unsit", "");
		}
		if (!isAllowed (object_uuid, "sittp")) {
			allowed_to_sittp=FALSE;
			remove (object_uuid, "sittp", "");
		}
		res = forceTeleport (option);
		if (!allowed_to_tploc) add (object_uuid, "tploc", "");
		if (!allowed_to_unsit) add (object_uuid, "unsit", "");
		if (!allowed_to_sittp) add (object_uuid, "sittp", "");
		return res;
	}
	else if (command=="attach" || command == "addoutfit") { // attach:cuffs=force
		return forceAttach (option, FALSE, AttachHow_over_or_replace); // Will have to be changed back to AttachHow_replace eventually, but not before a clear and early communication
	}
	else if (command=="attachover" || command == "addoutfitover") { // attachover:cuffs=force
		return forceAttach (option, FALSE, AttachHow_over);
	}
	else if (command=="attachoverorreplace" || command == "addoutfitoverorreplace") { // attachoverorreplace:cuffs=force
		return forceAttach (option, FALSE, AttachHow_over_or_replace);
	}
	else if (command=="attachthis" || command == "addoutfitthis") { // attachthis=force to attach the folder containing this object
		BOOL res = TRUE;
		std::string pathes_str = getFullPath (getItem(object_uuid), option);
		if (pathes_str != "") {
			std::deque<std::string> pathes = parse (pathes_str, ",");
			for (unsigned int i = 0; i < pathes.size(); ++i) {
				res &= forceAttach (pathes.at(i), FALSE, AttachHow_over_or_replace); // Will have to be changed back to AttachHow_replace eventually, but not before a clear and early communication
			}
		}
		return res;
	}
	else if (command=="attachthisover" || command == "addoutfitthisover") { // attachthisover=force to attach the folder containing this object
		BOOL res = TRUE;
		std::string pathes_str = getFullPath (getItem(object_uuid), option);
		if (pathes_str != "") {
			std::deque<std::string> pathes = parse (pathes_str, ",");
			for (unsigned int i = 0; i < pathes.size(); ++i) {
				res &= forceAttach (pathes.at(i), FALSE, AttachHow_over);
			}
		}
		return res;
	}
	else if (command=="attachthisoverorreplace" || command == "addoutfitthisoverorreplace") { // attachthisoverorreplace=force to attach the folder containing this object
		BOOL res = TRUE;
		std::string pathes_str = getFullPath (getItem(object_uuid), option);
		if (pathes_str != "") {
			std::deque<std::string> pathes = parse (pathes_str, ",");
			for (unsigned int i = 0; i < pathes.size(); ++i) {
				res &= forceAttach (pathes.at(i), FALSE, AttachHow_over_or_replace);
			}
		}
		return res;
	}
	else if (command=="attachall" || command == "addoutfitall") { // attachall:cuffs=force to attach a folder and its subfolders
		return forceAttach (option, TRUE, AttachHow_over_or_replace); // Will have to be changed back to AttachHow_replace eventually, but not before a clear and early communication
	}
	else if (command=="attachallover" || command == "addoutfitallover") { // attachallover:cuffs=force to attach a folder and its subfolders
		return forceAttach (option, TRUE, AttachHow_over);
	}
	else if (command=="attachalloverorreplace" || command == "addoutfitalloverorreplace") { // attachalloverorreplace:cuffs=force to attach a folder and its subfolders
		return forceAttach (option, TRUE, AttachHow_over_or_replace);
	}
	else if (command=="attachallthis" || command == "addoutfitallthis") { // attachallthis=force to attach the folder containing this object and its subfolders
		BOOL res = TRUE;
		std::string pathes_str = getFullPath (getItem(object_uuid), option);
		if (pathes_str != "") {
			std::deque<std::string> pathes = parse (pathes_str, ",");
			for (unsigned int i = 0; i < pathes.size(); ++i) {
				res &= forceAttach (pathes.at(i), TRUE, AttachHow_over_or_replace); // Will have to be changed back to AttachHow_replace eventually, but not before a clear and early communication
			}
		}
		return res;
	}
	else if (command=="attachallthisover" || command == "addoutfitallthisover") { // attachallthisover=force to attach the folder containing this object and its subfolders
		BOOL res = TRUE;
		std::string pathes_str = getFullPath (getItem(object_uuid), option);
		if (pathes_str != "") {
			std::deque<std::string> pathes = parse (pathes_str, ",");
			for (unsigned int i = 0; i < pathes.size(); ++i) {
				res &= forceAttach (pathes.at(i), TRUE, AttachHow_over);
			}
		}
		return res;
	}
	else if (command=="attachallthisoverorreplace" || command == "addoutfitallthisoverorreplace") { // attachallthisoverorreplace=force to attach the folder containing this object and its subfolders
		BOOL res = TRUE;
		std::string pathes_str = getFullPath (getItem(object_uuid), option);
		if (pathes_str != "") {
			std::deque<std::string> pathes = parse (pathes_str, ",");
			for (unsigned int i = 0; i < pathes.size(); ++i) {
				res &= forceAttach (pathes.at(i), TRUE, AttachHow_over_or_replace);
			}
		}
		return res;
	}
	else if (command.find ("setenv_") == 0) {
		BOOL res = TRUE;
		BOOL allowed = TRUE;
		if (!sRRNoSetEnv) {
			if (!isAllowed (object_uuid, "setenv")) {
				allowed=FALSE;
				remove (object_uuid, "setenv", "");
			}
			if (!mContainsSetenv) res = forceEnvironment (command, option);
			if (!allowed) add (object_uuid, "setenv", "");
		}
		return res;
	}
	else if (command.find ("setdebug_") == 0) {
		BOOL res = TRUE;
		BOOL allowed = TRUE;
		if (!isAllowed (object_uuid, "setdebug")) {
			allowed=FALSE;
			remove (object_uuid, "setdebug", "");
		}
		if (!contains("setdebug")) res = forceDebugSetting (command, option);
		if (!allowed) add (object_uuid, "setdebug", "");
		return res;
	}
	else if (command=="setrot") { // setrot:angle_radians=force
		LLVOAvatar* avatar = gAgent.getAvatarObject();
		if (!avatar) return FALSE;
		F32 val = atof (option.c_str());
		gAgent.startCameraAnimation();
		LLVector3 rot (0.0, 1.0, 0.0);
		rot = rot.rotVec(-val, LLVector3::z_axis);
		rot.normalize();
		gAgent.resetAxes(rot);
	}
	else if (command == "adjustheight") { // adjustheight:adjustment_centimeters=force or adjustheight:ref_pelvis_to_foot;scalar[;delta]=force
		if (!gSavedSettings.controlExists(AVATARHEIGHTOFFSET)) {
			return FALSE;
		}
		LLVOAvatar* avatar = gAgent.getAvatarObject();
		if (avatar) {
			F32 val = (F32)atoi(option.c_str()) / 100.0f;
			size_t i = option.find(";");
			if (i != std::string::npos && i + 1 < option.length()) {
				F32 scalar = (F32)atof(option.substr(i + 1).c_str());
				if (scalar != 0.0f) {
					if (sRestrainedLoveDebug) {
						llinfos << "Pelvis to foot = " << avatar->getPelvisToFoot() << "m" << llendl;
					}
					val = (atof(option.c_str()) - avatar->getPelvisToFoot()) * scalar;
					option = option.substr(i + 1);
					i = option.find(";");
					if (i != std::string::npos && i + 1 < option.length()) {
						val += (F32)atof(option.substr(i + 1).c_str());
					}
				}
			}
			if (val > 1.0f) {
				val = 1.0f;
			} else if (val < -1.0f) {
				val = -1.0f;
			}
			gSavedSettings.setF32(AVATARHEIGHTOFFSET, val);
		}
	}
	else if (command == "setgroup") {
		std::string target_group_name = option;
		LLStringUtil::toLower(target_group_name);
		if (target_group_name != "none") { // "none" is not localized here because a script should not have to bother about viewer language
			S32 nb = gAgent.mGroups.count();
			for (S32 i=0; i<nb; ++i) {
				LLGroupData group = gAgent.mGroups.get(i);
				std::string this_group_name = group.mName;
				LLStringUtil::toLower(this_group_name);
				if (this_group_name == target_group_name) {
					LLMessageSystem* msg = gMessageSystem;
					msg->newMessageFast(_PREHASH_ActivateGroup);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
					msg->addUUIDFast(_PREHASH_GroupID, group.mID);
					gAgent.sendReliableMessage();
					break;
				}
			}
		}
		else {
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessageFast(_PREHASH_ActivateGroup);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->addUUIDFast(_PREHASH_GroupID, LLUUID::null);
			gAgent.sendReliableMessage();
		}
		// not found => do nothing
	}
	return TRUE;
}

void RRInterface::removeWearableFromAvatar (EWearableType type)
{
	LLViewerInventoryItem* item = gInventory.getItem(gAgent.getWearableItem(type));
	if (!item) return;
	std::string name = item->getName();
	LLStringUtil::toLower(name);
	if (name.find ("nostrip") != -1) return;
	gAgent.removeWearable (type);
}

BOOL RRInterface::answerOnChat (std::string channel, std::string msg)
{
	S32 chan = (S32)atoi(channel.c_str());
	if (chan == 0) {
		// protection against abusive "@getstatus=0" commands, or against a non-numerical channel
		return FALSE;
	}
	if (msg.length() > (size_t)(chan > 0 ? 1023 : 255)) {
		llwarns << "Too large an answer: maximum is " << (chan > 0 ? "1023 characters" : "255 characters for a negative channel") << ". Truncated reply." << llendl;
		msg = msg.substr(0, (size_t)(chan > 0 ? 1022 : 254));
	}
	if (chan > 0) {
		std::ostringstream temp;
		temp << "/" << chan << " " << msg;
		gChatBar->sendChatFromViewer(temp.str(), CHAT_TYPE_SHOUT, FALSE);
	} else {
		gMessageSystem->newMessage("ScriptDialogReply");
		gMessageSystem->nextBlock("AgentData");
		gMessageSystem->addUUID("AgentID", gAgent.getID());
		gMessageSystem->addUUID("SessionID", gAgent.getSessionID());
		gMessageSystem->nextBlock("Data");
		gMessageSystem->addUUID("ObjectID", gAgent.getID());
		gMessageSystem->addS32("ChatChannel", chan);
		gMessageSystem->addS32("ButtonIndex", 1);
		gMessageSystem->addString("ButtonLabel", msg);
		gAgent.sendReliableMessage();
	}
	if (sRestrainedLoveDebug) {
		llinfos << "/" << chan << " " << msg << llendl;
	}
	return TRUE;
}

std::string RRInterface::crunchEmote (std::string msg, unsigned int truncateTo) {
	std::string crunched = msg;

	if (msg.find ("/me ") == 0 || msg.find ("/me'") == 0) {
		// Only allow emotes without "spoken" text.
		// Forbid text containing any symbol which could be used as quotes.
		if (msg.find ("\"") != -1 || msg.find ("''") != -1
		    || msg.find ("(")  != -1 || msg.find (")")  != -1
		    || msg.find (" -") != -1 || msg.find ("- ") != -1
		    || msg.find ("*")  != -1 || msg.find ("=")  != -1
		    || msg.find ("^")  != -1 || msg.find ("_")  != -1
		    || msg.find ("?")  != -1 || msg.find ("~")  != -1)
		{
			crunched = "...";
		}
		else if (truncateTo > 0 && !contains ("emote")) {
			// Only allow short emotes.
			int i = msg.find (".");
			if (i != -1) {
				crunched = msg.substr (0, ++i);
			}
			if (crunched.length () > truncateTo) {
				crunched = crunched.substr (0, truncateTo);
			}
		}
	}
	else if (msg.find ("/") == 0) {
		// only allow short gesture names (to avoid cheats).
		if (msg.length () > 7) { // allows things like "/ao off", "/hug X"
			crunched = "...";
		}
	}
	else if (msg.find ("((") != 0 || msg.find ("))") != msg.length () - 2) {
		// Only allow OOC chat, starting with "((" and ending with "))".
		crunched = "...";
	}
	return crunched;
}

std::string RRInterface::getOutfitLayerAsString (EWearableType layer)
{
	switch (layer) {
		case WT_SKIN: return WS_SKIN;
		case WT_GLOVES: return WS_GLOVES;
		case WT_JACKET: return WS_JACKET;
		case WT_PANTS: return WS_PANTS;
		case WT_SHIRT: return WS_SHIRT;
		case WT_SHOES: return WS_SHOES;
		case WT_SKIRT: return WS_SKIRT;
		case WT_SOCKS: return WS_SOCKS;
		case WT_UNDERPANTS: return WS_UNDERPANTS;
		case WT_UNDERSHIRT: return WS_UNDERSHIRT;
		case WT_ALPHA: return WS_ALPHA;
		case WT_TATTOO: return WS_TATTOO;
		case WT_EYES: return WS_EYES;
		case WT_HAIR: return WS_HAIR;
		case WT_SHAPE: return WS_SHAPE;
		default: return "";
	}
}

EWearableType RRInterface::getOutfitLayerAsType (std::string layer)
{
	if (layer==WS_SKIN) return WT_SKIN;
	if (layer==WS_GLOVES) return WT_GLOVES;
	if (layer==WS_JACKET) return WT_JACKET;
	if (layer==WS_PANTS) return WT_PANTS;
	if (layer==WS_SHIRT) return WT_SHIRT;
	if (layer==WS_SHOES) return WT_SHOES;
	if (layer==WS_SKIRT) return WT_SKIRT;
	if (layer==WS_SOCKS) return WT_SOCKS;
	if (layer==WS_UNDERPANTS) return WT_UNDERPANTS;
	if (layer==WS_UNDERSHIRT) return WT_UNDERSHIRT;
	if (layer==WS_ALPHA) return WT_ALPHA;
	if (layer==WS_TATTOO) return WT_TATTOO;
	if (layer==WS_EYES) return WT_EYES;
	if (layer==WS_HAIR) return WT_HAIR;
	if (layer==WS_SHAPE) return WT_SHAPE;
	return WT_INVALID;
}

std::string RRInterface::getOutfit (std::string layer)
{
	if (layer==WS_SKIN) return (gAgent.getWearable (WT_SKIN) != NULL ? "1" : "0");
	if (layer==WS_GLOVES) return (gAgent.getWearable (WT_GLOVES) != NULL ? "1" : "0");
	if (layer==WS_JACKET) return (gAgent.getWearable (WT_JACKET) != NULL ? "1" : "0");
	if (layer==WS_PANTS) return (gAgent.getWearable (WT_PANTS) != NULL ? "1" : "0");
	if (layer==WS_SHIRT)return (gAgent.getWearable (WT_SHIRT) != NULL ? "1" : "0");
	if (layer==WS_SHOES) return (gAgent.getWearable (WT_SHOES) != NULL ? "1" : "0");
	if (layer==WS_SKIRT) return (gAgent.getWearable (WT_SKIRT) != NULL ? "1" : "0");
	if (layer==WS_SOCKS) return (gAgent.getWearable (WT_SOCKS) != NULL ? "1" : "0");
	if (layer==WS_UNDERPANTS) return (gAgent.getWearable (WT_UNDERPANTS) != NULL ? "1" : "0");
	if (layer==WS_UNDERSHIRT) return (gAgent.getWearable (WT_UNDERSHIRT) != NULL ? "1" : "0");
	if (layer==WS_ALPHA) return (gAgent.getWearable (WT_ALPHA) != NULL ? "1" : "0");
	if (layer==WS_TATTOO) return (gAgent.getWearable (WT_TATTOO) != NULL ? "1" : "0");
	if (layer==WS_EYES) return (gAgent.getWearable (WT_EYES) != NULL ? "1" : "0");
	if (layer==WS_HAIR) return (gAgent.getWearable (WT_HAIR) != NULL ? "1" : "0");
	if (layer==WS_SHAPE) return (gAgent.getWearable (WT_SHAPE) != NULL ? "1" : "0");
	return getOutfit (WS_GLOVES)+getOutfit (WS_JACKET)+getOutfit (WS_PANTS)
			+getOutfit (WS_SHIRT)+getOutfit (WS_SHOES)+getOutfit (WS_SKIRT)
			+getOutfit (WS_SOCKS)+getOutfit (WS_UNDERPANTS)+getOutfit (WS_UNDERSHIRT)
			+getOutfit (WS_SKIN)+getOutfit (WS_EYES)+getOutfit (WS_HAIR)+getOutfit (WS_SHAPE)
			+getOutfit (WS_ALPHA)+getOutfit (WS_TATTOO)
			;
}

std::string RRInterface::getAttachments (std::string attachpt)
{
	std::string res="";
	std::string name;
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) {
		llwarns << "NULL avatar pointer. Aborting." << llendl;
		return res;
	}
	if (attachpt=="") res += "0"; // to match the LSL macros
	for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin(); 
		iter != avatar->mAttachmentPoints.end(); iter++)
	{
		LLVOAvatar::attachment_map_t::iterator curiter = iter;
		LLViewerJointAttachment* attachment = curiter->second;
		name=attachment->getName ();
		LLStringUtil::toLower(name);
		if (sRestrainedLoveDebug) {
			llinfos << "trying <" << name << ">" << llendl;
		}
		if (attachpt=="" || attachpt==name)
		{
			if (attachment->getNumObjects() > 0) res+="1"; //attachment->getName ();
			else res += "0";
		}
	}
	return res;
}

std::string RRInterface::getStatus (LLUUID object_uuid, std::string rule)
{
	std::string res="";
	std::string name;
	RRMAP::iterator it;
	if (object_uuid.isNull()) {
		it = mSpecialObjectBehaviours.begin();
	}
	else {
		it = mSpecialObjectBehaviours.find (object_uuid.asString());
	}
	bool is_first=true;
	while (it != mSpecialObjectBehaviours.end() &&
			(object_uuid.isNull() || it != mSpecialObjectBehaviours.upper_bound(object_uuid.asString()))
	)
	{
		if (rule=="" || it->second.find (rule)!=-1) {
			//if (!is_first) 
			res += "/";
			res += it->second;
			is_first=false;
		}
		it++;
	}
	return res;
}

BOOL RRInterface::forceDetach (std::string attachpt)
{
	std::string name;
	BOOL res=FALSE;
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) return res;
	for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin(); 
		 iter != avatar->mAttachmentPoints.end(); iter++)
	{
		LLVOAvatar::attachment_map_t::iterator curiter = iter;
		LLViewerJointAttachment* attachment = curiter->second;
		name=attachment->getName ();
		LLStringUtil::toLower(name);
		if (sRestrainedLoveDebug) {
			llinfos << "trying <" << name << ">" << llendl;
		}
		if (attachpt=="" || attachpt==name) {
			if (sRestrainedLoveDebug) {
				llinfos << "found => detaching" << llendl;
			}
			detachAllObjectsFromAttachment (attachment);
			res=TRUE;
		}
	}
	return res;
}

BOOL RRInterface::forceDetachByUuid (std::string object_uuid)
{
	BOOL res=FALSE;
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) return res;
	LLViewerObject* object = gObjectList.findObject(LLUUID (object_uuid));
	if (object) {
		for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin(); 
			 iter != avatar->mAttachmentPoints.end(); iter++)
		{
			LLVOAvatar::attachment_map_t::iterator curiter = iter;
			LLViewerJointAttachment* attachment = curiter->second;
			if (attachment && attachment->isObjectAttached (object)) {
				detachObject (object);
				res=TRUE;
			}
		}
	}
	return res;
}

BOOL RRInterface::hasLockedHuds ()
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) return FALSE;
	for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin(); 
		 iter != avatar->mAttachmentPoints.end(); iter++)
	{
		LLVOAvatar::attachment_map_t::iterator curiter = iter;
		LLViewerJointAttachment* attachment = curiter->second;
		LLViewerObject* obj;
		if (attachment) {
			for (unsigned int i = 0; i < attachment->mAttachedObjects.size(); ++i) {
				obj = attachment->mAttachedObjects.at(i);
				if (obj && obj->isHUDAttachment() && !canDetach(obj)) return TRUE;
			}
		}
	}
	return FALSE;
}


std::deque<LLInventoryItem*> RRInterface::getListOfLockedItems (LLInventoryCategory* root)
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	std::deque<LLInventoryItem*> res;
	std::deque<LLInventoryItem*> tmp;
	res.clear();
	
	if (root && avatar) {
		
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf (root->getUUID(), cats, items);
		S32 count;
		S32 count_tmp;
		S32 i;
		S32 j;
		LLInventoryItem* item = NULL;
		LLInventoryCategory* cat = NULL;
		//		LLViewerObject* attached_object = NULL;
		std::string attach_point_name = "";
		
		// Try to find locked items in the current category
		count = items->count();
		for (i = 0; i < count; ++i) {
			item = items->get(i);
			// If this is an object, add it if it is worn and locked, or worn and its attach point is locked
			if (item && item->getType() == LLAssetType::AT_OBJECT) {
				LLViewerObject* attached_object = avatar->getWornAttachment (item->getUUID());
				if (attached_object) {
					attach_point_name = avatar->getAttachedPointName (item->getLinkedUUID());
					if (!gAgent.mRRInterface.canDetach(attached_object)) {
						if (sRestrainedLoveDebug) {
							llinfos << "found a locked object : " << item->getName() << " on " << attach_point_name << llendl;
						}
						res.push_back (item);
					}
				}
			}
			// If this is a piece of clothing, add it if the avatar can't unwear clothes, or if this layer itself can't be unworn
			else if (item && item->getType() == LLAssetType::AT_CLOTHING) {
				if (gAgent.mRRInterface.contains ("remoutfit")
					|| gAgent.mRRInterface.containsSubstr ("remoutfit:")
					) {
					if (sRestrainedLoveDebug) {
						llinfos << "found a locked clothing : " << item->getName() << llendl;
					}
					res.push_back (item);
				}
			}
		}
		
		// We have all the locked objects contained directly in this folder, now add all the ones contained in children folders recursively
		count = cats->count();
		for (i = 0; i < count; ++i) {
			cat = cats->get(i);
			tmp = getListOfLockedItems (cat);
			count_tmp = tmp.size();
			for (j = 0; j < count_tmp; ++j) {
				item = tmp[j];
				if (item) res.push_back (item);
			}
		}
		
		if (sRestrainedLoveDebug) {
			llinfos << "number of locked objects under " << root->getName() << " =  " << res.size() << llendl;
		}
	}
	
	return res;
}


std::deque<std::string> RRInterface::getListOfRestrictions (LLUUID object_uuid, std::string rule /*= ""*/)
{
	std::deque<std::string> res;
	std::string name;
	RRMAP::iterator it;
	if (object_uuid.isNull()) {
		it = mSpecialObjectBehaviours.begin();
	}
	else {
		it = mSpecialObjectBehaviours.find (object_uuid.asString());
	}
	while (it != mSpecialObjectBehaviours.end() &&
			(object_uuid.isNull() || it != mSpecialObjectBehaviours.upper_bound(object_uuid.asString()))
	)
	{
		if (rule=="" || it->second.find (rule)!=-1) {
			res.push_back(it->second);
		}
		it++;
	}
	return res;
}


std::string RRInterface::getInventoryList (std::string path, BOOL withWornInfo /* = FALSE */)
{
	std::string res = "";
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	LLInventoryCategory* root = NULL;
	if (path == "") root = getRlvShare();
	else root = getCategoryUnderRlvShare (path);
	
	if (root) {
		gInventory.getDirectDescendentsOf (root->getUUID(), cats, items);
		if(cats) {
			S32 count = cats->count();
			bool found_one = false;
			if (withWornInfo) {
				res += "|" + getWornItems (root);
				found_one = true;
			}
			for(S32 i = 0; i < count; ++i) {
				LLInventoryCategory* cat = cats->get(i);
				std::string name = cat->getName();
				if (name != "" && name[0] !=  '.') { // hidden folders => invisible to the list
					if (found_one) res += ",";
					res += name.c_str();
					if (withWornInfo) {
						res += "|" + getWornItems (cat);
					}
					found_one = true;
				}
			}
		}
	}

	return res;
}

std::string RRInterface::getWornItems (LLInventoryCategory* cat)
{
	// Returns a string of 2 digits according to the proportion of worn items in this folder and its children :
	// First digit is this folder, second digit is children folders
	// 0 : No item contained in the folder
	// 1 : Some items contained but none is worn
	// 2 : Some items contained and some of them are worn
	// 3 : Some items contained and all of them are worn
	std::string res_as_string = "0";
	int res			= 0;
	int subRes		= 0;
	int prevSubRes	= 0;
	int nbItems		= 0;
	int nbWorn		= 0;
	BOOL isNoMod	= FALSE;
	BOOL isRoot		= (getRlvShare() == cat);
	
	// if cat exists, scan all the items inside it
	if (cat) {
	
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		
		// retrieve all the objects contained in this folder
		gInventory.getDirectDescendentsOf (cat->getUUID(), cats, items);
		if (!isRoot && items) { // do not scan the shared root
		
			// scan them one by one
			S32 count = items->count();
			for(S32 i = 0; i < count; ++i) {
			
				LLViewerInventoryItem* item = (LLViewerInventoryItem*)items->get(i);

				if (item) {
					LLVOAvatar* avatar = gAgent.getAvatarObject();
					if (item->getType() == LLAssetType::AT_OBJECT
					 || item->getType() == LLAssetType::AT_CLOTHING
					 || item->getType() == LLAssetType::AT_BODYPART
					) {
						nbItems++;
					}
					if ((avatar && avatar->isWearingAttachment(item->getUUID()))
						|| gAgent.isWearingItem (item->getUUID())) nbWorn++;

					// special case : this item is no-mod, hence we need to check its parent folder
					// is correctly named, and that the item is alone in its folder.
					// If so, then the calling method will have to deal with a special character instead
					// of a number
					if (count == 1
					 && item->getType() == LLAssetType::AT_OBJECT
					 && !item->getPermissions().allowModifyBy(gAgent.getID())) {
						if (findAttachmentPointFromName (cat->getName()) != NULL) {
							isNoMod = TRUE;
						}
					}
				}
			}
		}
		
		// scan every subfolder of the folder we are scanning, recursively
		// note : in the case of no-mod items we shouldn't have sub-folders, so no need to check
		if (cats && !isNoMod) {
		
			S32 count = cats->count();
			for(S32 i = 0; i < count; ++i) {

				LLViewerInventoryCategory* cat_child = (LLViewerInventoryCategory*)cats->get(i);

				if (cat_child) {
					std::string tmp = getWornItems (cat_child);
					// translate the result for no-mod items into something the upper levels can understand
					if (tmp == "N") {
						if (!isRoot) {
							nbWorn++;
							nbItems++;
						}
					}
					else if (tmp== "n") {
						if (!isRoot) {
							nbItems++;
						}
					}
					else if (cat_child->getName() != "" && cat_child->getName()[0] != '.') { // we don't want to include invisible folders, except the ones containing a no-mod item
						// This is an actual sub-folder with several items and sub-folders inside,
						// so retain its score to integrate it into the current one
						// As it is a sub-folder, to integrate it we need to reduce its score first (consider "0" as "ignore")
						// "00" = 0, "01" = 1, "10" = 1, "30" = 3, "03" = 3, "33" = 3, all the rest gives 2 (some worn, some not worn)
						if      (tmp == "00")                               subRes = 0;
						else if (tmp == "11" || tmp == "01" || tmp == "10") subRes = 1;
						else if (tmp == "33" || tmp == "03" || tmp == "30") subRes = 3;
						else subRes = 2;

						// Then we must combine with the previous sibling sub-folders
						// Same rule as above, set to 2 in all cases except when prevSubRes == subRes or when either == 0 (nothing present, ignore)
						if      (prevSubRes == 0 && subRes == 0) subRes = 0;
						else if (prevSubRes == 0 && subRes == 1) subRes = 1;
						else if (prevSubRes == 1 && subRes == 0) subRes = 1;
						else if (prevSubRes == 1 && subRes == 1) subRes = 1;
						else if (prevSubRes == 0 && subRes == 3) subRes = 3;
						else if (prevSubRes == 3 && subRes == 0) subRes = 3;
						else if (prevSubRes == 3 && subRes == 3) subRes = 3;
						else subRes = 2;
						prevSubRes = subRes;
					}
				}
			}
		}
	}

	if (isNoMod) {
		// the folder contains one no-mod object and is named from an attachment point
		// => return a special character that will be handled by the calling method
		if (nbWorn > 0) return "N";
		else return "n";
	}
	else {
		if (isRoot || nbItems == 0) res = 0; // forcibly hide all items contained directly under #RLV
		else if (nbWorn >= nbItems) res = 3;
		else if (nbWorn > 0) res = 2;
		else res = 1;
	}
	std::stringstream str;
	str << res;
	str << subRes;
	res_as_string = str.str();
	return res_as_string;
}

LLInventoryCategory* RRInterface::getRlvShare ()
{
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf (
					gInventory.findCategoryUUIDForType(LLAssetType::AT_CATEGORY), cats, items
	);

	if(cats) {
		S32 count = cats->count();
		for(S32 i = 0; i < count; ++i) {
			LLInventoryCategory* cat = cats->get(i);
			std::string name = cat->getName();
			if (name == RR_SHARED_FOLDER) {
				return cat;
			}
		}
	}
	return NULL;
}

BOOL RRInterface::isUnderRlvShare (LLInventoryItem* item)
{
	const LLUUID& cat_id = item->getParentUUID();
	return isUnderFolder(getRlvShare(), gInventory.getCategory(cat_id));
}

BOOL RRInterface::isUnderRlvShare (LLInventoryCategory* cat)
{
	return isUnderFolder (getRlvShare(), cat);
}

BOOL RRInterface::isUnderFolder (LLInventoryCategory* cat_parent, LLInventoryCategory* cat_child)
{
	if (cat_parent == NULL || cat_child == NULL) {
		return FALSE;
	}
	if (cat_child == cat_parent) {
		return TRUE;
	}
	LLInventoryCategory* res = NULL;
	LLUUID root_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_CATEGORY);

	const LLUUID& cat_id = cat_child->getParentUUID();
	res = gInventory.getCategory (cat_id);

	while (res && res->getUUID() != root_id) {
		if (res == cat_parent) {
			return TRUE;
		}
		const LLUUID& parent_id = res->getParentUUID();
		res = gInventory.getCategory (parent_id);
	}
	return FALSE;
}

LLInventoryCategory* RRInterface::getCategoryUnderRlvShare (std::string catName, LLInventoryCategory* root)
{
	if (root == NULL) root = getRlvShare();
	if (catName == "") return root;
	LLStringUtil::toLower (catName);
	std::deque<std::string> tokens = parse (catName, "/");

	// Preliminary action : remove everything after pipes ("|"), including pipes themselves
	// This way we can feed the result of a @getinvworn command directly into this method
	// without having to clean what is after the pipes
	int nb = tokens.size();
	for (int i=0; i<nb; ++i) {
		std::string tok = tokens[i];
		int ind = tok.find ("|");
		if (ind != -1) {
			tok = tok.substr (0, ind);
			tokens[i] = tok;
		}
	}
	
	if (root) {
		bool exact_match = false;
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf (root->getUUID(), cats, items);
		
		if(cats) {
			S32 count = cats->count();
			LLInventoryCategory* cat = NULL;
			
			// we need to scan first and retain the best match
			int max_size_index = -1;
			int max_size = 0;
			
			for(S32 i = 0; i < count; ++i) {
				cat = cats->get(i);
				std::string name = cat->getName();
				if (name != "" && name[0] !=  '.') { // ignore invisible folders
					LLStringUtil::toLower (name);
					
					int size = match (tokens, name, exact_match);
					if (size > max_size || (exact_match && size == max_size)) {
						max_size = size;
						max_size_index = i;
					}
				}
			}

			// only now we can grab the best match and either continue deeper or return it
			if (max_size > 0) {
				cat = cats->get(max_size_index);
				if (max_size == tokens.size()) {
					return cat;
				} else {
					return getCategoryUnderRlvShare(dumpList2String(getSubList(tokens, max_size), "/"), cat);
				}
			}
		}
	}

	if (sRestrainedLoveDebug) {
		llinfos << "category not found" << llendl;
	}
	return NULL;
}

LLInventoryCategory* RRInterface::findCategoryUnderRlvShare (std::string catName, LLInventoryCategory* root)
{
	if (root == NULL) root = getRlvShare();
	LLStringUtil::toLower (catName);
	std::deque<std::string> tokens = parse (catName, "&&");
	
	if (root) {
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf (root->getUUID(), cats, items);
		
		if(cats)
		{
			S32 count = cats->count();
			LLInventoryCategory* cat = NULL;
			
			for(S32 i = 0; i < count; ++i)
			{
				cat = cats->get(i);
				
				// search deeper first
				LLInventoryCategory* found = findCategoryUnderRlvShare (catName, cat);
				if (found != NULL) return found;
				
			}
		}
		// return this category if it matches
		std::string name = root->getName();
		LLStringUtil::toLower (name);
		// We can't find invisible folders ('.') and dropped folders ('~')
		if (name != "" && name[0] != '.' && name[0] != '~' && findMultiple (tokens, name)) return root;
	}
	// didn't find anything
	return NULL;
}

std::string RRInterface::findAttachmentNameFromPoint (LLViewerJointAttachment* attachpt)
{
	// return the lowercased name of the attachment, or empty if null
	if (attachpt == NULL) return "";
	std::string res = attachpt->getName();
	LLStringUtil::toLower(res);
	return res;
}

// This struct is meant to be used in RRInterface::findAttachmentPointFromName below
typedef struct
{
	int length;
	int index;
	LLViewerJointAttachment* attachment;
} Candidate;

LLViewerJointAttachment* RRInterface::findAttachmentPointFromName (std::string objectName, BOOL exactName)
{
	// for each possible attachment point, check whether its name appears in the name of
	// the item.
	// We are going to scan the whole list of attachments, but we won't decide which one to take right away.
	// Instead, for each matching point, we will store in lists the following results :
	// - length of its name
	// - right-most index where it is found in the name
	// - a pointer to that attachment point
	// When we have that list, choose the highest index, and in case of ex-aequo choose the longest length
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) {
		llwarns << "NULL avatar pointer. Aborting." << llendl;
		return NULL;
	}
	LLStringUtil::toLower(objectName);
	std::string attachName;
	int ind = -1;
	bool found_one = false;
	std::vector<Candidate> candidates;
	
	for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin(); 
		 iter != avatar->mAttachmentPoints.end(); )
	{
		LLVOAvatar::attachment_map_t::iterator curiter = iter++;
		LLViewerJointAttachment* attachment = curiter->second;
		if (attachment) {
			attachName = attachment->getName();
			LLStringUtil::toLower(attachName);
			if (exactName && objectName == attachName) return attachment;
			else if (!exactName && (ind = objectName.rfind (attachName)) != -1)
			{
				Candidate new_candidate;
				new_candidate.index = ind;
				new_candidate.length = attachName.length();
				new_candidate.attachment = attachment;
				candidates.push_back (new_candidate);
				found_one = true;
				if (sRestrainedLoveDebug) {
					llinfos << "new candidate '" << attachName << "' : index=" << new_candidate.index << "   length=" << new_candidate.length << llendl;
				}
			}
		}
	}
	if (!found_one) {
		if (sRestrainedLoveDebug) {
			llinfos << "no attachment found" << llendl;
		}
		return NULL;
	}
	// Now that we have at least one candidate, we have to decide which one to return
	LLViewerJointAttachment* res = NULL;
	Candidate candidate;
	unsigned int i;
	int ind_res = -1;
	int max_index = -1;
	int max_length = -1;
	// Find the highest index
	for (i=0; i<candidates.size(); ++i) {
		candidate = candidates[i];
		if (candidate.index > max_index) max_index = candidate.index;
	}
	// Find the longest match among the ones found at that index
	for (i=0; i<candidates.size(); ++i) {
		candidate = candidates[i];
		if (candidate.index == max_index) {
			if (candidate.length > max_length) {
				max_length = candidate.length;
				ind_res = i;
			}
		}
	}
	// Return this attachment point
	if (ind_res > -1) {
		candidate = candidates[ind_res];
		res = candidate.attachment;
		if (sRestrainedLoveDebug && res) {
			llinfos << "returning '" << res->getName() << "'" << llendl;
		}
	}
	return res;
}

LLViewerJointAttachment* RRInterface::findAttachmentPointFromParentName (LLInventoryItem* item)
{
	if (item) {
		// => look in parent folder (this could be a no-mod item), use its name to find the target
		// attach point
		LLViewerInventoryCategory* cat;
		const LLUUID& parent_id = item->getParentUUID();
		cat = gInventory.getCategory (parent_id);
		return findAttachmentPointFromName (cat->getName());
	}
	return NULL;
}

S32 RRInterface::findAttachmentPointNumber (LLViewerJointAttachment* attachment)
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (avatar) {
		for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin();
			 iter != avatar->mAttachmentPoints.end(); ++iter)
		{
			if (iter->second == attachment) {
				return iter->first;
			}
		}
	}
	return -1;
}

void RRInterface::detachObject(LLViewerObject* object)
{
	// Handle the detach message to the sim here, after a check
	if (!object) return;
	if (gRRenabled && !gAgent.mRRInterface.canDetach(object)) return;

	gMessageSystem->newMessage("ObjectDetach");
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
	gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
	gMessageSystem->sendReliable( gAgent.getRegionHost() );
}

void RRInterface::detachAllObjectsFromAttachment(LLViewerJointAttachment* attachment)
{
	if (!attachment) return;
	LLViewerObject* object;

	// We need to remove all the objects from attachment->mAttachedObjects, one by one.
	// To do this, and in order to avoid any race condition, we are going to copy the list and 
	// iterate on the copy instead of the original which changes everytime something is
	// attached and detached, asynchronously.
	LLViewerJointAttachment::attachedobjs_vec_t attachedObjectsCopy = attachment ->mAttachedObjects;

	for (unsigned int i = 0; i < attachedObjectsCopy.size(); ++i) {
		object = attachedObjectsCopy.at(i);
		detachObject (object);
	}
}

bool RRInterface::canDetachAllObjectsFromAttachment(LLViewerJointAttachment* attachment, BOOL handle_nostrip /* = TRUE */)
{
	if (!attachment) return false;
	LLViewerObject* object;

	for (unsigned int i = 0; i <  attachment ->mAttachedObjects.size(); ++i) {
		object =  attachment ->mAttachedObjects.at(i);
		if (!gAgent.mRRInterface.canDetach(object, handle_nostrip)) return false;
	}

	return true;
}
void RRInterface::fetchInventory (LLInventoryCategory* root)
{
	// do this only once on login

	if (mInventoryFetched) return;
	
	bool last_step = false;
	
	if (root == NULL) {
		root = getRlvShare();
		last_step = true;
	}
	
	if (root) {
		LLViewerInventoryCategory* viewer_root = (LLViewerInventoryCategory*) root;
		viewer_root->fetchDescendents ();

		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		
		// retrieve all the shared folders
		gInventory.getDirectDescendentsOf (viewer_root->getUUID(), cats, items);
		if (cats) {
			S32 count = cats->count();
			for(S32 i = 0; i < count; ++i) {
				LLInventoryCategory* cat = (LLInventoryCategory*)cats->get(i);
				fetchInventory (cat);
			}
		}

	}
	
	if (last_step) mInventoryFetched = TRUE;
}

void confirm_replace_attachment_rez(S32 option, void* user_data);

BOOL RRInterface::forceAttach (std::string category, BOOL recursive, AttachHow how)
{
	// recursive is TRUE in the case of an attachall command
	// find the category under RLV shared folder
	if (category == "") return TRUE; // just a safety
	LLInventoryCategory* cat = getCategoryUnderRlvShare (category);
	BOOL isRoot = (getRlvShare() == cat);
	BOOL replacing = (how == AttachHow_replace || how == AttachHow_over_or_replace); // we're replacing for now, but the name of the category could decide otherwise
	// if exists, wear all the items inside it
	if (cat) {
	
		// If the name of the category begins with a "+", then we force to stack instead of replacing
		if (how == AttachHow_over_or_replace) {
			if (cat->getName().find (RR_ADD_FOLDER_PREFIX) == 0) {
				replacing = FALSE;
			}
		}
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		
		// retrieve all the objects contained in this folder
		gInventory.getDirectDescendentsOf (cat->getUUID(), cats, items);
		if (items) {
		
			// wear them one by one
			S32 count = items->count();
			for(S32 i = 0; i < count; ++i) {
				if (!isRoot) {
					LLViewerInventoryItem* item = (LLViewerInventoryItem*)items->get(i);
					if (sRestrainedLoveDebug) {
						llinfos << "trying to attach " << item->getName() << llendl;
					}
					
					// this is an object to attach somewhere
					if (item && item->getType() == LLAssetType::AT_OBJECT) {
						LLViewerJointAttachment* attachpt = findAttachmentPointFromName (item->getName());
						
						if (attachpt) {
							if (sRestrainedLoveDebug) {
								llinfos << "attaching item to " << attachpt->getName() << llendl;
							}
							if (replacing)
							{
								// mimick rez_attachment without displaying an Xml alert to confirm
								S32 number = findAttachmentPointNumber (attachpt);
								if (canDetach(attachpt->getName()) && canAttach(item))
								{
									LLAttachmentRezAction* rez_action = new LLAttachmentRezAction;
									rez_action->mItemID = item->getLinkedUUID();
									rez_action->mAttachPt = number;
									confirm_replace_attachment_rez(0/*YES*/, (void*)rez_action);
								}
							}
							else {
								// we're stacking => call rez_attachment directly
								rez_attachment (item, attachpt, false);
							}
						}
					}
					// this is a piece of clothing
					else if (item->getType() == LLAssetType::AT_CLOTHING
							 || item->getType() == LLAssetType::AT_BODYPART) {
						wear_inventory_item_on_avatar (item);
					}
				}
			}
		}
		
		// scan every subfolder of the folder we are attaching, in order to attach no-mod items
		if (cats) {
		
			// for each subfolder, attach the first item it contains according to its name
			S32 count = cats->count();
			for(S32 i = 0; i < count; ++i) {
				LLViewerInventoryCategory* cat_child = (LLViewerInventoryCategory*)cats->get(i);
				LLViewerJointAttachment* attachpt = findAttachmentPointFromName (cat_child->getName());
				
				if (!isRoot) {
					if (attachpt) {
						// this subfolder is properly named => attach the first item it contains
						LLInventoryModel::cat_array_t* cats_grandchildren; // won't be used here
						LLInventoryModel::item_array_t* items_grandchildren; // actual no-mod item(s)
						gInventory.getDirectDescendentsOf (cat_child->getUUID(), 
															cats_grandchildren, items_grandchildren);

						if (items_grandchildren && items_grandchildren->count() == 1) {
							LLViewerInventoryItem* item_grandchild = 
									(LLViewerInventoryItem*)items_grandchildren->get(0);

							if (item_grandchild && item_grandchild->getType() == LLAssetType::AT_OBJECT
								&& !item_grandchild->getPermissions().allowModifyBy(gAgent.getID())
								&& findAttachmentPointFromParentName (item_grandchild) != NULL) { // it is no-mod and its parent is named correctly
								// we use the attach point from the name of the folder, not the no-mod item
								if (replacing) {
									// mimick rez_attachment without displaying an Xml alert to confirm
									S32 number = findAttachmentPointNumber (attachpt);
									if (canDetach(attachpt->getName()) && canAttach(item_grandchild))
									{
									LLAttachmentRezAction* rez_action = new LLAttachmentRezAction;
									rez_action->mItemID = item_grandchild->getLinkedUUID();
									rez_action->mAttachPt = number;
									confirm_replace_attachment_rez(0/*YES*/, (void*)rez_action);
									}
								}
								else {
									// we're stacking => call rez_attachment directly
									rez_attachment (item_grandchild, attachpt, false);
								}
							}
						}
					}
				}

				if (recursive && cat_child->getName().find (".") != 0) { // attachall and not invisible)
					forceAttach (getFullPath (cat_child), recursive, how);
				}
			}
		}
	}
	return TRUE;
}

BOOL RRInterface::forceDetachByName (std::string category, BOOL recursive, BOOL handle_nostrip)
{
	// find the category under RLV shared folder
	if (category == "") return TRUE; // just a safety
	LLInventoryCategory* cat = getCategoryUnderRlvShare (category);
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) return FALSE;
	BOOL isRoot = (getRlvShare() == cat);
	
	// if exists, detach/unwear all the items inside it
	if (cat) {
		if (handle_nostrip) {
			std::string name = cat->getName();
			LLStringUtil::toLower(name);
			if (name.find ("nostrip") != -1) return false;
		}

		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		
		// retrieve all the objects contained in this folder
		gInventory.getDirectDescendentsOf (cat->getUUID(), cats, items);
		if (items) {
		
			// unwear them one by one
			S32 count = items->count();
			for(S32 i = 0; i < count; ++i) {
				if (!isRoot) {
					LLViewerInventoryItem* item = (LLViewerInventoryItem*)items->get(i);
					if (sRestrainedLoveDebug) {
						llinfos << "trying to detach " << item->getName() << llendl;
					}
					
					// attached object
					if (item->getType() == LLAssetType::AT_OBJECT) {
						// find the attachpoint from which to detach
						for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin(); 
							 iter != avatar->mAttachmentPoints.end(); )
						{
							LLVOAvatar::attachment_map_t::iterator curiter = iter++;
							LLViewerJointAttachment* attachment = curiter->second;
							LLViewerObject* object = avatar->getWornAttachment(item->getUUID());
							if (object && attachment && attachment->isObjectAttached(object))
							{
								detachObject (object);
								break;
							}
							handle_detach_from_avatar ((void*)attachment);
							break;
						}					
					}
					// piece of clothing
					else if (item->getType() == LLAssetType::AT_CLOTHING) {
						LLWearable* layer = gAgent.getWearableFromWearableItem (item->getLinkedUUID());
						if (layer && canDetach (item)) removeWearableFromAvatar (layer->getType());
						
					}
				}
			}
		}
		
		if (cats) {
			// for each subfolder, detach the first item it contains (only for single no-mod items contained in appropriately named folders)
			S32 count = cats->count();
			for(S32 i = 0; i < count; ++i) {
				LLViewerInventoryCategory* cat_child = (LLViewerInventoryCategory*)cats->get(i);
				if (handle_nostrip) {
					std::string name = cat_child->getName();
					LLStringUtil::toLower(name);
					if (name.find ("nostrip") != -1) continue;
				}

				LLInventoryModel::cat_array_t* cats_grandchildren; // won't be used here
				LLInventoryModel::item_array_t* items_grandchildren; // actual no-mod item(s)
				gInventory.getDirectDescendentsOf (cat_child->getUUID(), 
													cats_grandchildren, items_grandchildren);

				if (!isRoot && items_grandchildren && items_grandchildren->count() == 1) { // only one item
					LLViewerInventoryItem* item_grandchild = 
							(LLViewerInventoryItem*)items_grandchildren->get(0);

					if (item_grandchild && item_grandchild->getType() == LLAssetType::AT_OBJECT
						&& !item_grandchild->getPermissions().allowModifyBy(gAgent.getID())
						&& findAttachmentPointFromParentName (item_grandchild) != NULL) { // and it is no-mod and its parent is named correctly
						// detach this object
						// find the attachpoint from which to detach
						for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin(); 
							 iter != avatar->mAttachmentPoints.end(); )
						{
							LLVOAvatar::attachment_map_t::iterator curiter = iter++;
							LLViewerJointAttachment* attachment = curiter->second;
							LLViewerObject* object = avatar->getWornAttachment(item_grandchild->getUUID());
							if (object && attachment && attachment->isObjectAttached(object))
							{
								detachObject (object);
								break;
							}
							handle_detach_from_avatar ((void*)attachment);
							break;
						}
					}
				}

				if (recursive && cat_child->getName().find (".") != 0) { // detachall and not invisible)
					forceDetachByName (getFullPath (cat_child), recursive, handle_nostrip);
				}
			}
		}
	}
	return TRUE;
}

BOOL RRInterface::forceTeleport (std::string location)
{
	// location must be X/Y/Z where X, Y and Z are ABSOLUTE coordinates => use a script in-world to translate from local to global
	std::string loc (location);
	std::string region_name;
	S32 x = 128;
	S32 y = 128;
	S32 z = 0;
	std::deque<std::string> tokens=parse (location, "/");
	if (tokens.size()==3) {
		x=atoi (tokens.at(0).c_str());
		y=atoi (tokens.at(1).c_str());
		z=atoi (tokens.at(2).c_str());
	}
	else {
		return FALSE;
	}

	if (sRestrainedLoveDebug) {
		llinfos << tokens.at(0) << "," << tokens.at(1) << "," << tokens.at(2) << "     " << x << "," << y << "," << z << llendl;
	}
	LLVector3d pos_global;
	pos_global.mdV[VX] = (F32)x;
	pos_global.mdV[VY] = (F32)y;
	pos_global.mdV[VZ] = (F32)z;
	
	mAllowCancelTp = FALSE; // will be checked once receiving the tp order from the sim, then set to TRUE again

	gAgent.teleportViaLocation (pos_global);
	return TRUE;
}

std::string RRInterface::stringReplace (std::string s, std::string what, std::string by, BOOL caseSensitive /* = FALSE */)
{
//	llinfos << "trying to replace <" << what << "> in <" << s << "> by <" << by << ">" << llendl;
	if (what == "" || what == " ") return s; // avoid an infinite loop
	int ind;
	int old_ind = 0;
	int len_what = what.length();
	int len_by = by.length();
	if (len_by == 0) len_by = 1; // avoid an infinite loop
	
	while ((ind = s.find ("%20")) != -1) // unescape
	{
		s = s.replace (ind, 3, " ");
	}
	
	std::string lower = s;
	if (!caseSensitive) {
		LLStringUtil::toLower (lower);
		LLStringUtil::toLower (what);
	}
	
	while ((ind = lower.find (what, old_ind)) != -1)
	{
//		llinfos << "ind=" << ind << "    old_ind=" << old_ind << llendl;
		s = s.replace (ind, len_what, by);
		old_ind = ind + len_by;
		lower = s;
		if (!caseSensitive) LLStringUtil::toLower (lower);
	}
	return s;
	
}

std::string RRInterface::getDummyName (std::string name, EChatAudible audible /* = CHAT_AUDIBLE_FULLY */)
{
	int len = name.length();
	if (len < 2) return ""; // just to avoid crashing in some cases
	unsigned char hash = name.at(3) + len; // very lame hash function I know... but it should be linear enough (the old length method was way too gaussian with a peak at 11 to 16 characters)
	unsigned char mod = hash % 28;
	std::string res = "";
	switch (mod) {
		case 0:		res = "A resident";			break;
		case 1:		res = "This resident";		break;
		case 2:		res = "That resident";		break;
		case 3:		res = "An individual";		break;
		case 4:		res = "This individual";	break;
		case 5:		res = "That individual";	break;
		case 6:		res = "A person";			break;
		case 7:		res = "This person";		break;
		case 8:		res = "That person";		break;
		case 9:		res = "A stranger";			break;
		case 10:	res = "This stranger";		break;
		case 11:	res = "That stranger";		break;
		case 12:	res = "A human being";		break;
		case 13:	res = "This human being";	break;
		case 14:	res = "That human being";	break;
		case 15:	res = "An agent";			break;
		case 16:	res = "This agent";			break;
		case 17:	res = "That agent";			break;
		case 18:	res = "A soul";				break;
		case 19:	res = "This soul";			break;
		case 20:	res = "That soul";			break;
		case 21:	res = "Somebody";			break;
		case 22:	res = "Some people";		break;
		case 23:	res = "Someone";			break;
		case 24:	res = "Mysterious one";		break;
		case 25:	res = "An unknown being";	break;
		case 26:	res = "Unidentified one";	break;
		default:	res = "An unknown person";	break;
	}
	if (audible == CHAT_AUDIBLE_BARELY) res += " afar";
	return res;
}

std::string RRInterface::getCensoredMessage (std::string str)
{
	// Hide every occurrence of the name of anybody around (found in cache, so not completely accurate nor completely immediate)
	std::vector<LLUUID> avatar_ids;
	LLUUID avatar_id;
	std::string name, dummy_name;
	LLAvatarName avatar_name;
	U32 i;

	LLWorld::getInstance()->getAvatars(&avatar_ids, NULL);

	for (i = 0; i < avatar_ids.size(); i++) {
		avatar_id = avatar_ids[i];
		if (gCacheName->getFullName(avatar_id, name)) {
			dummy_name = getDummyName(name);
			str = stringReplace(str, name, dummy_name); // legacy name
			if (LLAvatarNameCache::get(avatar_id, &avatar_name)) {
				if (!avatar_name.mIsDisplayNameDefault) {
					name = avatar_name.mDisplayName;
					str = stringReplace(str, name, dummy_name); // display name
				}
			}
		}
	}

	return str;
}

void updateAndSave (WLColorControl* color)
{
	if (color == NULL) return;
	color->i = color->r;
	if (color->g > color->i) {
		color->i = color->g;
	}
	if (color->b > color->i) {
		color->i = color->b;
	}
	color->update (LLWLParamManager::instance()->mCurParams);
}

void updateAndSave (WLFloatControl* floatControl)
{
	if (floatControl == NULL) return;
	floatControl->update (LLWLParamManager::instance()->mCurParams);
}

BOOL RRInterface::forceEnvironment (std::string command, std::string option)
{
	// command is "setenv_<something>"
	double val = atof (option.c_str());

	int length = 7; // size of "setenv_"
	command = command.substr (length);
	LLWLParamManager* params = LLWLParamManager::instance();

	params->mAnimator.mIsRunning = false;
	params->mAnimator.mUseLindenTime = false;

	if (command == "daytime") {
		if (val > 1.0) val = 1.0;
		if (val >= 0.0) {
			params->mAnimator.setDayTime(val);
			params->mAnimator.update(params->mCurParams);
		}
		else {
			LLWLParamManager::instance()->mAnimator.mIsRunning = true;
			LLWLParamManager::instance()->mAnimator.mUseLindenTime = true;
		}
	}
	else if (command == "bluehorizonr") {
		params->mBlueHorizon.r = val*2;
		updateAndSave (&(params->mBlueHorizon));
	}
	else if (command == "bluehorizong") {
		params->mBlueHorizon.g = val*2;
		updateAndSave (&(params->mBlueHorizon));
	}
	else if (command == "bluehorizonb") {
		params->mBlueHorizon.b = val*2;
		updateAndSave (&(params->mBlueHorizon));
	}
	else if (command == "bluehorizoni") {
		params->mBlueHorizon.r = val*2;
		params->mBlueHorizon.g = val*2;
		params->mBlueHorizon.b = val*2;
		updateAndSave (&(params->mBlueHorizon));
	}

	else if (command == "bluedensityr") {
		params->mBlueDensity.r = val*2;
		updateAndSave (&(params->mBlueDensity));
	}
	else if (command == "bluedensityg") {
		params->mBlueDensity.g = val*2;
		updateAndSave (&(params->mBlueDensity));
	}
	else if (command == "bluedensityb") {
		params->mBlueDensity.b = val*2;
		updateAndSave (&(params->mBlueDensity));
	}
	else if (command == "bluedensityi") {
		params->mBlueDensity.r = val*2;
		params->mBlueDensity.g = val*2;
		params->mBlueDensity.b = val*2;
		updateAndSave (&(params->mBlueDensity));
	}

	else if (command == "hazehorizon") {
		params->mHazeHorizon.r = val*2;
		params->mHazeHorizon.g = val*2;
		params->mHazeHorizon.b = val*2;
		updateAndSave (&(params->mHazeHorizon));
	}
	else if (command == "hazedensity") {
		params->mHazeDensity.r = val*2;
		params->mHazeDensity.g = val*2;
		params->mHazeDensity.b = val*2;
		updateAndSave (&(params->mHazeDensity));
	}

	else if (command == "densitymultiplier") {
		params->mDensityMult.x = val/1000;
		updateAndSave (&(params->mDensityMult));
//		LLWaterParamManager* water_params = LLWaterParamManager::instance();
//		water_params->mFogDensity.mExp = 5.0;
//		water_params->mFogDensity.update (water_params->mCurParams);
	}
	else if (command == "distancemultiplier") {
		params->mDistanceMult.x = val;
		updateAndSave (&(params->mDistanceMult));
//		LLWaterParamManager* water_params = LLWaterParamManager::instance();
//		water_params->mUnderWaterFogMod.mX = 1.0;
//		water_params->mUnderWaterFogMod.update (water_params->mCurParams);
	}
	else if (command == "maxaltitude") {
		params->mMaxAlt.x = val;
		updateAndSave (&(params->mMaxAlt));
	}

	else if (command == "sunmooncolorr") {
		params->mSunlight.r = val*3;
		updateAndSave (&(params->mSunlight));
	}
	else if (command == "sunmooncolorg") {
		params->mSunlight.g = val*3;
		updateAndSave (&(params->mSunlight));
	}
	else if (command == "sunmooncolorb") {
		params->mSunlight.b = val*3;
		updateAndSave (&(params->mSunlight));
	}
	else if (command == "sunmooncolori") {
		params->mSunlight.r = val*3;
		params->mSunlight.g = val*3;
		params->mSunlight.b = val*3;
		updateAndSave (&(params->mSunlight));
	}

	else if (command == "ambientr") {
		params->mAmbient.r = val*3;
		updateAndSave (&(params->mAmbient));
	}
	else if (command == "ambientg") {
		params->mAmbient.g = val*3;
		updateAndSave (&(params->mAmbient));
	}
	else if (command == "ambientb") {
		params->mAmbient.b = val*3;
		updateAndSave (&(params->mAmbient));
	}
	else if (command == "ambienti") {
		params->mAmbient.r = val*3;
		params->mAmbient.g = val*3;
		params->mAmbient.b = val*3;
		updateAndSave (&(params->mAmbient));
	}
	else if (command == "sunglowfocus") {
		params->mGlow.b = -val*5;
		updateAndSave (&(params->mGlow));
	}
	else if (command == "sunglowsize") {
		params->mGlow.r = (2-val)*20;
		updateAndSave (&(params->mGlow));
	}
	else if (command == "scenegamma") {
		params->mWLGamma.x = val;
		updateAndSave (&(params->mWLGamma));
	}
	else if (command == "sunmoonposition") {
		params->mCurParams.setSunAngle (F_TWO_PI * val);
	}
	else if (command == "eastangle") {
		params->mCurParams.setEastAngle (F_TWO_PI * val);
	}
	else if (command == "starbrightness") {
		params->mCurParams.setStarBrightness (val);
	}

	else if (command == "cloudcolorr") {
		params->mCloudColor.r = val;
		updateAndSave (&(params->mCloudColor));
	}
	else if (command == "cloudcolorg") {
		params->mCloudColor.g = val;
		updateAndSave (&(params->mCloudColor));
	}
	else if (command == "cloudcolorb") {
		params->mCloudColor.b = val;
		updateAndSave (&(params->mCloudColor));
	}
	else if (command == "cloudcolori") {
		params->mCloudColor.r = val;
		params->mCloudColor.g = val;
		params->mCloudColor.b = val;
		updateAndSave (&(params->mCloudColor));
	}

	else if (command == "cloudx") {
		params->mCloudMain.r = val;
		updateAndSave (&(params->mCloudMain));
	}
	else if (command == "cloudy") {
		params->mCloudMain.g = val;
		updateAndSave (&(params->mCloudMain));
	}
	else if (command == "cloudd") {
		params->mCloudMain.b = val;
		updateAndSave (&(params->mCloudMain));
	}

	else if (command == "clouddetailx") {
		params->mCloudDetail.r = val;
		updateAndSave (&(params->mCloudDetail));
	}
	else if (command == "clouddetaily") {
		params->mCloudDetail.g = val;
		updateAndSave (&(params->mCloudDetail));
	}
	else if (command == "clouddetaild") {
		params->mCloudDetail.b = val;
		updateAndSave (&(params->mCloudDetail));
	}

	else if (command == "cloudcoverage") {
		params->mCloudCoverage.x = val;
		updateAndSave (&(params->mCloudCoverage));
	}
	else if (command == "cloudscale") {
		params->mCloudScale.x = val;
		updateAndSave (&(params->mCloudScale));
	}

	else if (command == "cloudscrollx") {
		params->mCurParams.setCloudScrollX (val+10);
	}
	else if (command == "cloudscrolly") {
		params->mCurParams.setCloudScrollY (val+10);
	}
	// sunglowfocus 0-0.5, sunglowsize 0-2, scenegamma 0-10, starbrightness 0-2
	// cloudcolor rgb 0-1, cloudxydensity xyd 0-1, cloudcoverage 0-1, cloudscale 0-1, clouddetail xyd 0-1
	// cloudscrollx 0-1, cloudscrolly 0-1, drawclassicclouds 0/1

	else if (command == "preset") {
		params->loadPreset (option);
	}

	// send the current parameters to shaders
	LLWLParamManager::instance()->propagateParameters();

	return TRUE;
}

std::string RRInterface::getEnvironment (std::string command)
{
	F64 res = 0;
	int length = 7; // size of "getenv_"
	command = command.substr (length);
	LLWLParamManager* params = LLWLParamManager::instance();

	if (command == "daytime") {
		if (params->mAnimator.mIsRunning && params->mAnimator.mUseLindenTime) res = -1;
		else res = params->mAnimator.getDayTime();
	}

	else if (command == "bluehorizonr") res = params->mBlueHorizon.r/2;
	else if (command == "bluehorizong") res = params->mBlueHorizon.g/2;
	else if (command == "bluehorizonb") res = params->mBlueHorizon.b/2;
	else if (command == "bluehorizoni") res = max (max (params->mBlueHorizon.r, params->mBlueHorizon.g), params->mBlueHorizon.b) / 2;

	else if (command == "bluedensityr") res = params->mBlueDensity.r/2;
	else if (command == "bluedensityg") res = params->mBlueDensity.g/2;
	else if (command == "bluedensityb") res = params->mBlueDensity.b/2;
	else if (command == "bluedensityi") res = max (max (params->mBlueDensity.r, params->mBlueDensity.g), params->mBlueDensity.b) / 2;

	else if (command == "hazehorizon")  res = max (max (params->mHazeHorizon.r, params->mHazeHorizon.g), params->mHazeHorizon.b) / 2;
	else if (command == "hazedensity")  res = max (max (params->mHazeDensity.r, params->mHazeDensity.g), params->mHazeDensity.b) / 2;

	else if (command == "densitymultiplier")  res = params->mDensityMult.x*1000;
	else if (command == "distancemultiplier") res = params->mDistanceMult.x;
	else if (command == "maxaltitude")        res = params->mMaxAlt.x;

	else if (command == "sunmooncolorr") res = params->mSunlight.r/3;
	else if (command == "sunmooncolorg") res = params->mSunlight.g/3;
	else if (command == "sunmooncolorb") res = params->mSunlight.b/3;
	else if (command == "sunmooncolori") res = max (max (params->mSunlight.r, params->mSunlight.g), params->mSunlight.b) / 3;

	else if (command == "ambientr") res = params->mAmbient.r/3;
	else if (command == "ambientg") res = params->mAmbient.g/3;
	else if (command == "ambientb") res = params->mAmbient.b/3;
	else if (command == "ambienti") res = max (max (params->mAmbient.r, params->mAmbient.g), params->mAmbient.b) / 3;

	else if (command == "sunglowfocus")	res = -params->mGlow.b/5;
	else if (command == "sunglowsize")		res = 2-params->mGlow.r/20;
	else if (command == "scenegamma")		res = params->mWLGamma.x;

	else if (command == "sunmoonposition")		res = params->mCurParams.getSunAngle()/F_TWO_PI;
	else if (command == "eastangle")			res = params->mCurParams.getEastAngle()/F_TWO_PI;
	else if (command == "starbrightness")		res = params->mCurParams.getStarBrightness();

	else if (command == "cloudcolorr") res = params->mCloudColor.r;
	else if (command == "cloudcolorg") res = params->mCloudColor.g;
	else if (command == "cloudcolorb") res = params->mCloudColor.b;
	else if (command == "cloudcolori") res = max (max (params->mCloudColor.r, params->mCloudColor.g), params->mCloudColor.b);

	else if (command == "cloudx")  res = params->mCloudMain.r;
	else if (command == "cloudy")  res = params->mCloudMain.g;
	else if (command == "cloudd")  res = params->mCloudMain.b;

	else if (command == "clouddetailx")  res = params->mCloudDetail.r;
	else if (command == "clouddetaily")  res = params->mCloudDetail.g;
	else if (command == "clouddetaild")  res = params->mCloudDetail.b;

	else if (command == "cloudcoverage")	res = params->mCloudCoverage.x;
	else if (command == "cloudscale")		res = params->mCloudScale.x;

	else if (command == "cloudscrollx") res = params->mCurParams.getCloudScrollX() - 10;
	else if (command == "cloudscrolly") res = params->mCurParams.getCloudScrollY() - 10;

	else if (command == "preset") return getLastLoadedPreset();

	std::stringstream str;
	str << res;
	return str.str();
}

BOOL RRInterface::forceDebugSetting (std::string command, std::string option)
{
	//	MK: As some debug settings are critical to the user's experience and others
	//	are just useless/not used, we are following a whitelist approach : only allow
	//	certain debug settings to be changed and not all.
	
	// command is "setdebug_<something>"
	
	int length = 9; // size of "setdebug_"
	command = command.substr (length);
	LLStringUtil::toLower(command);
	std::string allowed;
	std::string tmp;
	int ind;
	
	allowed = mAllowedU32;
	tmp = allowed;
	LLStringUtil::toLower(tmp);
	if ((ind = tmp.find ("," + command + ",")) != -1) {
		gSavedSettings.setU32 (allowed.substr(++ind, command.length()), atoi(option.c_str()));
		return TRUE;
	}
	
	return TRUE;
}

std::string RRInterface::getDebugSetting (std::string command)
{
	std::stringstream res;
	int length = 9; // size of "getdebug_"
	command = command.substr (length);
	LLStringUtil::toLower(command);
	std::string allowed;
	std::string tmp;
	int ind;
	
	allowed = mAllowedU32;
	tmp = allowed;
	LLStringUtil::toLower(tmp);
	if ((ind = tmp.find ("," + command + ",")) != -1) {
		res << gSavedSettings.getU32 (allowed.substr(++ind, command.length()));
	}
	
	return res.str();
}

std::string RRInterface::getFullPath (LLInventoryCategory* cat)
{
	if (cat == NULL) return "";
	LLInventoryCategory* rlv = gAgent.mRRInterface.getRlvShare();
	if (rlv == NULL) return "";
	LLInventoryCategory* res = cat;
	std::deque<std::string> tokens;
	
	while (res && res != rlv) {
		tokens.push_front (res->getName());
		const LLUUID& parent_id = res->getParentUUID();
		res = gInventory.getCategory (parent_id);
	}
	return dumpList2String (tokens, "/");
}

std::string RRInterface::getFullPath (LLInventoryItem* item, std::string option, bool full_list /*= true*/)
{
	if (sRestrainedLoveDebug) {
		llinfos << "getFullPath(" << (item? item->getName(): "NULL") << ", " << option << ", " << full_list << ")" << llendl;
	}
	// Returns the path from the shared root to this object, or to the object worn at the attach point or clothing layer pointed by option if any
	if (option != "") {
		item = NULL; // an option is specified => we don't want to check the item that issued the command, but something else that is currently worn (object or clothing)
		
		EWearableType wearable_type = gAgent.mRRInterface.getOutfitLayerAsType (option);
		if (wearable_type != WT_INVALID) { // this is a clothing layer => replace item with the piece clothing
			LLUUID id = gAgent.getWearableItem (wearable_type);
			if (id.notNull()) item = gInventory.getItem(id);
			if (item != NULL && !gAgent.mRRInterface.isUnderRlvShare(item)) item = NULL; // security : we would return the path even if the item was not shared otherwise
		}
		else { // this is not a clothing layer => it has to be an attachment point
			LLViewerJointAttachment* attach_point = gAgent.mRRInterface.findAttachmentPointFromName (option, TRUE);
			if (attach_point) {
				std::deque<std::string> res;
				for (unsigned int i = 0; i < attach_point->mAttachedObjects.size(); ++i) {
					LLViewerObject* attached_object = attach_point->mAttachedObjects.at(i);
					if (attached_object) {
						item = getItemAux (attached_object, gAgent.mRRInterface.getRlvShare());
						if (item != NULL && !gAgent.mRRInterface.isUnderRlvShare(item)) item = NULL; // security : we would return the path even if the item was not shared otherwise
						else {
							// We have found the inventory item => add its path to the list
							// it appears to be a recursive call but the level of recursivity is only 2, we won't execute this instruction again in the called method since option will be empty
							res.push_back (getFullPath (item, ""));
							if (sRestrainedLoveDebug) {
								llinfos << "res=" << dumpList2String(res, ", ") << llendl;
							}
							if (!full_list) break; // old behaviour : we only return the first folder, not a full list
						}
					}
				}
				return dumpList2String (res, ",");
			}
		}
	}
	
	if (item != NULL && !gAgent.mRRInterface.isUnderRlvShare(item)) item = NULL; // security : we would return the path even if the item was not shared otherwise
	if (item == NULL) return "";
	LLUUID parent_id = item->getParentUUID();
	LLInventoryCategory* parent_cat = gInventory.getCategory (parent_id);
	
	if (item->getType() == LLAssetType::AT_OBJECT && !item->getPermissions().allowModifyBy(gAgent.getID())) {
		if (gAgent.mRRInterface.findAttachmentPointFromName(parent_cat->getName()) != NULL) {
			// this item is no-mod and its parent folder contains the name of an attach point
			// => probably we want the full path only to the containing folder of that folder
			parent_id = parent_cat->getParentUUID();
			parent_cat = gInventory.getCategory (parent_id);
			return getFullPath (parent_cat);
		}
	}
	
	return getFullPath (parent_cat);
}


LLInventoryItem* RRInterface::getItemAux (LLViewerObject* attached_object, LLInventoryCategory* root)
{
	// auxiliary function for getItem()
	if (!attached_object) return NULL;
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (root && avatar) {
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf (root->getUUID(), cats, items);
		S32 count;
		S32 i;
		LLInventoryItem* item = NULL;
		LLInventoryCategory* cat = NULL;
		
		// Try to find the item in the current category
		count = items->count();
		for(i = 0; i < count; ++i) {
			item = items->get(i);
			if (item && (item->getType() == LLAssetType::AT_OBJECT || item->getType() == LLAssetType::AT_CLOTHING) &&
				avatar->getWornAttachment (item->getUUID()) == attached_object)
			{
				// found the item in the current category
				return item;
			}
		}
		
		// We didn't find it here => browse the children categories
		count = cats->count();
		for(i = 0; i < count; ++i) {
			cat = cats->get(i);
			item = getItemAux (attached_object, cat);
			if (item != NULL) return item;
		}
	}
	// We didn't find it (this should not happen)
	return NULL;
}

LLInventoryItem* RRInterface::getItem (LLUUID wornObjectUuidInWorld)
{
	// return the inventory item corresponding to the viewer object which UUID is "wornObjectUuidInWorld", if any
	LLViewerObject* object = gObjectList.findObject (wornObjectUuidInWorld);
	if (object != NULL) {
		object = object->getRootEdit();
		if (object->isAttachment()) {
			return gInventory.getItem(object->getAttachmentItemID());
		}
	}
	// This object is not worn => it has nothing to do with any inventory item
	return NULL;
}

void RRInterface::attachObjectByUUID (LLUUID assetUUID, int attachPtNumber)
{
	// caution : this method does NOT check that the target attach point is already used by a locked item
	LLAttachmentRezAction* rez_action = new LLAttachmentRezAction;
	rez_action->mItemID = assetUUID;
	rez_action->mAttachPt = attachPtNumber;
	confirm_replace_attachment_rez(0/*YES*/, (void*)rez_action);
}

bool RRInterface::canDetachAllSelectedObjects ()
{
	for (LLObjectSelection::iterator iter = LLSelectMgr::getInstance()->getSelection()->begin();
		 iter != LLSelectMgr::getInstance()->getSelection()->end(); )
	{
		LLObjectSelection::iterator curiter = iter++;
		LLViewerObject* object = (*curiter)->getObject();
		if (object && !canDetach(object))
		{
			return false;
		}
	}
	return true;
}

bool RRInterface::isSittingOnAnySelectedObject()
{
	if (gAgent.getAvatarObject() && !gAgent.getAvatarObject()->mIsSitting) {
		return false;
	}
	
	for (LLObjectSelection::iterator iter = LLSelectMgr::getInstance()->getSelection()->begin();
		 iter != LLSelectMgr::getInstance()->getSelection()->end(); )
	{
		LLObjectSelection::iterator curiter = iter++;
		LLViewerObject* object = (*curiter)->getObject();
		if (object && object->isSeat())
		{
			return true;
		}
	}
	return false;
}

bool RRInterface::canAttachCategory(LLInventoryCategory* folder, bool with_exceptions /*= true*/)
{
	// return false if :
	// - at least one object issued a @attachthis:folder restriction
	// - at least one item in this folder is to be worn on a @attachthis:attachpt restriction
	// - at least one piece of clothing in this folder is to be worn on a @attachthis:layer restriction
	// - any parent folder returns false with @attachallthis
	if (!folder) return true;
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) return true;
	LLInventoryCategory* rlvShare = getRlvShare();
	if (!rlvShare || !isUnderRlvShare(folder)) {
		if (contains ("unsharedwear")) return false;
		else return true;
	}
	return canAttachCategoryAux(folder, false, false, with_exceptions);
}

bool RRInterface::canAttachCategoryAux(LLInventoryCategory* folder, bool in_parent, bool in_no_mod, bool with_exceptions /*= true*/)
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) return true;
	FolderLock folder_lock = FolderLock_unlocked;
	if (folder) {
		// check @attachthis:folder in all restrictions
		RRMAP::iterator it = mSpecialObjectBehaviours.begin ();
		//LLInventoryCategory* restricted_cat;
		std::string path_to_check;
		std::string restriction = "attachthis";
		if (in_parent) restriction = "attachallthis";
		folder_lock = isFolderLockedWithoutException(folder, "attach");
		if (folder_lock == FolderLock_locked_without_except) {
			return false;
		}

		if (!with_exceptions && folder_lock == FolderLock_locked_with_except) {
			return false;
		}

		//while (it != mSpecialObjectBehaviours.end()) {
		//	if (it->second.find (restriction+":") == 0) {
		//		path_to_check = it->second.substr (restriction.length()+1); // remove ":" as well
		//		restricted_cat = getCategoryUnderRlvShare(path_to_check);
		//		if (restricted_cat == folder) return false;
		//	}
		//	it++;
		//}

		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf (folder->getUUID(), cats, items);
		S32 count;
		S32 i;
		LLInventoryItem* item = NULL;
		LLInventoryCategory* cat = NULL;
		
		// Try to find the item in the current category
		count = items->count();
		for(i = 0; i < count; ++i) {
			item = items->get(i);
			if (item) {
				if (item->getType() == LLAssetType::AT_OBJECT) {
					LLViewerJointAttachment* attachpt = NULL;
					if (in_no_mod) {
						if (item->getPermissions().allowModifyBy(gAgent.getID()) || count > 1) return true;
						LLInventoryCategory* parent = gInventory.getCategory (folder->getParentUUID());
						attachpt = findAttachmentPointFromName(parent->getName());
					}
					else {
						attachpt = findAttachmentPointFromName(item->getName());
					}
					if (attachpt && contains (restriction + ":" + attachpt->getName())) return false;
				}
				else if (item->getType() == LLAssetType::AT_CLOTHING || item->getType() == LLAssetType::AT_BODYPART) {
					LLWearable* wearable = gAgent.getWearableFromWearableItem (item->getLinkedUUID());
					if (wearable) {
						if (contains(restriction + ":" + getOutfitLayerAsString(wearable->getType()))) return false;
					}
				}
			}
		}

		// now check all no-mod items => look at the sub-categories and return false if any of them returns false on a call to canAttachCategoryAux()
		count = cats->count();
		for(i = 0; i < count; ++i) {
			cat = cats->get(i);
			if (cat) {
				std::string name = cat->getName();
				if (name != "" && name[0] ==  '.' && findAttachmentPointFromName(name) != NULL) {
					if (!canAttachCategoryAux(cat, false, true, with_exceptions)) return false;
				}
			}
		}
	}
	if (folder == getRlvShare()) return true;
	if (!in_no_mod && folder_lock == FolderLock_unlocked) {
		return canAttachCategoryAux(gInventory.getCategory (folder->getParentUUID()), true, false, with_exceptions); // check for @attachallthis in the parent
	}
	return true;
}

bool RRInterface::canDetachCategory(LLInventoryCategory* folder, bool with_exceptions /*= true*/)
{
	// return false if :
	// - at least one object contained in this folder issued a @detachthis restriction
	// - at least one object issued a @detachthis:folder restriction
	// - at least one worn attachment in this folder is worn on a @detachthis:attachpt restriction
	// - at least one worn piece of clothing in this folder is worn on a @detachthis:layer restriction
	// - any parent folder returns false with @detachallthis
	if (!folder) return true;
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) return true;
	LLInventoryCategory* rlvShare = getRlvShare();
	if (!rlvShare || !isUnderRlvShare(folder)) {
		if (contains ("unsharedunwear")) return false;
		else return true;
	}
	return canDetachCategoryAux(folder, false, false, with_exceptions);
}

bool RRInterface::canDetachCategoryAux(LLInventoryCategory* folder, bool in_parent, bool in_no_mod, bool with_exceptions /*= true*/)
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar) return true;
	FolderLock folder_lock = FolderLock_unlocked;
	if (folder) {
		// check @detachthis:folder in all restrictions
		RRMAP::iterator it = mSpecialObjectBehaviours.begin ();
		//LLInventoryCategory* restricted_cat;
		std::string path_to_check;
		std::string restriction = "detachthis";
		if (in_parent) restriction = "detachallthis";
		folder_lock = isFolderLockedWithoutException(folder, "detach");
		if (folder_lock == FolderLock_locked_without_except) {
			return false;
		}

		if (!with_exceptions && folder_lock == FolderLock_locked_with_except) {
			return false;
		}

		//while (it != mSpecialObjectBehaviours.end()) {
		//	if (it->second.find (restriction+":") == 0) {
		//		path_to_check = it->second.substr (restriction.length()+1); // remove ":" as well
		//		restricted_cat = getCategoryUnderRlvShare(path_to_check);
		//		if (restricted_cat == folder) return false;
		//	}
		//	it++;
		//}

		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf (folder->getUUID(), cats, items);
		S32 count;
		S32 i;
		LLInventoryItem* item = NULL;
		LLInventoryCategory* cat = NULL;
		
		// Try to find the item in the current category
		count = items->count();
		for(i = 0; i < count; ++i) {
			item = items->get(i);
			if (item) {
				if (item->getType() == LLAssetType::AT_OBJECT) {
					if (in_no_mod) {
						if (item->getPermissions().allowModifyBy(gAgent.getID()) || count > 1) return true;
					}
					LLViewerObject* attached_object = avatar->getWornAttachment (item->getLinkedUUID());
					if (attached_object) {
						if (!isAllowed(attached_object->getRootEdit()->getID(), restriction)) return false;
						if (contains (restriction + ":" + avatar->getAttachedPointName(item->getLinkedUUID()))) return false;
					}
				}
				else if (item->getType() == LLAssetType::AT_CLOTHING || item->getType() == LLAssetType::AT_BODYPART) {
					LLWearable* wearable = gAgent.getWearableFromWearableItem (item->getLinkedUUID());
					if (wearable) {
						if (contains(restriction + ":" + getOutfitLayerAsString(wearable->getType()))) return false;
					}
				}
			}
		}

		// now check all no-mod items => look at the sub-categories and return false if any of them returns false on a call to canDetachCategoryAux()
		count = cats->count();
		for(i = 0; i < count; ++i) {
			cat = cats->get(i);
			if (cat) {
				std::string name = cat->getName();
				if (name != "" && name[0] ==  '.' && findAttachmentPointFromName(name) != NULL) {
					if (!canDetachCategoryAux(cat, false, true)) return false;
				}
			}
		}
	}
	if (folder == getRlvShare()) return true;
	if (!in_no_mod && folder_lock == FolderLock_unlocked) {
		return canDetachCategoryAux(gInventory.getCategory (folder->getParentUUID()), true, false, with_exceptions); // check for @detachallthis in the parent
	}
	return true;
}

bool RRInterface::canUnwear(LLViewerInventoryItem* item, BOOL handle_nostrip /* = TRUE */)
{
	if (item) {
		if (item->getType() ==  LLAssetType::AT_OBJECT) {
			return canDetach (item, handle_nostrip);
		}
		else if (item->getType() == LLAssetType::AT_CLOTHING || item->getType() == LLAssetType::AT_BODYPART) {
			EWearableType type = (EWearableType)(item->getFlags() & LLInventoryItem::II_FLAGS_WEARABLES_MASK);
			return canUnwear (type);
		}
	}
	return true;
}

bool RRInterface::canUnwear(EWearableType type, BOOL handle_nostrip /* = TRUE */)
{
	if (contains ("remoutfit")) {
		return false;
	}
	else if (contains ("remoutfit:" + getOutfitLayerAsString (type))) {
		return false;
	}
	LLInventoryItem* item = gAgent.getWearableInventoryItem(type);
	if (item) {
		LLInventoryCategory* cat_parent = gInventory.getCategory(item->getParentUUID());
		if (cat_parent && !canDetachCategory(cat_parent)) return false;
	}
	return true;
}

bool RRInterface::canWear(LLViewerInventoryItem* item, BOOL handle_nostrip /* = TRUE */)
{
	if (item) {
		LLInventoryCategory* parent = gInventory.getCategory (item->getParentUUID());
		if (item->getType() ==  LLAssetType::AT_OBJECT) {
			LLViewerJointAttachment* attachpt = findAttachmentPointFromName(item->getName());
			if (attachpt) {
				if (!canAttach (NULL, attachpt->getName())) return false;
			}
			return canAttachCategory (parent);
		}
		else if (item->getType() == LLAssetType::AT_CLOTHING || item->getType() == LLAssetType::AT_BODYPART) {
			EWearableType type = (EWearableType)(item->getFlags() & LLInventoryItem::II_FLAGS_WEARABLES_MASK);
			if (!canWear (type)) return false;
			if (!canAttachCategory (parent)) return false;
		}
	}
	return true;
}

bool is_cloud()
{
	LLVOAvatar* avatarp = gAgent.getAvatarObject();
	if (!avatarp) return true;
	if (avatarp->visualParamWeightsAreDefault()) return true;
	if (!avatarp->isTextureDefined(LLVOAvatar::TEX_HAIR) ||
		!avatarp->isTextureDefined(LLVOAvatar::TEX_LOWER_BAKED) ||
		!avatarp->isTextureDefined(LLVOAvatar::TEX_UPPER_BAKED) ||
		!avatarp->isTextureDefined(LLVOAvatar::TEX_HEAD_BAKED)) {
		return true;
	}
	return false;
}

bool RRInterface::canWear(EWearableType type, bool from_server /*= false*/)
{
	// If we are still a cloud, we can always wear bodyparts because we are still logging on
	if (is_cloud())
	{
		if (type == WT_SHAPE || type == WT_HAIR || type == WT_EYES || type == WT_SKIN) {
			return true;
		}
	}
	// If from_server is true, return true because we are probably trying to wear our bodyparts while logging on
	else if (from_server) {
		return true;
	}
	else if (contains ("addoutfit")) {
		return false;
	}
	else if (contains ("addoutfit:"+getOutfitLayerAsString (type))) {
		return false;
	}
	return true;
}

bool RRInterface::canDetach(LLViewerInventoryItem* item, BOOL handle_nostrip /* = TRUE */)
{
	if (item == NULL) return true;
	LLVOAvatar* avatarp = gAgent.getAvatarObject();
	if (!avatarp) return true;

	if (handle_nostrip) {
		std::string name = item->getName();
		LLStringUtil::toLower(name);
		if (name.find ("nostrip") != -1) return false;
	}

	if (item->getType() == LLAssetType::AT_OBJECT) {
		// we'll check canDetachCategory() inside this function
		return canDetach (avatarp->getWornAttachment(item->getLinkedUUID()), handle_nostrip);
	}
	else if (item->getType() == LLAssetType::AT_CLOTHING) {
		LLWearable* wearable = gAgent.getWearableFromWearableItem (item->getLinkedUUID());
		if (wearable) return canUnwear (wearable->getType());
		return true;
	}
	return true;
}

bool RRInterface::canDetach(LLViewerObject* attached_object, BOOL handle_nostrip /* = TRUE */)
{
	if (attached_object == NULL) return true;
	if (attached_object->getRootEdit() == NULL) return true;
	
	if (!isAllowed (attached_object->getRootEdit()->getID(), "detach", FALSE)) return false;

	LLInventoryItem* item = getItem (attached_object->getRootEdit()->getID());
	if (item) {
		LLInventoryCategory* cat_parent = gInventory.getCategory (item->getParentUUID());
		if (cat_parent && !canDetachCategory(cat_parent)) return false;

		if (handle_nostrip) {
			std::string name = item->getName();
			LLStringUtil::toLower(name);
			if (name.find ("nostrip") != -1) return false;
		}

		LLVOAvatar* avatarp = gAgent.getAvatarObject();
		if (avatarp) {
			std::string attachpt = avatarp->getAttachedPointName(item->getLinkedUUID());
			if (contains("detach:" + attachpt)) return false;
			if (contains("remattach")) return false;
			if (contains("remattach:" + attachpt)) return false;
		}
	}
	return true;
}

bool RRInterface::canDetach(std::string attachpt, BOOL handle_nostrip /* = TRUE */)
{
	LLStringUtil::toLower(attachpt);
	if (contains("detach:" + attachpt)) return false;
	if (contains("remattach")) return false;
	if (contains("remattach:" + attachpt)) return false;
	LLViewerJointAttachment* attachment = findAttachmentPointFromName (attachpt, TRUE);
	if (!canDetachAllObjectsFromAttachment (attachment, handle_nostrip)) return false;
	return true;
}

bool RRInterface::canAttach(LLViewerObject* object_to_attach, std::string attachpt, bool from_server /* = false */)
{
	LLStringUtil::toLower(attachpt);
	if (contains("addattach")) return false;
	if (contains("addattach:" + attachpt)) return false;
	if (object_to_attach) {
		LLInventoryItem* item = getItem(object_to_attach->getRootEdit()->getID());
		if (item) {
			LLInventoryCategory* cat_parent = gInventory.getCategory (item->getParentUUID());
			if (cat_parent && !canAttachCategory(cat_parent)) return false;
		}
	}

	return true;
}

bool RRInterface::canAttach(LLViewerInventoryItem* item)
{
	if (contains("addattach")) return false;
	if (!item) return true;
	LLViewerJointAttachment* attachpt = findAttachmentPointFromName (item->getName());
	if (attachpt && contains("addattach:" + attachpt->getName())) return false;
	LLInventoryCategory* cat_parent = gInventory.getCategory (item->getParentUUID());
	if (cat_parent && !canAttachCategory(cat_parent)) return false;
	return true;
}

bool RRInterface::canEdit(LLViewerObject* object)
{
	if (!object || !object->getRootEdit()) return false;
	if (containsWithoutException ("edit", object->getRootEdit()->getID().asString())) return false;
	if (contains ("editobj:" + object->getRootEdit()->getID().asString())) return false;
	return true;
}

bool RRInterface::canTouch(LLViewerObject* object, LLVector3 pick_intersection /* = LLVector3::zero */)
{
	if (!object) return true;

	LLViewerObject* root = object->getRootEdit();
	if (!root) return true;

	if (!root->isHUDAttachment() && contains ("touchall")) return false;
	if (contains ("touchthis:"+object->getRootEdit()->getID().asString())) return false;

	if (!isAllowed (object->getRootEdit()->getID(), "touchme")) return true; // to check the presence of "touchme" on this object, which means that we can touch it

	if (!canTouchFar (object, pick_intersection)) return false;

	if (root->isAttachment()) {
		if (!root->isHUDAttachment()) {
			if (contains ("touchattach")) return false;

			LLInventoryItem* inv_item = getItem (root->getID());
			if (inv_item) { // this attachment is in my inv => it belongs to me
				if (contains ("touchattachself")) {
					return false;
				}
			}
			else { // this attachment is not in my inv => it does not belong to me
				if (contains ("touchattachother")) {
					return false;
				}
			}
		}
	}
	else {
		if (containsWithoutException ("touchworld", object->getRootEdit()->getID().asString())) return false;
	}
	return true;
}

bool RRInterface::canTouchFar(LLViewerObject* object, LLVector3 pick_intersection /* = LLVector3::zero */)
{
	if (!object) return true;
	if (object->isHUDAttachment()) return true;

	LLVector3 pos = object->getPositionRegion ();
	if (pick_intersection != LLVector3::zero) pos = pick_intersection;
	pos -= gAgent.getPositionAgent ();
	F32 dist = pos.magVec();
	if (mContainsFartouch && dist >= 1.5) return false;

	return true;
}
