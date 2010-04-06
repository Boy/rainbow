/** 
 * @file llpreview.cpp
 * @brief LLPreview class implementation
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
#include "stdenums.h"

#include "llpreview.h"
#include "lllineeditor.h"
#include "llinventory.h"
#include "llinventorymodel.h"
#include "llresmgr.h"
#include "lltextbox.h"
#include "llfocusmgr.h"
#include "lltooldraganddrop.h"
#include "llradiogroup.h"
#include "llassetstorage.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "lldbstrings.h"
#include "llfloatersearchreplace.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llagent.h"
#include "llvoavatar.h"
#include "llselectmgr.h"
#include "llinventoryview.h"
#include "llviewerinventory.h"

// Constants

// Globals and statics
LLPreview::preview_multimap_t LLPreview::sPreviewsBySource;
LLPreview::preview_map_t LLPreview::sInstances;
std::map<LLUUID, LLHandle<LLFloater> > LLMultiPreview::sAutoOpenPreviewHandles;

// Functions
LLPreview::LLPreview(const std::string& name) :
	LLFloater(name),
	mCopyToInvBtn(NULL),
	mForceClose(FALSE),
	mUserResized(FALSE),
	mCloseAfterSave(FALSE),
	mAssetStatus(PREVIEW_ASSET_UNLOADED),
	mItem(NULL),
	mDirty(TRUE)
{
	// don't add to instance list, since ItemID is null
	mAuxItem = new LLInventoryItem; // (LLPointer is auto-deleted)
	// don't necessarily steal focus on creation -- sometimes these guys pop up without user action
	setAutoFocus(FALSE);
	gInventory.addObserver(this);
}

LLPreview::LLPreview(const std::string& name, const LLRect& rect, const std::string& title, const LLUUID& item_uuid, const LLUUID& object_uuid, BOOL allow_resize, S32 min_width, S32 min_height, LLPointer<LLViewerInventoryItem> inv_item )
:	LLFloater(name, rect, title, allow_resize, min_width, min_height ),
	mItemUUID(item_uuid),
	mSourceID(LLUUID::null),
	mObjectUUID(object_uuid),
	mCopyToInvBtn( NULL ),
	mForceClose( FALSE ),
	mUserResized(FALSE),
	mCloseAfterSave(FALSE),
	mAssetStatus(PREVIEW_ASSET_UNLOADED),
	mItem(inv_item),
	mDirty(TRUE)
{
	mAuxItem = new LLInventoryItem;
	// don't necessarily steal focus on creation -- sometimes these guys pop up without user action
	setAutoFocus(FALSE);

	if (mItemUUID.notNull())
	{
		sInstances[mItemUUID] = this;
	}
	gInventory.addObserver(this);
}

LLPreview::~LLPreview()
{
	gFocusMgr.releaseFocusIfNeeded( this ); // calls onCommit()

	if (mItemUUID.notNull())
	{
		sInstances.erase( mItemUUID );
	}

	if (mSourceID.notNull())
	{
		preview_multimap_t::iterator found_it = sPreviewsBySource.find(mSourceID);
		for (; found_it != sPreviewsBySource.end(); ++found_it)
		{
			if (found_it->second == getHandle())
			{
				sPreviewsBySource.erase(found_it);
				break;
			}
		}
	}
	gInventory.removeObserver(this);
}

void LLPreview::setItemID(const LLUUID& item_id)
{
	if (mItemUUID.notNull())
	{
		sInstances.erase(mItemUUID);
	}

	mItemUUID = item_id;

	if (mItemUUID.notNull())
	{
		sInstances[mItemUUID] = this;
	}
}

void LLPreview::setObjectID(const LLUUID& object_id)
{
	mObjectUUID = object_id;
}

void LLPreview::setSourceID(const LLUUID& source_id)
{
	if (mSourceID.notNull())
	{
		// erase old one
		preview_multimap_t::iterator found_it = sPreviewsBySource.find(mSourceID);
		for (; found_it != sPreviewsBySource.end(); ++found_it)
		{
			if (found_it->second == getHandle())
			{
				sPreviewsBySource.erase(found_it);
				break;
			}
		}
	}
	mSourceID = source_id;
	sPreviewsBySource.insert(preview_multimap_t::value_type(mSourceID, getHandle()));
}

const LLViewerInventoryItem *LLPreview::getItem() const
{
	if(mItem)
		return mItem;
	const LLViewerInventoryItem *item = NULL;
	if(mObjectUUID.isNull())
	{
		// it's an inventory item, so get the item.
		item = gInventory.getItem(mItemUUID);
	}
	else
	{
		// it's an object's inventory item.
		LLViewerObject* object = gObjectList.findObject(mObjectUUID);
		if(object)
		{
			item = (LLViewerInventoryItem*)object->getInventoryObject(mItemUUID);
		}
	}
	return item;
}

// Sub-classes should override this function if they allow editing
void LLPreview::onCommit()
{
	const LLViewerInventoryItem *item = getItem();
	if(item)
	{
		if (!item->isComplete())
		{
			// We are attempting to save an item that was never loaded
			llwarns << "LLPreview::onCommit() called with mIsComplete == FALSE"
					<< " Type: " << item->getType()
					<< " ID: " << item->getUUID()
					<< llendl;
			return;
		}
		
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setDescription(childGetText("desc"));
		if(mObjectUUID.notNull())
		{
			// must be in an object
			LLViewerObject* object = gObjectList.findObject(mObjectUUID);
			if(object)
			{
				object->updateInventory(
					new_item,
					TASK_INVENTORY_ITEM_KEY,
					false);
			}
		}
		else if(item->getPermissions().getOwner() == gAgent.getID())
		{
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();

			// If the item is an attachment that is currently being worn,
			// update the object itself.
			if( item->getType() == LLAssetType::AT_OBJECT )
			{
				LLVOAvatar* avatar = gAgent.getAvatarObject();
				if( avatar )
				{
					LLViewerObject* obj = avatar->getWornAttachment( item->getUUID() );
					if( obj )
					{
						LLSelectMgr::getInstance()->deselectAll();
						LLSelectMgr::getInstance()->addAsIndividual( obj, SELECT_ALL_TES, FALSE );
						LLSelectMgr::getInstance()->selectionSetObjectDescription( childGetText("desc") );

						LLSelectMgr::getInstance()->deselectAll();
					}
				}
			}
		}
	}
}

void LLPreview::changed(U32 mask)
{
	mDirty = TRUE;
}

void LLPreview::draw()
{
	LLFloater::draw();
	if (mDirty)
	{
		mDirty = FALSE;
		const LLViewerInventoryItem *item = getItem();
		if (item)
		{
			refreshFromItem(item);
		}
	}
}

void LLPreview::refreshFromItem(const LLInventoryItem* item)
{
	setTitle(llformat("%s: %s",getTitleName(),item->getName().c_str()));
	childSetText("desc",item->getDescription());

	BOOL can_agent_manipulate = item->getPermissions().allowModifyBy(gAgent.getID());
	childSetEnabled("desc",can_agent_manipulate);
}

// static 
void LLPreview::onText(LLUICtrl*, void* userdata)
{
	LLPreview* self = (LLPreview*) userdata;
	self->onCommit();
}

// static
void LLPreview::onRadio(LLUICtrl*, void* userdata)
{
	LLPreview* self = (LLPreview*) userdata;
	self->onCommit();
}

// static
LLPreview* LLPreview::find(const LLUUID& item_uuid)
{
	LLPreview* instance = NULL;
	preview_map_t::iterator found_it = LLPreview::sInstances.find(item_uuid);
	if(found_it != LLPreview::sInstances.end())
	{
		instance = found_it->second;
	}
	return instance;
}

// static
LLPreview* LLPreview::show( const LLUUID& item_uuid, BOOL take_focus )
{
	LLPreview* instance = LLPreview::find(item_uuid);
	if(instance)
	{
		if (LLFloater::getFloaterHost() && LLFloater::getFloaterHost() != instance->getHost())
		{
			// this preview window is being opened in a new context
			// needs to be rehosted
			LLFloater::getFloaterHost()->addFloater(instance, TRUE);
		}
		instance->open();  /*Flawfinder: ignore*/
		if (take_focus)
		{
			instance->setFocus(TRUE);
		}
	}

	return instance;
}

