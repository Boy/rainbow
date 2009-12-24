/** 
 * @file llviewerobjectlist.cpp
 * @brief Implementation of LLViewerObjectList class.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llviewerobjectlist.h"

#include "message.h"
#include "timing.h"
#include "llfasttimer.h"
#include "llrender.h"

#include "llviewercontrol.h"
#include "llface.h"
#include "llvoavatar.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"
#include "llnetmap.h"
#include "llagent.h"
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llhoverview.h"
#include "llworld.h"
#include "llstring.h"
#include "llhudtext.h"
#include "lldrawable.h"
#include "xform.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llselectmgr.h"
#include "llresmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "llkeyboard.h"
#include "u64.h"
#include "llviewerimagelist.h"
#include "lldatapacker.h"
#ifdef LL_STANDALONE
#include <zlib.h>
#else
#include "zlib/zlib.h"
#endif
#include "object_flags.h"

#include "llappviewer.h"

#include "primbackup.h"

extern F32 gMinObjectDistance;
extern BOOL gAnimateTextures;

void dialog_refresh_all();

#define CULL_VIS
//#define ORPHAN_SPAM
//#define IGNORE_DEAD

// Global lists of objects - should go away soon.
LLViewerObjectList gObjectList;

extern LLPipeline	gPipeline;

// Statics for object lookup tables.
U32						LLViewerObjectList::sSimulatorMachineIndex = 1; // Not zero deliberately, to speed up index check.
LLMap<U64, U32>			LLViewerObjectList::sIPAndPortToIndex;
std::map<U64, LLUUID>	LLViewerObjectList::sIndexAndLocalIDToUUID;

LLViewerObjectList::LLViewerObjectList()
{
	mNumVisCulled = 0;
	mNumSizeCulled = 0;
	mCurLazyUpdateIndex = 0;
	mCurBin = 0;
	mNumDeadObjects = 0;
	mNumOrphans = 0;
	mNumNewObjects = 0;
	mWasPaused = FALSE;
	mNumDeadObjectUpdates = 0;
	mNumUnknownKills = 0;
	mNumUnknownUpdates = 0;
}

LLViewerObjectList::~LLViewerObjectList()
{
	destroy();
}

void LLViewerObjectList::destroy()
{
	killAllObjects();

	resetObjectBeacons();
	mActiveObjects.clear();
	mDeadObjects.clear();
	mMapObjects.clear();
	mUUIDObjectMap.clear();
}


void LLViewerObjectList::getUUIDFromLocal(LLUUID &id,
										  const U32 local_id,
										  const U32 ip,
										  const U32 port)
{
	U64 ipport = (((U64)ip) << 32) | (U64)port;

	U32 index = sIPAndPortToIndex[ipport];

	if (!index)
	{
		index = sSimulatorMachineIndex++;
		sIPAndPortToIndex[ipport] = index;
	}

	U64	indexid = (((U64)index) << 32) | (U64)local_id;

	id = get_if_there(sIndexAndLocalIDToUUID, indexid, LLUUID::null);
}

U64 LLViewerObjectList::getIndex(const U32 local_id,
								 const U32 ip,
								 const U32 port)
{
	U64 ipport = (((U64)ip) << 32) | (U64)port;

	U32 index = sIPAndPortToIndex[ipport];

	if (!index)
	{
		return 0;
	}

	return (((U64)index) << 32) | (U64)local_id;
}

BOOL LLViewerObjectList::removeFromLocalIDTable(const LLViewerObject &object)
{
	if(object.getRegion())
	{
		U32 local_id = object.mLocalID;
		LLHost region_host = object.getRegion()->getHost();
		U32 ip = region_host.getAddress();
		U32 port = region_host.getPort();
		U64 ipport = (((U64)ip) << 32) | (U64)port;
		U32 index = sIPAndPortToIndex[ipport];

		U64	indexid = (((U64)index) << 32) | (U64)local_id;
		return sIndexAndLocalIDToUUID.erase(indexid) > 0 ? TRUE : FALSE;
	}

	return FALSE ;
}

void LLViewerObjectList::setUUIDAndLocal(const LLUUID &id,
										  const U32 local_id,
										  const U32 ip,
										  const U32 port)
{
	U64 ipport = (((U64)ip) << 32) | (U64)port;

	U32 index = sIPAndPortToIndex[ipport];

	if (!index)
	{
		index = sSimulatorMachineIndex++;
		sIPAndPortToIndex[ipport] = index;
	}

	U64	indexid = (((U64)index) << 32) | (U64)local_id;

	sIndexAndLocalIDToUUID[indexid] = id;
}

S32 gFullObjectUpdates = 0;
S32 gTerseObjectUpdates = 0;

void LLViewerObjectList::processUpdateCore(LLViewerObject* objectp, 
										   void** user_data, 
										   U32 i, 
										   const EObjectUpdateType update_type, 
										   LLDataPacker* dpp, 
										   BOOL just_created)
{
	LLMessageSystem* msg = gMessageSystem;

	// ignore returned flags
	objectp->processUpdateMessage(msg, user_data, i, update_type, dpp);
		
	if (objectp->isDead())
	{
		// The update failed
		return;
	}

	updateActive(objectp);

	if (just_created) 
	{
		gPipeline.addObject(objectp);
	}
	else
	{
		primbackup::getInstance()->prim_update(objectp);
	}

	// Also sets the approx. pixel area
	objectp->setPixelAreaAndAngle(gAgent);

	// RN: this must be called after we have a drawable 
	// (from gPipeline.addObject)
	// so that the drawable parent is set properly
	findOrphans(objectp, msg->getSenderIP(), msg->getSenderPort());
	
	// If we're just wandering around, don't create new objects selected.
	if (just_created 
		&& update_type != OUT_TERSE_IMPROVED 
		&& objectp->mCreateSelected)
	{
		if ( LLToolMgr::getInstance()->getCurrentTool() != LLToolPie::getInstance() )
		{
			//llinfos << "DEBUG selecting " << objectp->mID << " " 
			//		<< objectp->mLocalID << llendl;
			LLSelectMgr::getInstance()->selectObjectAndFamily(objectp);
			dialog_refresh_all();
		}

		objectp->mCreateSelected = false;
		gViewerWindow->getWindow()->decBusyCount();
		gViewerWindow->getWindow()->setCursor( UI_CURSOR_ARROW );

		primbackup::getInstance()->newprim(objectp);
	}
}

void LLViewerObjectList::processObjectUpdate(LLMessageSystem *mesgsys,
											 void **user_data,
											 const EObjectUpdateType update_type,
											 bool cached, bool compressed)
{
	LLFastTimer t(LLFastTimer::FTM_PROCESS_OBJECTS);	
	
	LLVector3d camera_global = gAgent.getCameraPositionGlobal();
	LLViewerObject *objectp;
	S32			num_objects;
	U32			local_id;
	LLPCode		pcode = 0;
	LLUUID		fullid;
	S32			i;

	// figure out which simulator these are from and get it's index
	// Coordinates in simulators are region-local
	// Until we get region-locality working on viewer we
	// have to transform to absolute coordinates.
	num_objects = mesgsys->getNumberOfBlocksFast(_PREHASH_ObjectData);

	if (!cached && !compressed && update_type != OUT_FULL)
	{
		gTerseObjectUpdates += num_objects;
		S32 size;
		if (mesgsys->getReceiveCompressedSize())
		{
			size = mesgsys->getReceiveCompressedSize();
		}
		else
		{
			size = mesgsys->getReceiveSize();
		}
//		llinfos << "Received terse " << num_objects << " in " << size << " byte (" << size/num_objects << ")" << llendl;
	}
	else
	{
		S32 size;
		if (mesgsys->getReceiveCompressedSize())
		{
			size = mesgsys->getReceiveCompressedSize();
		}
		else
		{
			size = mesgsys->getReceiveSize();
		}

//		llinfos << "Received " << num_objects << " in " << size << " byte (" << size/num_objects << ")" << llendl;
		gFullObjectUpdates += num_objects;
	}

	U64 region_handle;
	mesgsys->getU64Fast(_PREHASH_RegionData, _PREHASH_RegionHandle, region_handle);
	LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromHandle(region_handle);

	if (!regionp)
	{
		llwarns << "Object update from unknown region!" << llendl;
		return;
	}

	U8 compressed_dpbuffer[2048];
	LLDataPackerBinaryBuffer compressed_dp(compressed_dpbuffer, 2048);
	LLDataPacker *cached_dpp = NULL;
	
	for (i = 0; i < num_objects; i++)
	{
		LLTimer update_timer;
		BOOL justCreated = FALSE;

		if (cached)
		{
			U32 id;
			U32 crc;
			mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, id, i);
			mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_CRC, crc, i);
		
			// Lookup data packer and add this id to cache miss lists if necessary.
			cached_dpp = regionp->getDP(id, crc);
			if (cached_dpp)
			{
				cached_dpp->reset();
				cached_dpp->unpackUUID(fullid, "ID");
				cached_dpp->unpackU32(local_id, "LocalID");
				cached_dpp->unpackU8(pcode, "PCode");
			}
			else
			{
				continue; // no data packer, skip this object
			}
		}
		else if (compressed)
		{
			U8							compbuffer[2048];
			S32							uncompressed_length = 2048;
			S32							compressed_length;
			
			compressed_dp.reset();

			U32 flags = 0;
			if (update_type != OUT_TERSE_IMPROVED)
			{
				mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_UpdateFlags, flags, i);
			}
			
			if (flags & FLAGS_ZLIB_COMPRESSED)
			{
				compressed_length = mesgsys->getSizeFast(_PREHASH_ObjectData, i, _PREHASH_Data);
				mesgsys->getBinaryDataFast(_PREHASH_ObjectData, _PREHASH_Data, compbuffer, 0, i);
				uncompressed_length = 2048;
				uncompress(compressed_dpbuffer, (unsigned long *)&uncompressed_length,
						   compbuffer, compressed_length);
				compressed_dp.assignBuffer(compressed_dpbuffer, uncompressed_length);
			}
			else
			{
				uncompressed_length = mesgsys->getSizeFast(_PREHASH_ObjectData, i, _PREHASH_Data);
				mesgsys->getBinaryDataFast(_PREHASH_ObjectData, _PREHASH_Data, compressed_dpbuffer, 0, i);
				compressed_dp.assignBuffer(compressed_dpbuffer, uncompressed_length);
			}


			if (update_type != OUT_TERSE_IMPROVED)
			{
				compressed_dp.unpackUUID(fullid, "ID");
				compressed_dp.unpackU32(local_id, "LocalID");
				compressed_dp.unpackU8(pcode, "PCode");
			}
			else
			{
				compressed_dp.unpackU32(local_id, "LocalID");
				getUUIDFromLocal(fullid,
								 local_id,
								 gMessageSystem->getSenderIP(),
								 gMessageSystem->getSenderPort());
				if (fullid.isNull())
				{
					//llwarns << "update for unknown localid " << local_id << " host " << gMessageSystem->getSender() << llendl;
					mNumUnknownUpdates++;
				}
			}
		}
		else if (update_type != OUT_FULL)
		{
			mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);
			getUUIDFromLocal(fullid,
							local_id,
							gMessageSystem->getSenderIP(),
							gMessageSystem->getSenderPort());
			if (fullid.isNull())
			{
				//llwarns << "update for unknown localid " << local_id << " host " << gMessageSystem->getSender() << llendl;
				mNumUnknownUpdates++;
			}
		}
		else
		{
			mesgsys->getUUIDFast(_PREHASH_ObjectData, _PREHASH_FullID, fullid, i);
			mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);
		//	llinfos << "Full Update, obj " << local_id << ", global ID" << fullid << "from " << mesgsys->getSender() << llendl;
		}
		objectp = findObject(fullid);

		// This looks like it will break if the local_id of the object doesn't change
		// upon boundary crossing, but we check for region id matching later...
		if (objectp && (objectp->mLocalID != local_id))
		{
			removeFromLocalIDTable(*objectp);
			setUUIDAndLocal(fullid,
							local_id,
							gMessageSystem->getSenderIP(),
							gMessageSystem->getSenderPort());
		}

		if (!objectp)
		{
			if (compressed)
			{
				if (update_type == OUT_TERSE_IMPROVED)
				{
					//	llinfos << "terse update for an unknown object:" << fullid << llendl;
					continue;
				}
			}
			else if (cached)
			{
			}
			else
			{
				if (update_type != OUT_FULL)
				{
// 					llinfos << "terse update for an unknown object:" << fullid << llendl;
					continue;
				}

				mesgsys->getU8Fast(_PREHASH_ObjectData, _PREHASH_PCode, pcode, i);
			}
#ifdef IGNORE_DEAD
			if (mDeadObjects.find(fullid) != mDeadObjects.end())
			{
				mNumDeadObjectUpdates++;
				//llinfos << "update for a dead object:" << fullid << llendl;
				continue;
			}
#endif

			objectp = createObject(pcode, regionp, fullid, local_id, gMessageSystem->getSender());
			if (!objectp)
			{
				continue;
			}
			justCreated = TRUE;
			mNumNewObjects++;
		}
		else
		{
			if (objectp->getRegion() != regionp)
			{
				// Object has changed region!  Update lookup tables, set region pointer.
				removeFromLocalIDTable(*objectp);
				setUUIDAndLocal(fullid,
								local_id,
								gMessageSystem->getSenderIP(),
								gMessageSystem->getSenderPort());
				objectp->setRegion(regionp);
			}
			objectp->updateRegion(regionp); // for LLVOAvatar
		}


		if (objectp->isDead())
		{
			llwarns << "Dead object " << objectp->mID << " in UUID map 1!" << llendl;
		}

		if (compressed)
		{
			if (update_type != OUT_TERSE_IMPROVED)
			{
				objectp->mLocalID = local_id;
			}
			processUpdateCore(objectp, user_data, i, update_type, &compressed_dp, justCreated);
			if (update_type != OUT_TERSE_IMPROVED)
			{
				objectp->mRegionp->cacheFullUpdate(objectp, compressed_dp);
			}
		}
		else if (cached)
		{
			objectp->mLocalID = local_id;
			processUpdateCore(objectp, user_data, i, update_type, cached_dpp, justCreated);
		}
		else
		{
			if (update_type == OUT_FULL)
			{
				objectp->mLocalID = local_id;
			}
			processUpdateCore(objectp, user_data, i, update_type, NULL, justCreated);
		}
	}

	LLVOAvatar::cullAvatarsByPixelArea();
}

void LLViewerObjectList::processCompressedObjectUpdate(LLMessageSystem *mesgsys,
											 void **user_data,
											 const EObjectUpdateType update_type)
{
	processObjectUpdate(mesgsys, user_data, update_type, false, true);
}

void LLViewerObjectList::processCachedObjectUpdate(LLMessageSystem *mesgsys,
											 void **user_data,
											 const EObjectUpdateType update_type)
{
	processObjectUpdate(mesgsys, user_data, update_type, true, false);
}	

void LLViewerObjectList::dirtyAllObjectInventory()
{
	S32 count = mObjects.count();
	for(S32 i = 0; i < count; ++i)
	{
		mObjects[i]->dirtyInventory();
	}
}

void LLViewerObjectList::updateApparentAngles(LLAgent &agent)
{
	S32 i;
	S32 num_objects = 0;
	LLViewerObject *objectp;

	S32 num_updates, max_value;
	if (NUM_BINS - 1 == mCurBin)
	{
		num_updates = mObjects.count() - mCurLazyUpdateIndex;
		max_value = mObjects.count();
		gImageList.setUpdateStats(TRUE);
	}
	else
	{
		num_updates = (mObjects.count() / NUM_BINS) + 1;
		max_value = llmin(mObjects.count(), mCurLazyUpdateIndex + num_updates);
	}


	if (!gNoRender)
	{
		// Slam priorities for textures that we care about (hovered, selected, and focused)
		// Hovered
		// Assumes only one level deep of parenting
		objectp = gHoverView->getLastHoverObject();
		if (objectp)
		{
			objectp->boostTexturePriority();
		}
	}

	// Focused
	objectp = gAgent.getFocusObject();
	if (objectp)
	{
		objectp->boostTexturePriority();
	}

	// Selected
	struct f : public LLSelectedObjectFunctor
	{
		virtual bool apply(LLViewerObject* objectp)
		{
			objectp->boostTexturePriority();
			return true;
		}
	} func;
	LLSelectMgr::getInstance()->getSelection()->applyToRootObjects(&func);

	// Iterate through some of the objects and lazy update their texture priorities
	for (i = mCurLazyUpdateIndex; i < max_value; i++)
	{
		objectp = mObjects[i];
		if (!objectp->isDead())
		{
			num_objects++;

			//  Update distance & gpw 
			objectp->setPixelAreaAndAngle(agent); // Also sets the approx. pixel area
			objectp->updateTextures(agent);	// Update the image levels of textures for this object.
		}
	}

	mCurLazyUpdateIndex = max_value;
	if (mCurLazyUpdateIndex == mObjects.count())
	{
		mCurLazyUpdateIndex = 0;
	}

	mCurBin = (++mCurBin) % NUM_BINS;

	LLVOAvatar::cullAvatarsByPixelArea();
}


void LLViewerObjectList::update(LLAgent &agent, LLWorld &world)
{
	LLMemType mt(LLMemType::MTYPE_OBJECT);
	// Update globals
	gVelocityInterpolate = gSavedSettings.getBOOL("VelocityInterpolate");
	gPingInterpolate = gSavedSettings.getBOOL("PingInterpolate");
	gAnimateTextures = gSavedSettings.getBOOL("AnimateTextures");

	// update global timer
	F32 last_time = gFrameTimeSeconds;
	U64 time = totalTime();                 // this will become the new gFrameTime when the update is done
	// Time _can_ go backwards, for example if the user changes the system clock.
	// It doesn't cause any fatal problems (just some oddness with stats), so we shouldn't assert here.
//	llassert(time > gFrameTime);
	F64 time_diff = U64_to_F64(time - gFrameTime)/(F64)SEC_TO_MICROSEC;
	gFrameTime    = time;
	F64 time_since_start = U64_to_F64(gFrameTime - gStartTime)/(F64)SEC_TO_MICROSEC;
	gFrameTimeSeconds = (F32)time_since_start;

	gFrameIntervalSeconds = gFrameTimeSeconds - last_time;
	if (gFrameIntervalSeconds < 0.f)
	{
		gFrameIntervalSeconds = 0.f;
	}

	//clear avatar LOD change counter
	LLVOAvatar::sNumLODChangesThisFrame = 0;

	const F64 frame_time = LLFrameTimer::getElapsedSeconds();
	
	std::vector<LLViewerObject*> kill_list;
	S32 num_active_objects = 0;
	LLViewerObject *objectp = NULL;	
	
	// Make a copy of the list in case something in idleUpdate() messes with it
	std::vector<LLViewerObject*> idle_list;
	idle_list.reserve( mActiveObjects.size() );

 	for (std::set<LLPointer<LLViewerObject> >::iterator active_iter = mActiveObjects.begin();
		active_iter != mActiveObjects.end(); active_iter++)
	{
		objectp = *active_iter;
		if (objectp)
		{
			idle_list.push_back( objectp );
		}
		else
		{	// There shouldn't be any NULL pointers in the list, but they have caused
			// crashes before.  This may be idleUpdate() messing with the list.
			llwarns << "LLViewerObjectList::update has a NULL objectp" << llendl;
		}
	}

	if (gSavedSettings.getBOOL("FreezeTime"))
	{
		for (std::vector<LLViewerObject*>::iterator iter = idle_list.begin();
			iter != idle_list.end(); iter++)
		{
			objectp = *iter;
			if (objectp->getPCode() == LLViewerObject::LL_VO_CLOUDS ||
				objectp->isAvatar())
			{
				objectp->idleUpdate(agent, world, frame_time);
			}
		}
	}
	else
	{
		for (std::vector<LLViewerObject*>::iterator idle_iter = idle_list.begin();
			idle_iter != idle_list.end(); idle_iter++)
		{
			objectp = *idle_iter;
			if (!objectp->idleUpdate(agent, world, frame_time))
			{
				//  If Idle Update returns false, kill object!
				kill_list.push_back(objectp);
			}
			else
			{
				num_active_objects++;
			}
		}
		for (std::vector<LLViewerObject*>::iterator kill_iter = kill_list.begin();
			kill_iter != kill_list.end(); kill_iter++)
		{
			objectp = *kill_iter;
			killObject(objectp);
		}
	}

	mNumSizeCulled = 0;
	mNumVisCulled = 0;

	// compute all sorts of time-based stats
	// don't factor frames that were paused into the stats
	if (! mWasPaused)
	{
		LLViewerStats::getInstance()->updateFrameStats(time_diff);
	}

	/*
	// Debugging code for viewing orphans, and orphaned parents
	LLUUID id;
	for (i = 0; i < mOrphanParents.count(); i++)
	{
		id = sIndexAndLocalIDToUUID[mOrphanParents[i]];
		LLViewerObject *objectp = findObject(id);
		if (objectp)
		{
			std::string id_str;
			objectp->mID.toString(id_str);
			std::string tmpstr = std::string("Par:    ") + id_str;
			addDebugBeacon(objectp->getPositionAgent(),
							tmpstr,
							LLColor4(1.f,0.f,0.f,1.f),
							LLColor4(1.f,1.f,1.f,1.f));
		}
	}

	LLColor4 text_color;
	for (i = 0; i < mOrphanChildren.count(); i++)
	{
		OrphanInfo oi = mOrphanChildren[i];
		LLViewerObject *objectp = findObject(oi.mChildInfo);
		if (objectp)
		{
			std::string id_str;
			objectp->mID.toString(id_str);
			std::string tmpstr;
			if (objectp->getParent())
			{
				tmpstr = std::string("ChP:    ") + id_str;
				text_color = LLColor4(0.f, 1.f, 0.f, 1.f);
			}
			else
			{
				tmpstr = std::string("ChNoP:    ") + id_str;
				text_color = LLColor4(1.f, 0.f, 0.f, 1.f);
			}
			id = sIndexAndLocalIDToUUID[oi.mParentInfo];
			addDebugBeacon(objectp->getPositionAgent() + LLVector3(0.f, 0.f, -0.25f),
							tmpstr,
							LLColor4(0.25f,0.25f,0.25f,1.f),
							text_color);
		}
		i++;
	}
	*/

	mNumObjectsStat.addValue(mObjects.count());
	mNumActiveObjectsStat.addValue(num_active_objects);
	mNumSizeCulledStat.addValue(mNumSizeCulled);
	mNumVisCulledStat.addValue(mNumVisCulled);
}

