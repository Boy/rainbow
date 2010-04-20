/** 
 * @file RRInterface.h
 * @author Marine Kelley
 * @brief The header for all RLV features
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

#ifndef LL_RRINTERFACE_H
#define LL_RRINTERFACE_H

#define RR_VIEWER_NAME "RestrainedLife"
#define RR_VIEWER_NAME_NEW "RestrainedLove"
#define RR_VERSION_NUM "1230000"
#define RR_VERSION "1.23.0"
#define RR_SLV_VERSION "RV 1.22.12.6"

#define RR_PREFIX "@"
#define RR_SHARED_FOLDER "#RLV"
#define RR_RLV_REDIR_FOLDER_PREFIX "#RLV/~"
// Length of the "#RLV/" string constant in characters.
#define RR_HRLVS_LENGTH 5

// wearable types as strings
#define WS_ALL "all"
#define WS_EYES "eyes"
#define WS_SKIN "skin"
#define WS_SHAPE "shape"
#define WS_HAIR "hair"
#define WS_GLOVES "gloves"
#define WS_JACKET "jacket"
#define WS_PANTS "pants"
#define WS_SHIRT "shirt"
#define WS_SHOES "shoes"
#define WS_SKIRT "skirt"
#define WS_SOCKS "socks"
#define WS_UNDERPANTS "underpants"
#define WS_UNDERSHIRT "undershirt"


//#include <set>
#include <deque>
#include <map>
#include <string>

#include "lluuid.h"
#include "llchat.h"
#include "llchatbar.h"
#include "llinventorymodel.h"
#include "llviewermenu.h"
#include "llwearable.h"

typedef std::multimap<std::string, std::string> RRMAP;
typedef struct Command {
	LLUUID uuid;
	std::string command;
} Command;

typedef struct AssetAndTarget {
	LLUUID uuid;
	std::string attachpt;
} AssetAndTarget;

class RRInterface
{
public:
	
	RRInterface ();
	~RRInterface ();

	std::string getVersion (); // returns "RestrainedLife Viewer blah blah"
	std::string getVersion2 (); // returns "RestrainedLove Viewer blah blah"
	BOOL isAllowed (LLUUID object_uuid, std::string action, BOOL log_it = TRUE);
	BOOL contains (std::string action); // return TRUE if the action is contained
	BOOL containsSubstr (std::string action);
	BOOL containsWithoutException (std::string action, std::string except = ""); // return TRUE if the action or action+"_sec" is contained, and either there is no global exception, or there is no local exception if we found action+"_sec"

	BOOL add (LLUUID object_uuid, std::string action, std::string option);
	BOOL remove (LLUUID object_uuid, std::string action, std::string option);
	BOOL clear (LLUUID object_uuid, std::string command="");
	void replace (LLUUID what, LLUUID by);
	BOOL garbageCollector (BOOL all=TRUE); // if false, don't clear rules attached to NULL_KEY as they are issued from external objects (only cleared when changing parcel)
	std::deque<std::string> parse (std::string str, std::string sep); // utility function
	void notify (LLUUID object_uuid, std::string action, std::string suffix); // scan the list of restrictions, when finding "notify" say the action on the specified channel

	BOOL parseCommand (std::string command, std::string& behaviour, std::string& option, std::string& param);
	BOOL handleCommand (LLUUID uuid, std::string command);
	BOOL fireCommands (); // execute commands buffered while the viewer was initializing (mostly useful for force-sit as when the command is sent the object is not necessarily rezzed yet)
	BOOL force (LLUUID object_uuid, std::string command, std::string option);

	BOOL answerOnChat (std::string channel, std::string msg);
	std::string crunchEmote (std::string msg, unsigned int truncateTo);

	std::string getOutfitLayerAsString (EWearableType layer);
	EWearableType getOutfitLayerAsType (std::string layer);
	std::string getOutfit (std::string layer);
	std::string getAttachments (std::string attachpt);

	std::string getStatus (LLUUID object_uuid, std::string rule); // if object_uuid is null, return all
	BOOL forceDetach (std::string attachpt);
	BOOL forceDetachByUuid (std::string object_uuid);

	BOOL hasLockedHuds ();
	std::deque<LLInventoryItem*> getListOfLockedItems (LLInventoryCategory* root);
	std::string getInventoryList (std::string path, BOOL withWornInfo = FALSE);
	std::string getWornItems (LLInventoryCategory* cat);
	LLInventoryCategory* getRlvShare (); // return pointer to #RLV folder or null if does not exist
	BOOL isUnderRlvShare (LLInventoryItem* item);
	void renameAttachment (LLInventoryItem* item, LLViewerJointAttachment* attachment); // DEPRECATED
	LLInventoryCategory* getCategoryUnderRlvShare (std::string catName, LLInventoryCategory* root = NULL);
	LLInventoryCategory* findCategoryUnderRlvShare (std::string catName, LLInventoryCategory* root = NULL);
	std::string findAttachmentNameFromPoint (LLViewerJointAttachment* attachpt);
	LLViewerJointAttachment* findAttachmentPointFromName (std::string objectName, BOOL exactName = FALSE);
	LLViewerJointAttachment* findAttachmentPointFromParentName (LLInventoryItem* item);
	S32 findAttachmentPointNumber (LLViewerJointAttachment* attachment);
	void fetchInventory (LLInventoryCategory* root = NULL);

	BOOL forceAttach (std::string category, BOOL recursive = FALSE);
	BOOL forceDetachByName (std::string category, BOOL recursive = FALSE);

	BOOL getAllowCancelTp() { return sAllowCancelTp; }
	void setAllowCancelTp(BOOL newval) { sAllowCancelTp = newval; }

	std::string getParcelName () { return sParcelName; }
	void setParcelName (std::string newval) { sParcelName = newval; }

	BOOL forceTeleport (std::string location);

	std::string stringReplace (std::string s, std::string what, std::string by, BOOL caseSensitive = FALSE);

	std::string getDummyName (std::string name, EChatAudible audible = CHAT_AUDIBLE_FULLY); // return "someone", "unknown" etc according to the length of the name (when shownames is on)
	std::string getCensoredMessage (std::string str); // replace names by dummy names

	LLUUID getSitTargetId () { return sSitTargetId; }
	void setSitTargetId (LLUUID newval) { sSitTargetId = newval; }

	BOOL forceEnvironment (std::string command, std::string option); // command is "setenv_<something>", option is a list of floats (separated by "/")
	std::string getEnvironment (std::string command); // command is "getenv_<something>"
	
	std::string getLastLoadedPreset () { return sLastLoadedPreset; }
	void setLastLoadedPreset (std::string newval) { sLastLoadedPreset = newval; }

	BOOL forceDebugSetting (std::string command, std::string option); // command is "setdebug_<something>", option is a list of values (separated by "/")
	std::string getDebugSetting (std::string command); // command is "getdebug_<something>"

	std::string getFullPath (LLInventoryCategory* cat);
	std::string getFullPath (LLInventoryItem* item, std::string option = "");
	LLInventoryItem* getItemAux (LLViewerObject* attached_object, LLInventoryCategory* root);
	LLInventoryItem* getItem (LLUUID wornObjectUuidInWorld);
	void attachObjectByUUID (LLUUID assetUUID, int attachPtNumber = 0);
	
	bool canDetachAllSelectedObjects();
	bool isSittingOnAnySelectedObject();
	
	bool canDetach(LLViewerObject* attached_object);
	bool canDetach(std::string attachpt);
	bool canAttach(LLViewerObject* object_to_attach, std::string attachpt);
	
	// Some cache variables to accelerate common checks
	BOOL mHasLockedHuds;
	BOOL mContainsDetach;
	BOOL mContainsShowinv;
	BOOL mContainsUnsit;
	BOOL mContainsFartouch;
	BOOL mContainsShowworldmap;
	BOOL mContainsShowminimap;
	BOOL mContainsShowloc;
	BOOL mContainsShownames;
	BOOL mContainsSetenv;
	BOOL mContainsFly;
	BOOL mContainsEdit;
	BOOL mContainsRez;
	BOOL mContainsShowhovertextall;
	BOOL mContainsShowhovertexthud;
	BOOL mContainsShowhovertextworld;
	BOOL mContainsDefaultwear;
	BOOL mContainsPermissive;

	// Allowed debug settings (initialized in the ctor)
	std::string sAllowedU32;
	std::string sAllowedS32;
	std::string sAllowedF32;
	std::string sAllowedBOOLEAN;
	std::string sAllowedSTRING;
	std::string sAllowedVEC3;
	std::string sAllowedVEC3D;
	std::string sAllowedRECT;
	std::string sAllowedCOL4;
	std::string sAllowedCOL3;
	std::string sAllowedCOL4U;
	
	// These should be private but we may want to browse them from the outside world, so let's keep them public
	RRMAP sSpecialObjectBehaviours;
	std::deque<Command> sRetainedCommands;

	// When a locked attachment is kicked off by another one with llAttachToAvatar() in a script, retain its UUID here, to reattach it later 
	std::deque<AssetAndTarget> sAssetsToReattach;
	int sTimeBeforeReattaching;
	AssetAndTarget sJustDetached; // we need this to inhibit the removeObject event that occurs right after addObject in the case of a replacement
	AssetAndTarget sJustReattached; // we need this to inhibit the removeObject event that occurs right after addObject in the case of a replacement

private:
	std::string sParcelName; // for convenience (gAgent does not retain the name of the current parcel)
	BOOL sInventoryFetched; // FALSE at first, used to fetch RL Share inventory once upon login
	BOOL sAllowCancelTp; // TRUE unless forced to TP with @tpto (=> receive TP order from server, act like it is a lure from a Linden => don't show the cancel button)
	LLUUID sSitTargetId;
	std::string sLastLoadedPreset; // contains the name of the latest loaded Windlight preset
	
};


#endif