// static
bool LLPreview::save( const LLUUID& item_uuid, LLPointer<LLInventoryItem>* itemptr )
{
	bool res = false;
	LLPreview* instance = LLPreview::find(item_uuid);
	if(instance)
	{
		res = instance->saveItem(itemptr);
	}
	if (!res)
	{
		delete itemptr;
	}
	return res;
}

// static
void LLPreview::hide(const LLUUID& item_uuid, BOOL no_saving /* = FALSE */ )
{
	preview_map_t::iterator found_it = LLPreview::sInstances.find(item_uuid);
	if(found_it != LLPreview::sInstances.end())
	{
		LLPreview* instance = found_it->second;

		if ( no_saving )
		{
			instance->mForceClose = TRUE;
		}

		instance->close();
	}
}

// static
void LLPreview::rename(const LLUUID& item_uuid, const std::string& new_name)
{
	preview_map_t::iterator found_it = LLPreview::sInstances.find(item_uuid);
	if(found_it != LLPreview::sInstances.end())
	{
		LLPreview* instance = found_it->second;
		instance->setTitle( new_name );
	}
}

BOOL LLPreview::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if(mClientRect.pointInRect(x, y))
	{
		// No handler needed for focus lost since this class has no
		// state that depends on it.
		bringToFront(x, y);
		gFocusMgr.setMouseCapture(this);
		S32 screen_x;
		S32 screen_y;
		localPointToScreen(x, y, &screen_x, &screen_y );
		LLToolDragAndDrop::getInstance()->setDragStart(screen_x, screen_y);
		return TRUE;
	}
	return LLFloater::handleMouseDown(x, y, mask);
}