void LLViewerObjectList::clearDebugText()
{
	for (S32 i = 0; i < mObjects.count(); i++)
	{
		mObjects[i]->setDebugText("");
	}
}


void LLViewerObjectList::cleanupReferences(LLViewerObject *objectp)
{
	LLMemType mt(LLMemType::MTYPE_OBJECT);
	if (mDeadObjects.count(objectp->mID))
	{
		llinfos << "Object " << objectp->mID << " already on dead list, ignoring cleanup!" << llendl;	
		return;
	}

	mDeadObjects.insert(std::pair<LLUUID, LLPointer<LLViewerObject> >(objectp->mID, objectp));

	// Cleanup any references we have to this object
	// Remove from object map so noone can look it up.

	mUUIDObjectMap.erase(objectp->mID);
	removeFromLocalIDTable(*objectp);

	if (objectp->onActiveList())
	{
		//llinfos << "Removing " << objectp->mID << " " << objectp->getPCodeString() << " from active list in cleanupReferences." << llendl;
		objectp->setOnActiveList(FALSE);
		mActiveObjects.erase(objectp);
	}

	if (objectp->isOnMap())
	{
		mMapObjects.removeObj(objectp);
	}

	// Don't clean up mObject references, these will be cleaned up more efficiently later!
	// Also, not cleaned up
	removeDrawable(objectp->mDrawable);

	mNumDeadObjects++;
}

