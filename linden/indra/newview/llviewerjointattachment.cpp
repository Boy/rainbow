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
//mk

extern LLPipeline gPipeline;

//-----------------------------------------------------------------------------
// LLViewerJointAttachment()
//-----------------------------------------------------------------------------
LLViewerJointAttachment::LLViewerJointAttachment() :
mVisibleInFirst(FALSE),
mGroup(0),
mIsHUDAttachment(FALSE),
mPieSlice(-1)
{
	mValid = FALSE;
	mUpdateXform = FALSE;
	mAttachedObjects.clear();
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
U32 LLViewerJointAttachment::drawShape( F32 pixelArea, BOOL first_pass, BOOL is_dummy )
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

void LLViewerJointAttachment::setupDrawable(LLViewerObject *object)
{
	if (!object->mDrawable)
		return;
	if (object->mDrawable->isActive())
	{
		object->mDrawable->makeStatic(FALSE);
	}

	object->mDrawable->mXform.setParent(getXform()); // LLViewerJointAttachment::lazyAttach
	object->mDrawable->makeActive();
	LLVector3 current_pos = object->getRenderPosition();
	LLQuaternion current_rot = object->getRenderRotation();
	LLQuaternion attachment_pt_inv_rot = ~getWorldRotation();

	current_pos -= getWorldPosition();
	current_pos.rotVec(attachment_pt_inv_rot);

	current_rot = current_rot * attachment_pt_inv_rot;

	object->mDrawable->mXform.setPosition(current_pos);
	object->mDrawable->mXform.setRotation(current_rot);
	gPipeline.markMoved(object->mDrawable);
	gPipeline.markTextured(object->mDrawable); // face may need to change draw pool to/from POOL_HUD
	object->mDrawable->setState(LLDrawable::USE_BACKLIGHT);
	
	if(mIsHUDAttachment)
	{
		for (S32 face_num = 0; face_num < object->mDrawable->getNumFaces(); face_num++)
		{
			object->mDrawable->getFace(face_num)->setState(LLFace::HUD_RENDER);
		}
	}

	LLViewerObject::const_child_list_t& child_list = object->getChildren();
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
	object->extractAttachmentItemID();

//MK
	BOOL was_empty = true;
//mk
	if (isObjectAttached(object))
	{
//MK
		was_empty = false;
//mk
		llinfos << "(same object re-attached)" << llendl;
		removeObject(object);
		// Pass through anyway to let setupDrawable()
		// re-connect object to the joint correctly
	}

	mAttachedObjects.push_back(object);
	setupDrawable(object);

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
	if (gRRenabled)
	{
		LLInventoryItem* inv_item = gAgent.mRRInterface.getItem (object->getID());
		LLUUID item_id = object->getAttachmentItemID();
		// If this attachment point is locked and empty, then force detach, unless the attached object was supposed to be reattached automatically
		if (was_empty)
		{
			std::string name = getName();
			LLStringUtil::toLower(name);
			if (!gAgent.mRRInterface.canAttach(object, name, true))
			{
				bool just_reattaching = false;
				std::deque<AssetAndTarget>::iterator it = gAgent.mRRInterface.mAssetsToReattach.begin();
				for (; it != gAgent.mRRInterface.mAssetsToReattach.end(); ++it)
				{
					if (it->uuid == item_id)
					{
						just_reattaching = true;
						break;
					}
				}
				if (!just_reattaching)
				{
					llinfos << "Attached to a locked point : " << item_id << llendl;
					gMessageSystem->newMessage("ObjectDetach");
					gMessageSystem->nextBlockFast(_PREHASH_AgentData);
					gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
					gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());	
					gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
					gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
					gMessageSystem->sendReliable( gAgent.getRegionHost() );

					gAgent.mRRInterface.mJustDetached.uuid = item_id;
					gAgent.mRRInterface.mJustDetached.attachpt = getName();

					// Now notify that this object has been attached and will be detached right away
					gAgent.mRRInterface.notify(LLUUID::null, "attached illegally " + getName(), "");
				}
				else
				{
					// Notify that this object has just been reattached
					gAgent.mRRInterface.notify(LLUUID::null, "reattached legally " + getName(), "");
				}
			}
			else
			{
				// Notify that this object has been attached
				if (inv_item)
				{
					gAgent.mRRInterface.notify(LLUUID::null, "attached legally " + getName(), "");
				}
			}
		}

		// If the UUID of the attached item is contained into the list of things waiting to reattach,
		// signal it and remove it from the list.
		std::deque<AssetAndTarget>::iterator it = gAgent.mRRInterface.mAssetsToReattach.begin();
		for (; it != gAgent.mRRInterface.mAssetsToReattach.end(); ++it)
		{
			if (it->uuid == item_id)
			{
				llinfos << "Reattached asset " << item_id << " automatically" << llendl;
				gAgent.mRRInterface.mReattaching = FALSE;
				gAgent.mRRInterface.mReattachTimeout = FALSE;
				gAgent.mRRInterface.mAssetsToReattach.erase(it);
//				gAgent.mRRInterface.mJustReattached.uuid = item_id;
//				gAgent.mRRInterface.mJustReattached.attachpt = getName();
				// Replace the previously stored asset id with the new viewer id in the list of restrictions
				gAgent.mRRInterface.replace(item_id, object->getRootEdit()->getID());
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
	attachedobjs_vec_t::iterator iter;
	for (iter = mAttachedObjects.begin();
		 iter != mAttachedObjects.end();
		 ++iter)
	{
		LLViewerObject *attached_object = (*iter);
		if (attached_object == object)
		{
			break;
		}
	}
	if (iter == mAttachedObjects.end())
	{
		llwarns << "Could not find object to detach" << llendl;
		return;
	}
//MK
	if (gRRenabled)
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
		inv_item_id = object->getAttachmentItemID();
 		if (!gAgent.mRRInterface.canDetach(object)
			&& gAgent.mRRInterface.mJustDetached.attachpt != target_attachpt	// we didn't just detach something from this attach pt automatically
			&& gAgent.mRRInterface.mJustReattached.attachpt != target_attachpt)	// we didn't just reattach something to this attach pt automatically
		{
			llinfos << "Detached a locked object : " << inv_item_id << " from " << target_attachpt << llendl;

			// Now notify that this object has been detached and will be reattached right away
			gAgent.mRRInterface.notify (LLUUID::null, "detached illegally " + getName(), "");

			std::deque<AssetAndTarget>::iterator it = gAgent.mRRInterface.mAssetsToReattach.begin();
			bool found = false;
			bool found_for_this_point = false;
			for (; it != gAgent.mRRInterface.mAssetsToReattach.end(); ++it)
			{
				if (it->uuid == inv_item_id) found = true;
				if (it->attachpt == target_attachpt) found_for_this_point = true;
			}

			if (!found && !found_for_this_point)
			{
				AssetAndTarget at;
				at.uuid = inv_item_id;
				at.attachpt = target_attachpt;
				gAgent.mRRInterface.mReattachTimer.reset();
				gAgent.mRRInterface.mAssetsToReattach.push_back(at);
				// Little hack : store this item's asset id into the list of restrictions so they are automatically reapplied when it is reattached
				gAgent.mRRInterface.replace(object->getRootEdit()->getID(), inv_item_id);
			}
		}
		else
		{
			if (inv_item)
			{
				// Notify that this object has been detached
				gAgent.mRRInterface.notify (LLUUID::null, "detached legally " + getName(), "");
			}
		}
		gAgent.mRRInterface.mJustDetached.uuid.setNull();
		gAgent.mRRInterface.mJustDetached.attachpt = "";
		gAgent.mRRInterface.mJustReattached.uuid.setNull();
		gAgent.mRRInterface.mJustReattached.attachpt = "";
	}
//mk
	// force object visibile
	setAttachmentVisibility(TRUE);

	mAttachedObjects.erase(iter);
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
	if (mAttachedObjects.size() == 0)
	{
		mUpdateXform = FALSE;
	}
	object->setAttachmentItemID(LLUUID::null);
}

//-----------------------------------------------------------------------------
// setAttachmentVisibility()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::setAttachmentVisibility(BOOL visible)
{
	for (attachedobjs_vec_t::const_iterator iter = mAttachedObjects.begin();
		 iter != mAttachedObjects.end();
		 ++iter)
	{
		LLViewerObject *attached_obj = (*iter);
		if (!attached_obj || attached_obj->mDrawable.isNull() || 
			!(attached_obj->mDrawable->getSpatialBridge()))
			continue;
		
		if (visible)
		{
			// Hack to make attachments not visible by disabling their type mask!
			// This will break if you can ever attach non-volumes! - djs 02/14/03
			attached_obj->mDrawable->getSpatialBridge()->mDrawableType = 
				attached_obj->isHUDAttachment() ? LLPipeline::RENDER_TYPE_HUD : LLPipeline::RENDER_TYPE_VOLUME;
		}
		else
		{
			attached_obj->mDrawable->getSpatialBridge()->mDrawableType = 0;
		}
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
	for (attachedobjs_vec_t::const_iterator iter = mAttachedObjects.begin();
		 iter != mAttachedObjects.end();
		 ++iter)
	{
		if (LLViewerObject *attached_object = (*iter))
		{
			// *NOTE: object can drift when hitting maximum radius
			LLVector3 attachmentPos = attached_object->getPosition();
			F32 dist = attachmentPos.normVec();
			dist = llmin(dist, MAX_ATTACHMENT_DIST);
			attachmentPos *= dist;
			attached_object->setPosition(attachmentPos);
		}
	}
}

//-----------------------------------------------------------------------------
// calcLOD()
//-----------------------------------------------------------------------------
void LLViewerJointAttachment::calcLOD()
{
	F32 maxarea = 0;
	for (attachedobjs_vec_t::const_iterator iter = mAttachedObjects.begin();
		 iter != mAttachedObjects.end();
		 ++iter)
	{
		if (LLViewerObject *attached_object = (*iter))
		{
			maxarea = llmax(maxarea,attached_object->getMaxScale() * attached_object->getMidScale());
			LLViewerObject::const_child_list_t& child_list = attached_object->getChildren();
			for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
				 iter != child_list.end(); ++iter)
			{
				LLViewerObject* childp = *iter;
				F32 area = childp->getMaxScale() * childp->getMidScale();
				maxarea = llmax(maxarea, area);
			}
		}
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

BOOL LLViewerJointAttachment::isObjectAttached(const LLViewerObject *viewer_object) const
{
	for (attachedobjs_vec_t::const_iterator iter = mAttachedObjects.begin();
		 iter != mAttachedObjects.end();
		 ++iter)
	{
		const LLViewerObject* attached_object = (*iter);
		if (attached_object == viewer_object)
		{
			return TRUE;
		}
	}
	return FALSE;
}

const LLViewerObject *LLViewerJointAttachment::getAttachedObject(const LLUUID &object_id) const
{
	for (attachedobjs_vec_t::const_iterator iter = mAttachedObjects.begin();
		 iter != mAttachedObjects.end();
		 ++iter)
	{
		const LLViewerObject* attached_object = (*iter);
		if (attached_object->getAttachmentItemID() == object_id)
		{
			return attached_object;
		}
	}
	return NULL;
}

LLViewerObject *LLViewerJointAttachment::getAttachedObject(const LLUUID &object_id)
{
	for (attachedobjs_vec_t::iterator iter = mAttachedObjects.begin();
		 iter != mAttachedObjects.end();
		 ++iter)
	{
		LLViewerObject* attached_object = (*iter);
		if (attached_object->getAttachmentItemID() == object_id)
		{
			return attached_object;
		}
	}
	return NULL;
}