BOOL LLPreview::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if(hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
		return TRUE;
	}
	return LLFloater::handleMouseUp(x, y, mask);
}

BOOL LLPreview::handleHover(S32 x, S32 y, MASK mask)
{
	if(hasMouseCapture())
	{
		S32 screen_x;
		S32 screen_y;
		const LLViewerInventoryItem *item = getItem();

		localPointToScreen(x, y, &screen_x, &screen_y );
		if(item
		   && item->getPermissions().allowCopyBy(gAgent.getID(),
												 gAgent.getGroupID())
		   && LLToolDragAndDrop::getInstance()->isOverThreshold(screen_x, screen_y))
		{
			EDragAndDropType type;
			type = LLAssetType::lookupDragAndDropType(item->getType());
			LLToolDragAndDrop::ESource src = LLToolDragAndDrop::SOURCE_LIBRARY;
			if(!mObjectUUID.isNull())
			{
				src = LLToolDragAndDrop::SOURCE_WORLD;
			}
			else if(item->getPermissions().getOwner() == gAgent.getID())
			{
				src = LLToolDragAndDrop::SOURCE_AGENT;
			}
			LLToolDragAndDrop::getInstance()->beginDrag(type,
										item->getUUID(),
										src,
										mObjectUUID);
			return LLToolDragAndDrop::getInstance()->handleHover(x, y, mask );
		}
	}
	return LLFloater::handleHover(x,y,mask);
}

void LLPreview::open()	/*Flawfinder: ignore*/
{
	if (!getFloaterHost() && !getHost() && getAssetStatus() == PREVIEW_ASSET_UNLOADED)
	{
		loadAsset();
	}
	LLFloater::open();		/*Flawfinder: ignore*/
}

// virtual
bool LLPreview::saveItem(LLPointer<LLInventoryItem>* itemptr)
{
	return false;
}


// static
void LLPreview::onBtnCopyToInv(void* userdata)
{
	LLPreview* self = (LLPreview*) userdata;
	LLInventoryItem *item = self->mAuxItem;

	if(item && item->getUUID().notNull())
	{
		// Copy to inventory
		if (self->mNotecardInventoryID.notNull())
		{
			copy_inventory_from_notecard(self->mObjectID,
				self->mNotecardInventoryID, item);
		}
		else
		{
			LLPointer<LLInventoryCallback> cb = NULL;
			copy_inventory_item(
				gAgent.getID(),
				item->getPermissions().getOwner(),
				item->getUUID(),
				LLUUID::null,
				std::string(),
				cb);
		}
	}
	self->close();
}

// static
void LLPreview::onKeepBtn(void* data)
{
	LLPreview* self = (LLPreview*)data;
	self->close();
}

// static
void LLPreview::onDiscardBtn(void* data)
{
	LLPreview* self = (LLPreview*)data;

	const LLViewerInventoryItem* item = self->getItem();
	if (!item) return;

	self->mForceClose = TRUE;
	self->close();

	// Delete the item entirely
	/*
	item->removeFromServer();
	gInventory.deleteObject(item->getUUID());
	gInventory.notifyObservers();
	*/

	// Move the item to the trash
	LLUUID trash_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_TRASH);
	if (item->getParentUUID() != trash_id)
	{
		LLInventoryModel::update_list_t update;
		LLInventoryModel::LLCategoryUpdate old_folder(item->getParentUUID(),-1);
		update.push_back(old_folder);
		LLInventoryModel::LLCategoryUpdate new_folder(trash_id, 1);
		update.push_back(new_folder);
		gInventory.accountForUpdate(update);

		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setParent(trash_id);
		// no need to restamp it though it's a move into trash because
		// it's a brand new item already.
		new_item->updateParentOnServer(FALSE);
		gInventory.updateItem(new_item);
		gInventory.notifyObservers();
	}
}