void LLViewerObjectList::removeDrawable(LLDrawable* drawablep)
{
	if (!drawablep)
	{
		return;
	}

	for (S32 i = 0; i < drawablep->getNumFaces(); i++)
	{
		LLFace* facep = drawablep->getFace(i) ;
		if(facep)
		{
			   LLViewerObject* objectp = facep->getViewerObject();
			   if(objectp)
			   {
					   mSelectPickList.erase(objectp);
			   }
		}
	}
}

BOOL LLViewerObjectList::killObject(LLViewerObject *objectp)
{
	// When we're killing objects, all we do is mark them as dead.
	// We clean up the dead objects later.

	if (objectp)
	{
		if (objectp->isDead())
		{
			// This object is already dead.  Don't need to do more.
			return TRUE;
		}
		else
		{
			objectp->markDead();
		}

		return TRUE;
	}
	return FALSE;
}

void LLViewerObjectList::killObjects(LLViewerRegion *regionp)
{
	LLViewerObject *objectp;

	S32 i;
	for (i = 0; i < mObjects.count(); i++)
	{
		objectp = mObjects[i];
		
		if (objectp->mRegionp == regionp)
		{
			killObject(objectp);

			// invalidate region pointer. region will become invalid, but 
			// refcounted objects may survive the cleanDeadObjects() call below
			objectp->mRegionp = NULL;	 
		}
	}

	// Have to clean right away because the region is becoming invalid.
	cleanDeadObjects(FALSE);
}

