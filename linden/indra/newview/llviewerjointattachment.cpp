/** 
 * @file llviewerjointattachment.cpp
 * @brief Implementation of LLViewerJointAttachment class
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

#include "llviewerjointattachment.h"

#include "llagentconstants.h"

#include "llviewercontrol.h"
#include "lldrawable.h"
#include "llgl.h"
#include "llrender.h"
#include "llvoavatar.h"
#include "llvolume.h"
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llinventorymodel.h"
#include "llviewerobjectlist.h"
#include "llface.h"
#include "llvoavatar.h"

#include "llglheaders.h"

//MK
#include "llinventoryview.h"
#include "llagent.h"
extern BOOL RRenabled;
//mk

extern LLPipeline gPipeline;

//-----------------------------------------------------------------------------
// LLViewerJointAttachment()
//-----------------------------------------------------------------------------
LLViewerJointAttachment::LLViewerJointAttachment() :
mAttachedObject(NULL),
mVisibleInFirst(FALSE),
mGroup(0),
mIsHUDAttachment(FALSE),
mPieSlice(-1)
{
	mValid = FALSE;
	mUpdateXform = FALSE;
}

//-----------------------------------------------------------------------------
// ~LLViewerJointAttachment()
//-----------------------------------------------------------------------------
LLViewerJointAttachment::~LLViewerJointAttachment()
{
}

//-----------------------------------------------------------------------------
// isTransparent()
//-----------------------------------------------------------------------------
BOOL LLViewerJointAttachment::isTransparent()
{
	return FALSE;
}

//-----------------------------------------------------------------------------
// drawShape()
//-----------------------------------------------------------------------------
U32 LLViewerJointAttachment::drawShape( F32 pixelArea, BOOL first_pass )
{
	if (LLVOAvatar::sShowAttachmentPoints)
	{
		LLGLDisable cull_face(GL_CULL_FACE);
		
		gGL.color4f(1.f, 1.f, 1.f, 1.f);
		gGL.begin(LLRender::QUADS);
		{
			gGL.vertex3f(-0.1f, 0.1f, 0.f);
			gGL.vertex3f(-0.1f, -0.1f, 0.f);
			gGL.vertex3f(0.1f, -0.1f, 0.f);
			gGL.vertex3f(0.1f, 0.1f, 0.f);
		}gGL.end();
	}
	return 0;
}

void LLViewerJointAttachment::setupDrawable(LLDrawable* drawablep)
{
	drawablep->mXform.setParent(&mXform); // LLViewerJointAttachment::lazyAttach
	drawablep->makeActive();
	LLVector3 current_pos = mAttachedObject->getRenderPosition();
	LLQuaternion current_rot = mAttachedObject->getRenderRotation();
	LLQuaternion attachment_pt_inv_rot = ~getWorldRotation();

	current_pos -= getWorldPosition();
	current_pos.rotVec(attachment_pt_inv_rot);

	current_rot = current_rot * attachment_pt_inv_rot;

	drawablep->mXform.setPosition(current_pos);
	drawablep->mXform.setRotation(current_rot);
	gPipeline.markMoved(drawablep);
	gPipeline.markTextured(drawablep); // face may need to change draw pool to/from POOL_HUD
	drawablep->setState(LLDrawable::USE_BACKLIGHT);
	
	if(mIsHUDAttachment)
	{
		for (S32 face_num = 0; face_num < drawablep->getNumFaces(); face_num++)
		{
			drawablep->getFace(face_num)->setState(LLFace::HUD_RENDER);
		}
	}

	LLViewerObject::const_child_list_t& child_list = mAttachedObject->getChildren();
	for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
		 iter != child_list.end(); iter++)
	{
		LLViewerObject* childp = *iter;
		if (childp && childp->mDrawable.notNull())
		{
			childp->mDrawable->setState(LLDrawable::USE_BACKLIGHT);
			gPipeline.markTextured(childp->mDrawable); // face may need to change draw pool to/from POOL_HUD
			gPipeline.markMoved(childp->mDrawable);

			if(mIsHUDAttachment)
			{
				for (S32 face_num = 0; face_num < childp->mDrawable->getNumFaces(); face_num++)
				{
					childp->mDrawable->getFace(face_num)->setState(LLFace::HUD_RENDER);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// addObject()
//-----------------------------------------------------------------------------
BOOL LLViewerJointAttachment::addObject(LLViewerObject* object)
{
//MK
	BOOL was_empty = true;
//mk
	if (mAttachedObject)
	{
//MK
		was_empty = false;
//mk
		llwarns << "Attempted to attach object where an attachment already exists!" << llendl;
		
		if (mAttachedObject == object) {
			llinfos << "(same object re-attached)" << llendl;
			removeObject(mAttachedObject);
			// Pass through anyway to let setupDrawable()
			// re-connect object to the joint correctly
		}
		else {
			llinfos << "(objects differ, removing existing object)" << llendl;
			// Rather hacky, but no-one can think of something
			// better to do for this case.
			gObjectList.killObject(mAttachedObject);
			// Proceed with new object attachment
		}
	}
	mAttachedObject = object;
	
	LLUUID item_id;

	// Find the inventory item ID of the attached object
	LLNameValue* item_id_nv = object->getNVPair("AttachItemID");
	if( item_id_nv )
	{
		const char* s = item_id_nv->getString();
		if( s )
		{
			item_id.set( s );
			lldebugs << "getNVPair( AttachItemID ) = " << item_id << llendl;
		}
	}

	mItemID = item_id;

	LLDrawable* drawablep = object->mDrawable;

	if (drawablep)
	{
		//if object is active, make it static
		if(drawablep->isActive())
		{
			drawablep->makeStatic(FALSE) ;
		}

		setupDrawable(drawablep);
	}

	if (mIsHUDAttachment)
	{
		if (object->mText.notNull())
		{
			object->mText->setOnHUDAttachment(TRUE);
		}
		LLViewerObject::const_child_list_t& child_list = object->getChildren();
		for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
			 iter != child_list.end(); iter++)
		{
			LLViewerObject* childp = *iter;
			if (childp && childp->mText.notNull())
			{
				childp->mText->setOnHUDAttachment(TRUE);
			}
		}
	}
	calcLOD();
	mUpdateXform = TRUE;

//MK
	if (RRenabled)
	{
		// If this attachment point is locked and empty, then force detach, unless the attached object was supposed to be reattached automatically
		if (was_empty)
		{
			std::string name = getName();
			LLStringUtil::toLower(name);
			if (!gAgent.mRRInterface.canAttach(object, name))
			{
				bool just_reattaching = false;
				std::deque<AssetAndTarget>::iterator it = gAgent.mRRInterface.sAssetsToReattach.begin();
				for (; it != gAgent.mRRInterface.sAssetsToReattach.end(); ++it)
				{
					if (it->uuid == item_id)
					{
						just_reattaching = true;
						break;
					}
				}
				if (!just_reattaching)
				{
					llinfos << "Attached to a locked point : " << mItemID << llendl;
					gMessageSystem->newMessage("ObjectDetach");
					gMessageSystem->nextBlockFast(_PREHASH_AgentData);
					gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
					gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());	
					gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
					gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
					gMessageSystem->sendReliable( gAgent.getRegionHost() );

					gAgent.mRRInterface.sJustDetached.uuid = item_id;
					gAgent.mRRInterface.sJustDetached.attachpt = getName();
				}
			}
		}

		// If the UUID of the attached item is contained into the list of things waiting to reattach,
		// signal it and remove it from the list (and if the list is not empty, restart the timer for the next).
		std::deque<AssetAndTarget>::iterator it = gAgent.mRRInterface.sAssetsToReattach.begin();
		for (; it != gAgent.mRRInterface.sAssetsToReattach.end(); ++it)
		{
			if (it->uuid == item_id)
			{
				llinfos << "Reattached asset " << item_id << " automatically" << llendl;
				gAgent.mRRInterface.sAssetsToReattach.erase(it);
//				gAgent.mRRInterface.sJustReattached.uuid = item_id;
//				gAgent.mRRInterface.sJustReattached.attachpt = getName();
				// Replace the previously stored asset id with the new viewer id in the list of restrictions
				gAgent.mRRInterface.replace(item_id, object->getRootEdit()->getID());
				if (!gAgent.mRRInterface.sAssetsToReattach.empty())
				{
					gAgent.mRRInterface.sTimeBeforeReattaching = 50; // let's wait a little before reattaching the next item
				}
				else
				{
					gAgent.mRRInterface.sTimeBeforeReattaching = 0;
				}
				break;
			}
		}
	}
//mk

	return TRUE;
}

//-----------------------------------------------------------------------------
// removeObject()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::removeObject(LLViewerObject *object)
{
//MK
	if (RRenabled)
	{
		// We first need to check whether the object is locked, as some techniques (like llAttachToAvatar)
		// can kick even a locked attachment off.
		// If so, retain its UUID for later
		// Note : we need to delay the reattach a little, or we risk losing the item in the inventory.
		LLVOAvatar *avatarp = gAgent.getAvatarObject();
		LLUUID inv_item_id = LLUUID::null;
		LLInventoryItem* inv_item = gAgent.mRRInterface.getItem(object->getRootEdit()->getID());
		if (inv_item) inv_item_id = inv_item->getUUID();
		std::string target_attachpt = "";
		if (avatarp) target_attachpt = avatarp->getAttachedPointName(inv_item_id);
		if (!gAgent.mRRInterface.canDetach(object)
			&& gAgent.mRRInterface.sJustDetached.attachpt != target_attachpt	// we didn't just detach something from this attach pt automatically
			&& gAgent.mRRInterface.sJustReattached.attachpt != target_attachpt)	// we didn't just reattach something to this attach pt automatically
		{
			llinfos << "Detached a locked object : " << mItemID << llendl;

			std::deque<AssetAndTarget>::iterator it = gAgent.mRRInterface.sAssetsToReattach.begin();
			bool found = false;
			bool found_for_this_point = false;
			for (; it != gAgent.mRRInterface.sAssetsToReattach.end(); ++it)
			{
				if (it->uuid == mItemID) found = true;
				if (it->attachpt == target_attachpt) found_for_this_point = true;
			}

			if (!found && !found_for_this_point)
			{
				AssetAndTarget at;
				at.uuid = mItemID;
				at.attachpt = target_attachpt;
				gAgent.mRRInterface.sTimeBeforeReattaching = 50; // number of cycles before triggering the reattach (approx 5 seconds)
				gAgent.mRRInterface.sAssetsToReattach.push_back(at);
				// Little hack : store this item's asset id into the list of restrictions so they are automatically reapplied when it is reattached
				gAgent.mRRInterface.replace(object->getRootEdit()->getID(), mItemID);
			}
		}
		gAgent.mRRInterface.sJustDetached.uuid.setNull();
		gAgent.mRRInterface.sJustDetached.attachpt = "";
		gAgent.mRRInterface.sJustReattached.uuid.setNull();
		gAgent.mRRInterface.sJustReattached.attachpt = "";
	}
//mk
	// force object visibile
	setAttachmentVisibility(TRUE);

	if (object->mDrawable.notNull())
	{
		//if object is active, make it static
		if(object->mDrawable->isActive())
		{
			object->mDrawable->makeStatic(FALSE) ;
		}

		LLVector3 cur_position = object->getRenderPosition();
		LLQuaternion cur_rotation = object->getRenderRotation();

		object->mDrawable->mXform.setPosition(cur_position);
		object->mDrawable->mXform.setRotation(cur_rotation);
		gPipeline.markMoved(object->mDrawable, TRUE);
		gPipeline.markTextured(object->mDrawable); // face may need to change draw pool to/from POOL_HUD
		object->mDrawable->clearState(LLDrawable::USE_BACKLIGHT);

		if (mIsHUDAttachment)
		{
			for (S32 face_num = 0; face_num < object->mDrawable->getNumFaces(); face_num++)
			{
				object->mDrawable->getFace(face_num)->clearState(LLFace::HUD_RENDER);
			}
		}
	}

	LLViewerObject::const_child_list_t& child_list = object->getChildren();
	for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
		 iter != child_list.end(); iter++)
	{
		LLViewerObject* childp = *iter;
		if (childp && childp->mDrawable.notNull())
		{
			childp->mDrawable->clearState(LLDrawable::USE_BACKLIGHT);
			gPipeline.markTextured(childp->mDrawable); // face may need to change draw pool to/from POOL_HUD
			if (mIsHUDAttachment)
			{
				for (S32 face_num = 0; face_num < childp->mDrawable->getNumFaces(); face_num++)
				{
					childp->mDrawable->getFace(face_num)->clearState(LLFace::HUD_RENDER);
				}
			}
		}
	} 

	if (mIsHUDAttachment)
	{
		if (object->mText.notNull())
		{
			object->mText->setOnHUDAttachment(FALSE);
		}
		LLViewerObject::const_child_list_t& child_list = object->getChildren();
		for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
			 iter != child_list.end(); iter++)
		{
			LLViewerObject* childp = *iter;
			if (childp->mText.notNull())
			{
				childp->mText->setOnHUDAttachment(FALSE);
			}
		}
	}

	mAttachedObject = NULL;
	mUpdateXform = FALSE;
	mItemID.setNull();
}

//-----------------------------------------------------------------------------
// setAttachmentVisibility()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::setAttachmentVisibility(BOOL visible)
{
	if (!mAttachedObject || mAttachedObject->mDrawable.isNull() || 
		!(mAttachedObject->mDrawable->getSpatialBridge()))
		return;

	if (visible)
	{
		// Hack to make attachments not visible by disabling their type mask!
		// This will break if you can ever attach non-volumes! - djs 02/14/03
		mAttachedObject->mDrawable->getSpatialBridge()->mDrawableType = 
			mAttachedObject->isHUDAttachment() ? LLPipeline::RENDER_TYPE_HUD : LLPipeline::RENDER_TYPE_VOLUME;
	}
	else
	{
		mAttachedObject->mDrawable->getSpatialBridge()->mDrawableType = 0;
	}
}

//-----------------------------------------------------------------------------
// setOriginalPosition()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::setOriginalPosition(LLVector3& position)
{
	mOriginalPos = position;
	setPosition(position);
}

//-----------------------------------------------------------------------------
// clampObjectPosition()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::clampObjectPosition()
{
	if (mAttachedObject)
	{
		// *NOTE: object can drift when hitting maximum radius
		LLVector3 attachmentPos = mAttachedObject->getPosition();
		F32 dist = attachmentPos.normVec();
		dist = llmin(dist, MAX_ATTACHMENT_DIST);
		attachmentPos *= dist;
		mAttachedObject->setPosition(attachmentPos);
	}
}

//-----------------------------------------------------------------------------
// calcLOD()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::calcLOD()
{
	F32 maxarea = mAttachedObject->getMaxScale() * mAttachedObject->getMidScale();
	LLViewerObject::const_child_list_t& child_list = mAttachedObject->getChildren();
	for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
		 iter != child_list.end(); iter++)
	{
		LLViewerObject* childp = *iter;
		F32 area = childp->getMaxScale() * childp->getMidScale();
		maxarea = llmax(maxarea, area);
	}
	maxarea = llclamp(maxarea, .01f*.01f, 1.f);
	F32 avatar_area = (4.f * 4.f); // pixels for an avatar sized attachment
	F32 min_pixel_area = avatar_area / maxarea;
	setLOD(min_pixel_area);
}

//-----------------------------------------------------------------------------
// updateLOD()
//-----------------------------------------------------------------------------
BOOL LLViewerJointAttachment::updateLOD(F32 pixel_area, BOOL activate)
{
	BOOL res = FALSE;
	if (!mValid)
	{
		setValid(TRUE, TRUE);
		res = TRUE;
	}
	return res;
}