//static
LLPreview* LLPreview::getFirstPreviewForSource(const LLUUID& source_id)
{
	preview_multimap_t::iterator found_it = sPreviewsBySource.find(source_id);
	if (found_it != sPreviewsBySource.end())
	{
		// just return first one
		return (LLPreview*)found_it->second.get();
	}
	return NULL;
}

void LLPreview::userSetShape(const LLRect& new_rect)
{
	if(new_rect.getWidth() != getRect().getWidth() || new_rect.getHeight() != getRect().getHeight())
	{
		userResized();
	}
	LLFloater::userSetShape(new_rect);
}

//
// LLMultiPreview
//

LLMultiPreview::LLMultiPreview(const LLRect& rect) : LLMultiFloater(std::string("Preview"), rect)
{
	setCanResize(TRUE);
}

void LLMultiPreview::open()		/*Flawfinder: ignore*/
{
	LLMultiFloater::open();		/*Flawfinder: ignore*/
	LLPreview* frontmost_preview = (LLPreview*)mTabContainer->getCurrentPanel();
	if (frontmost_preview && frontmost_preview->getAssetStatus() == LLPreview::PREVIEW_ASSET_UNLOADED)
	{
		frontmost_preview->loadAsset();
	}
}


void LLMultiPreview::userSetShape(const LLRect& new_rect)
{
	if(new_rect.getWidth() != getRect().getWidth() || new_rect.getHeight() != getRect().getHeight())
	{
		LLPreview* frontmost_preview = (LLPreview*)mTabContainer->getCurrentPanel();
		if (frontmost_preview) frontmost_preview->userResized();
	}
	LLFloater::userSetShape(new_rect);
}


void LLMultiPreview::tabOpen(LLFloater* opened_floater, bool from_click)
{
	LLPreview* opened_preview = (LLPreview*)opened_floater;
	if (opened_preview && opened_preview->getAssetStatus() == LLPreview::PREVIEW_ASSET_UNLOADED)
	{
		opened_preview->loadAsset();
	}

	LLFloater* search_floater = LLFloaterSearchReplace::getInstance();
	if (search_floater && search_floater->getDependee() == this)
	{
		LLPreviewNotecard* notecard_preview; LLPreviewLSL* script_preview;
		if ((notecard_preview = dynamic_cast<LLPreviewNotecard*>(opened_preview)) != NULL)
		{
			LLFloaterSearchReplace::show(notecard_preview->getEditor());
		}
		else if ((script_preview = dynamic_cast<LLPreviewLSL*>(opened_preview)) != NULL)
		{
			LLFloaterSearchReplace::show(script_preview->getEditor());
		}
		else
		{
			search_floater->setVisible(FALSE);
		}
	}
}

//static 
LLMultiPreview* LLMultiPreview::getAutoOpenInstance(const LLUUID& id)
{
	handle_map_t::iterator found_it = sAutoOpenPreviewHandles.find(id);
	if (found_it != sAutoOpenPreviewHandles.end())
	{
		return (LLMultiPreview*)found_it->second.get();	
	}
	return NULL;
}

//static
void LLMultiPreview::setAutoOpenInstance(LLMultiPreview* previewp, const LLUUID& id)
{
	if (previewp)
	{
		sAutoOpenPreviewHandles[id] = previewp->getHandle();
	}
}

void LLPreview::setAssetId(const LLUUID& asset_id)
{
	const LLViewerInventoryItem* item = getItem();
	if(NULL == item)
	{
		return;
	}
	if(mObjectUUID.isNull())
	{
		// Update avatar inventory asset_id.
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setAssetUUID(asset_id);
		gInventory.updateItem(new_item);
		gInventory.notifyObservers();
	}
	else
	{
		// Update object inventory asset_id.
		LLViewerObject* object = gObjectList.findObject(mObjectUUID);
		if(NULL == object)
		{
			return;
		}
		object->updateViewerInventoryAsset(item, asset_id);
	}
}