void LLViewerObjectList::killAllObjects()
{
	// Used only on global destruction.
	LLViewerObject *objectp;

	for (S32 i = 0; i < mObjects.count(); i++)
	{
		objectp = mObjects[i];
		
		killObject(objectp);
		llassert(objectp->isDead());
	}

	cleanDeadObjects(FALSE);

	if(!mObjects.empty())
	{
		llwarns << "LLViewerObjectList::killAllObjects still has entries in mObjects: " << mObjects.count() << llendl;
		mObjects.clear();
	}

	if (!mActiveObjects.empty())
	{
		llwarns << "Some objects still on active object list!" << llendl;
		mActiveObjects.clear();
	}

	if (!mMapObjects.empty())
	{
		llwarns << "Some objects still on map object list!" << llendl;
		mMapObjects.clear();
	}
}

void LLViewerObjectList::cleanDeadObjects(BOOL use_timer)
{
	if (!mNumDeadObjects)
	{
		// No dead objects, don't need to scan object list.
		return;
	}

	S32 i = 0;
	S32 num_removed = 0;
	LLViewerObject *objectp;
	while (i < mObjects.count())
	{
		// Scan for all of the dead objects and remove any "global" references to them.
		objectp = mObjects[i];
		if (objectp->isDead())
		{
			mObjects.remove(i);
			num_removed++;

			if (num_removed == mNumDeadObjects)
			{
				// We've cleaned up all of the dead objects.
				break;
			}
		}
		else
		{
			// iterate, this isn't a dead object.
			i++;
		}
	}

	// We've cleaned the global object list, now let's do some paranoia testing on objects
	// before blowing away the dead list.
	mDeadObjects.clear();
	mNumDeadObjects = 0;
}

void LLViewerObjectList::updateActive(LLViewerObject *objectp)
{
	LLMemType mt(LLMemType::MTYPE_OBJECT);
	if (objectp->isDead())
	{
		return; // We don't update dead objects!
	}

	BOOL active = objectp->isActive();
	if (active != objectp->onActiveList())
	{
		if (active)
		{
			//llinfos << "Adding " << objectp->mID << " " << objectp->getPCodeString() << " to active list." << llendl;
			mActiveObjects.insert(objectp);
			objectp->setOnActiveList(TRUE);
		}
		else
		{
			//llinfos << "Removing " << objectp->mID << " " << objectp->getPCodeString() << " from active list." << llendl;
			mActiveObjects.erase(objectp);
			objectp->setOnActiveList(FALSE);
		}
	}
}



void LLViewerObjectList::shiftObjects(const LLVector3 &offset)
{
	// This is called when we shift our origin when we cross region boundaries...
	// We need to update many object caches, I'll document this more as I dig through the code
	// cleaning things out...

	if (gNoRender || 0 == offset.magVecSquared())
	{
		return;
	}

	LLViewerObject *objectp;
	S32 i;
	for (i = 0; i < mObjects.count(); i++)
	{
		objectp = getObject(i);
		// There could be dead objects on the object list, so don't update stuff if the object is dead.
		if (objectp)
		{
			objectp->updatePositionCaches();

			if (objectp->mDrawable.notNull() && !objectp->mDrawable->isDead())
			{
				gPipeline.markShift(objectp->mDrawable);
			}
		}
	}

	gPipeline.shiftObjects(offset);
	LLWorld::getInstance()->shiftRegions(offset);
}

void LLViewerObjectList::renderObjectsForMap(LLNetMap &netmap)
{
	LLColor4 above_water_color = gColors.getColor( "NetMapOtherOwnAboveWater" );
	LLColor4 below_water_color = gColors.getColor( "NetMapOtherOwnBelowWater" );
	LLColor4 you_own_above_water_color = 
						gColors.getColor( "NetMapYouOwnAboveWater" );
	LLColor4 you_own_below_water_color = 
						gColors.getColor( "NetMapYouOwnBelowWater" );
	LLColor4 group_own_above_water_color = 
						gColors.getColor( "NetMapGroupOwnAboveWater" );
	LLColor4 group_own_below_water_color = 
						gColors.getColor( "NetMapGroupOwnBelowWater" );


	for (S32 i = 0; i < mMapObjects.count(); i++)
	{
		LLViewerObject* objectp = mMapObjects[i];
		if (!objectp->getRegion() || objectp->isOrphaned() || objectp->isAttachment())
		{
			continue;
		}
		const LLVector3& scale = objectp->getScale();
		const LLVector3d pos = objectp->getPositionGlobal();
		const F64 water_height = F64( objectp->getRegion()->getWaterHeight() );
		// LLWorld::getInstance()->getWaterHeight();

		F32 approx_radius = (scale.mV[VX] + scale.mV[VY]) * 0.5f * 0.5f * 1.3f;  // 1.3 is a fudge

		LLColor4U color = above_water_color;
		if( objectp->permYouOwner() )
		{
			const F32 MIN_RADIUS_FOR_OWNED_OBJECTS = 2.f;
			if( approx_radius < MIN_RADIUS_FOR_OWNED_OBJECTS )
			{
				approx_radius = MIN_RADIUS_FOR_OWNED_OBJECTS;
			}

			if( pos.mdV[VZ] >= water_height )
			{
				if ( objectp->permGroupOwner() )
				{
					color = group_own_above_water_color;
				}
				else
				{
				color = you_own_above_water_color;
			}
			}
			else
			{
				if ( objectp->permGroupOwner() )
				{
					color = group_own_below_water_color;
				}
			else
			{
				color = you_own_below_water_color;
			}
		}
		}
		else
		if( pos.mdV[VZ] < water_height )
		{
			color = below_water_color;
		}

		netmap.renderScaledPointGlobal( 
			pos, 
			color,
			approx_radius );
	}
}

void LLViewerObjectList::renderObjectBounds(const LLVector3 &center)
{
}

void LLViewerObjectList::renderObjectsForSelect(LLCamera &camera, const LLRect& screen_rect, BOOL pick_parcel_wall, BOOL render_transparent)
{
	generatePickList(camera);
	renderPickList(screen_rect, pick_parcel_wall, render_transparent);
}

void LLViewerObjectList::generatePickList(LLCamera &camera)
{
		LLViewerObject *objectp;
		S32 i;
		// Reset all of the GL names to zero.
		for (i = 0; i < mObjects.count(); i++)
		{
			objectp = mObjects[i];
			objectp->mGLName = 0;
		}

		mSelectPickList.clear();

		std::vector<LLDrawable*> pick_drawables;

		for (LLWorld::region_list_t::iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* region = *iter;
			for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
			{
				LLSpatialPartition* part = region->getSpatialPartition(i);
				if (part)
				{	
					part->cull(camera, &pick_drawables, TRUE);
				}
			}
		}

		for (std::vector<LLDrawable*>::iterator iter = pick_drawables.begin();
			iter != pick_drawables.end(); iter++)
		{
			LLDrawable* drawablep = *iter;
			if( !drawablep )
				continue;

			LLViewerObject* last_objectp = NULL;
			for (S32 face_num = 0; face_num < drawablep->getNumFaces(); face_num++)
			{
				LLViewerObject* objectp = drawablep->getFace(face_num)->getViewerObject();

				if (objectp && objectp != last_objectp)
				{
					mSelectPickList.insert(objectp);
					last_objectp = objectp;
				}
			}
		}

		LLHUDText::addPickable(mSelectPickList);

		for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
			iter != LLCharacter::sInstances.end(); ++iter)
		{
			objectp = (LLVOAvatar*) *iter;
			if (!objectp->isDead())
			{
				if (objectp->mDrawable.notNull() && objectp->mDrawable->isVisible())
				{
					mSelectPickList.insert(objectp);
				}
			}
		}

		// add all hud objects to pick list
		LLVOAvatar* avatarp = gAgent.getAvatarObject();
		if (avatarp)
		{
			for (LLVOAvatar::attachment_map_t::iterator iter = avatarp->mAttachmentPoints.begin(); 
				 iter != avatarp->mAttachmentPoints.end(); )
			{
				LLVOAvatar::attachment_map_t::iterator curiter = iter++;
				LLViewerJointAttachment* attachmentp = curiter->second;
				if (attachmentp->getIsHUDAttachment())
				{
					LLViewerObject* objectp = attachmentp->getObject();
					if (objectp)
					{
						mSelectPickList.insert(objectp);		
						LLViewerObject::const_child_list_t& child_list = objectp->getChildren();
						for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
							 iter != child_list.end(); iter++)
						{
							LLViewerObject* childp = *iter;
							if (childp)
							{
								mSelectPickList.insert(childp);
							}
						}
					}
				}
			}
		}
		
		S32 num_pickables = (S32)mSelectPickList.size() + LLHUDIcon::getNumInstances();

		if (num_pickables != 0)
		{
			S32 step = (0x000fffff - GL_NAME_INDEX_OFFSET) / num_pickables;

			std::set<LLViewerObject*>::iterator pick_it;
			i = 0;
			for (pick_it = mSelectPickList.begin(); pick_it != mSelectPickList.end();)
			{
				LLViewerObject* objp = (*pick_it);
				if (!objp || objp->isDead() || !objp->mbCanSelect)
				{
					mSelectPickList.erase(pick_it++);
					continue;
				}
				
				objp->mGLName = (i * step) + GL_NAME_INDEX_OFFSET;
				i++;
				++pick_it;
			}

			LLHUDIcon::generatePickIDs(i * step, step);
	}
}

void LLViewerObjectList::renderPickList(const LLRect& screen_rect, BOOL pick_parcel_wall, BOOL render_transparent)
{
	gRenderForSelect = TRUE;
		
	gPipeline.renderForSelect(mSelectPickList, render_transparent, screen_rect);

	//
	// Render pass for selected objects
	//
	gGL.color4f(1,1,1,1);	
	gViewerWindow->renderSelections( TRUE, pick_parcel_wall, FALSE );

	//fix for DEV-19335.  Don't pick hud objects when customizing avatar (camera mode doesn't play nice with nametags).
	if (!gAgent.cameraCustomizeAvatar())
	{
		// render pickable ui elements, like names, etc.
		LLHUDObject::renderAllForSelect();
	}
	
	gGL.flush();
	LLVertexBuffer::unbind();

	gRenderForSelect = FALSE;

	//llinfos << "Rendered " << count << " for select" << llendl;
	//llinfos << "Took " << pick_timer.getElapsedTimeF32()*1000.f << "ms to pick" << llendl;
}

LLViewerObject *LLViewerObjectList::getSelectedObject(const U32 object_id)
{
	std::set<LLViewerObject*>::iterator pick_it;
	for (pick_it = mSelectPickList.begin(); pick_it != mSelectPickList.end(); ++pick_it)
	{
		if ((*pick_it)->mGLName == object_id)
		{
			return (*pick_it);
		}
	}
	return NULL;
}

void LLViewerObjectList::addDebugBeacon(const LLVector3 &pos_agent,
										const std::string &string,
										const LLColor4 &color,
										const LLColor4 &text_color,
										S32 line_width)
{
	LLDebugBeacon *beaconp = mDebugBeacons.reserve_block(1);
	beaconp->mPositionAgent = pos_agent;
	beaconp->mString = string;
	beaconp->mColor = color;
	beaconp->mTextColor = text_color;
	beaconp->mLineWidth = line_width;
}

void LLViewerObjectList::resetObjectBeacons()
{
	mDebugBeacons.reset();
}

LLViewerObject *LLViewerObjectList::createObjectViewer(const LLPCode pcode, LLViewerRegion *regionp)
{
	LLMemType mt(LLMemType::MTYPE_OBJECT);
	LLUUID fullid;
	fullid.generate();

	LLViewerObject *objectp = LLViewerObject::createObject(fullid, pcode, regionp);
	if (!objectp)
	{
// 		llwarns << "Couldn't create object of type " << LLPrimitive::pCodeToString(pcode) << llendl;
		return NULL;
	}

	mUUIDObjectMap[fullid] = objectp;

	mObjects.put(objectp);

	updateActive(objectp);

	return objectp;
}



LLViewerObject *LLViewerObjectList::createObject(const LLPCode pcode, LLViewerRegion *regionp,
												 const LLUUID &uuid, const U32 local_id, const LLHost &sender)
{
	LLMemType mt(LLMemType::MTYPE_OBJECT);
	LLFastTimer t(LLFastTimer::FTM_CREATE_OBJECT);
	
	LLUUID fullid;
	if (uuid == LLUUID::null)
	{
		fullid.generate();
	}
	else
	{
		fullid = uuid;
	}

	LLViewerObject *objectp = LLViewerObject::createObject(fullid, pcode, regionp);
	if (!objectp)
	{
// 		llwarns << "Couldn't create object of type " << LLPrimitive::pCodeToString(pcode) << " id:" << fullid << llendl;
		return NULL;
	}

	mUUIDObjectMap[fullid] = objectp;
	setUUIDAndLocal(fullid,
					local_id,
					gMessageSystem->getSenderIP(),
					gMessageSystem->getSenderPort());

	mObjects.put(objectp);

	updateActive(objectp);

	return objectp;
}

LLViewerObject *LLViewerObjectList::replaceObject(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp)
{
	LLViewerObject *old_instance = findObject(id);
	if (old_instance)
	{
		cleanupReferences(old_instance);
		old_instance->markDead();
		
		return createObject(pcode, regionp, id, old_instance->getLocalID(), LLHost());
	}
	return NULL;
}

S32 LLViewerObjectList::findReferences(LLDrawable *drawablep) const
{
	LLViewerObject *objectp;
	S32 i;
	S32 num_refs = 0;
	for (i = 0; i < mObjects.count(); i++)
	{
		objectp = mObjects[i];
		if (objectp->mDrawable.notNull())
		{
			num_refs += objectp->mDrawable->findReferences(drawablep);
		}
	}
	return num_refs;
}


void LLViewerObjectList::orphanize(LLViewerObject *childp, U32 parent_id, U32 ip, U32 port)
{
	LLMemType mt(LLMemType::MTYPE_OBJECT);
#ifdef ORPHAN_SPAM
	llinfos << "Orphaning object " << childp->getID() << " with parent " << parent_id << llendl;
#endif

	// We're an orphan, flag things appropriately.
	childp->mOrphaned = TRUE;
	if (childp->mDrawable.notNull())
	{
		bool make_invisible = true;
		LLViewerObject *parentp = (LLViewerObject *)childp->getParent();
		if (parentp)
		{
			if (parentp->getRegion() != childp->getRegion())
			{
				// This is probably an object flying across a region boundary, the
				// object probably ISN'T being reparented, but just got an object
				// update out of order (child update before parent).
				make_invisible = false;
				//llinfos << "Don't make object handoffs invisible!" << llendl;
			}
		}

		if (make_invisible)
		{
			// Make sure that this object becomes invisible if it's an orphan
			childp->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
		}
	}

	// Unknown parent, add to orpaned child list
	U64 parent_info = getIndex(parent_id, ip, port);

	if (-1 == mOrphanParents.find(parent_info))
	{
		mOrphanParents.put(parent_info);
	}

	LLViewerObjectList::OrphanInfo oi(parent_info, childp->mID);
	if (-1 == mOrphanChildren.find(oi))
	{
		mOrphanChildren.put(oi);
		mNumOrphans++;
	}
}


void LLViewerObjectList::findOrphans(LLViewerObject* objectp, U32 ip, U32 port)
{
	if (gNoRender)
	{
		return;
	}

	if (objectp->isDead())
	{
		llwarns << "Trying to find orphans for dead obj " << objectp->mID 
			<< ":" << objectp->getPCodeString() << llendl;
		return;
	}

	// See if we are a parent of an orphan.
	// Note:  This code is fairly inefficient but it should happen very rarely.
	// It can be sped up if this is somehow a performance issue...
	if (0 == mOrphanParents.count())
	{
		// no known orphan parents
		return;
	}
	if (-1 == mOrphanParents.find(getIndex(objectp->mLocalID, ip, port)))
	{
		// did not find objectp in OrphanParent list
		return;
	}

	S32 i;
	U64 parent_info = getIndex(objectp->mLocalID, ip, port);
	BOOL orphans_found = FALSE;
	// Iterate through the orphan list, and set parents of matching children.
	for (i = 0; i < mOrphanChildren.count(); i++)
	{
		if (mOrphanChildren[i].mParentInfo != parent_info)
		{
			continue;
		}
		LLViewerObject *childp = findObject(mOrphanChildren[i].mChildInfo);
		if (childp)
		{
			if (childp == objectp)
			{
				llwarns << objectp->mID << " has self as parent, skipping!" 
					<< llendl;
				continue;
			}

#ifdef ORPHAN_SPAM
			llinfos << "Reunited parent " << objectp->mID 
				<< " with child " << childp->mID << llendl;
			llinfos << "Glob: " << objectp->getPositionGlobal() << llendl;
			llinfos << "Agent: " << objectp->getPositionAgent() << llendl;
			addDebugBeacon(objectp->getPositionAgent(),"");
#endif
            gPipeline.markMoved(objectp->mDrawable);                
            objectp->setChanged(LLXform::MOVED | LLXform::SILHOUETTE);

			// Flag the object as no longer orphaned
			childp->mOrphaned = FALSE;
			if (childp->mDrawable.notNull())
			{
				// Make the drawable visible again and set the drawable parent
 				childp->mDrawable->setState(LLDrawable::CLEAR_INVISIBLE);
				childp->setDrawableParent(objectp->mDrawable); // LLViewerObjectList::findOrphans()
			}

			// Make certain particles, icon and HUD aren't hidden
			childp->hideExtraDisplayItems( FALSE );

			objectp->addChild(childp);
			orphans_found = TRUE;
		}
		else
		{
			llinfos << "Missing orphan child, removing from list" << llendl;
			mOrphanChildren.remove(i);
			i--;
		}
	}

	// Remove orphan parent and children from lists now that they've been found
	mOrphanParents.remove(mOrphanParents.find(parent_info));

	i = 0;
	while (i < mOrphanChildren.count())
	{
		if (mOrphanChildren[i].mParentInfo == parent_info)
		{
			mOrphanChildren.remove(i);
			mNumOrphans--;
		}
		else
		{
			i++;
		}
	}

	if (orphans_found && objectp->isSelected())
	{
		LLSelectNode* nodep = LLSelectMgr::getInstance()->getSelection()->findNode(objectp);
		if (nodep && !nodep->mIndividualSelection)
		{
			// rebuild selection with orphans
			LLSelectMgr::getInstance()->deselectObjectAndFamily(objectp);
			LLSelectMgr::getInstance()->selectObjectAndFamily(objectp);
		}
	}
}

////////////////////////////////////////////////////////////////////////////

LLViewerObjectList::OrphanInfo::OrphanInfo()
	: mParentInfo(0)
{
}

LLViewerObjectList::OrphanInfo::OrphanInfo(const U64 parent_info, const LLUUID child_info)
	: mParentInfo(parent_info), mChildInfo(child_info)
{
}

bool LLViewerObjectList::OrphanInfo::operator==(const OrphanInfo &rhs) const
{
	return (mParentInfo == rhs.mParentInfo) && (mChildInfo == rhs.mChildInfo);
}

bool LLViewerObjectList::OrphanInfo::operator!=(const OrphanInfo &rhs) const
{
	return !operator==(rhs);
}


