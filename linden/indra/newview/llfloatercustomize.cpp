/** 
 * @file llfloatercustomize.cpp
 * @brief The customize avatar floater, triggered by "Appearance..."
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

#include "llfloatercustomize.h"
#include "llfontgl.h"
#include "llbutton.h"
#include "lliconctrl.h"
#include "llresmgr.h"
#include "llmorphview.h"
#include "llfloatertools.h"
#include "llagent.h"
#include "lltoolmorph.h"
#include "llvoavatar.h"
#include "llradiogroup.h"
#include "lltoolmgr.h"
#include "llviewermenu.h"
#include "llscrollcontainer.h"
#include "llscrollingpanellist.h"
#include "llsliderctrl.h"
#include "lltabcontainervertical.h"
#include "llviewerwindow.h"
#include "llinventorymodel.h"
#include "llinventoryview.h"
#include "lltextbox.h"
#include "lllineeditor.h"
#include "llviewerimagelist.h"
#include "llfocusmgr.h"
#include "llviewerwindow.h"
#include "llviewercamera.h"
#include "llgenepool.h"
#include "llappearance.h"
#include "imageids.h"
#include "llmodaldialog.h"
#include "llassetstorage.h"
#include "lltexturectrl.h"
#include "lltextureentry.h"
#include "llwearablelist.h"
#include "llviewerinventory.h"
#include "lldbstrings.h"
#include "llcolorswatch.h"
#include "llglheaders.h"
#include "llui.h"
#include "llviewermessage.h"
#include "llimagejpeg.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"

#include "llfilepicker.h"

//*TODO:translate : The ui xml for this really needs to be integrated with the appearance paramaters

// Globals
LLFloaterCustomize* gFloaterCustomize = NULL;

const F32 PARAM_STEP_TIME_THRESHOLD = 0.25f;

/////////////////////////////////////////////////////////////////////
// LLUndoWearable

class LLUndoWearable
	:	public LLUndoBuffer::LLUndoAction
{
protected:
	LLAppearance			mAppearance;

protected:
	LLUndoWearable() {};
	virtual ~LLUndoWearable(){};

public:
	static LLUndoAction *create()	{ return new LLUndoWearable(); }

	void			setVisualParam(S32 param_id, F32 weight);
	void			setColor( LLVOAvatar::ETextureIndex te, const LLColor4& color );
	void			setTexture( LLVOAvatar::ETextureIndex te, const LLUUID& asset_id );
	void			setWearable( EWearableType type );

	virtual void	undo() {applyUndoRedo();}
	virtual void	redo() {applyUndoRedo();}
	void			applyUndoRedo();
};


/////////////////////////////////////////////////////////////////////
// LLFloaterCustomizeObserver

class LLFloaterCustomizeObserver : public LLInventoryObserver
{
public:
	LLFloaterCustomizeObserver(LLFloaterCustomize* fc) : mFC(fc) {}
	virtual ~LLFloaterCustomizeObserver() {}
	virtual void changed(U32 mask) { mFC->updateScrollingPanelUI(); }
protected:
	LLFloaterCustomize* mFC;
};

////////////////////////////////////////////////////////////////////////////

// Local Constants 

class LLWearableSaveAsDialog : public LLModalDialog
{
private:
	std::string	mItemName;
	void		(*mCommitCallback)(LLWearableSaveAsDialog*,void*);
	void*		mCallbackUserData;

public:
	LLWearableSaveAsDialog( const std::string& desc, void(*commit_cb)(LLWearableSaveAsDialog*,void*), void* userdata )
		: LLModalDialog( LLStringUtil::null, 240, 100 ),
		  mCommitCallback( commit_cb ),
		  mCallbackUserData( userdata )
	{
		LLUICtrlFactory::getInstance()->buildFloater(this, "floater_wearable_save_as.xml");
		
		childSetAction("Save", LLWearableSaveAsDialog::onSave, this );
		childSetAction("Cancel", LLWearableSaveAsDialog::onCancel, this );
		childSetTextArg("name ed", "[DESC]", desc);
	}

	virtual void startModal()
	{
		LLModalDialog::startModal();
		LLLineEditor* edit = getChild<LLLineEditor>("name ed");
		if (!edit) return;
		edit->setFocus(TRUE);
		edit->selectAll();
	}

	const std::string& getItemName() { return mItemName; }

	static void onSave( void* userdata )
	{
		LLWearableSaveAsDialog* self = (LLWearableSaveAsDialog*) userdata;
		self->mItemName = self->childGetValue("name ed").asString();
		LLStringUtil::trim(self->mItemName);
		if( !self->mItemName.empty() )
		{
			if( self->mCommitCallback )
			{
				self->mCommitCallback( self, self->mCallbackUserData );
			}
			self->close(); // destroys this object
		}
	}

	static void onCancel( void* userdata )
	{
		LLWearableSaveAsDialog* self = (LLWearableSaveAsDialog*) userdata;
		self->close(); // destroys this object
	}
};

////////////////////////////////////////////////////////////////////////////

BOOL edit_wearable_for_teens(EWearableType type)
{
	switch(type)
	{
	case WT_UNDERSHIRT:
	case WT_UNDERPANTS:
		return FALSE;
	default:
		return TRUE;
	}
}

class LLMakeOutfitDialog : public LLModalDialog
{
private:
	std::string	mFolderName;
	void		(*mCommitCallback)(LLMakeOutfitDialog*,void*);
	void*		mCallbackUserData;
	std::vector<std::pair<std::string,S32> > mCheckBoxList;
	
public:
	LLMakeOutfitDialog( void(*commit_cb)(LLMakeOutfitDialog*,void*), void* userdata )
		: LLModalDialog(LLStringUtil::null,515, 510, TRUE ),
		  mCommitCallback( commit_cb ),
		  mCallbackUserData( userdata )
	{
		LLUICtrlFactory::getInstance()->buildFloater(this, "floater_new_outfit_dialog.xml");
		
		// Build list of check boxes
		for( S32 i = 0; i < WT_COUNT; i++ )
		{
			std::string name = std::string("checkbox_") + LLWearable::typeToTypeLabel( (EWearableType)i );
			mCheckBoxList.push_back(std::make_pair(name,i));
			// Hide teen items
			if (gAgent.isTeen() &&
				!edit_wearable_for_teens((EWearableType)i))
			{
				// hide wearable checkboxes that don't apply to this account
				std::string name = std::string("checkbox_") + LLWearable::typeToTypeLabel( (EWearableType)i );
				childSetVisible(name, FALSE);
			}
		}

		// NOTE: .xml needs to be updated if attachments are added or their names are changed!
		LLVOAvatar* avatar = gAgent.getAvatarObject();
		if( avatar )
		{
			for (LLVOAvatar::attachment_map_t::iterator iter = avatar->mAttachmentPoints.begin(); 
				 iter != avatar->mAttachmentPoints.end(); )
			{
				LLVOAvatar::attachment_map_t::iterator curiter = iter++;
				LLViewerJointAttachment* attachment = curiter->second;
				S32	attachment_pt = curiter->first;	
				BOOL object_attached = ( attachment->getNumObjects() > 0 );
				std::string name = std::string("checkbox_") + attachment->getName();
				mCheckBoxList.push_back(std::make_pair(name,attachment_pt));
				childSetEnabled(name, object_attached);
			}
		}

		childSetAction("Save", onSave, this ); 
		childSetAction("Cancel", onCancel, this ); 
	}

	BOOL getRenameClothing()
	{
		return childGetValue("rename").asBoolean();

	}
	virtual void draw()
	{
		BOOL one_or_more_items_selected = FALSE;
		for( S32 i = 0; i < (S32)mCheckBoxList.size(); i++ )
		{
			if( childGetValue(mCheckBoxList[i].first).asBoolean() )
			{
				one_or_more_items_selected = TRUE;
				break;
			}
		}

		childSetEnabled("Save", one_or_more_items_selected );
		
		LLModalDialog::draw();
	}

	const std::string& getFolderName() { return mFolderName; }

	void setWearableToInclude( S32 wearable, S32 enabled, S32 selected )
	{
		if( (0 <= wearable) && (wearable < WT_COUNT) )
		{
			std::string name = std::string("checkbox_") + LLWearable::typeToTypeLabel( (EWearableType)wearable );
			childSetEnabled(name, enabled);
			childSetValue(name, selected);
		}
	}

	void getIncludedItems( LLDynamicArray<S32> &wearables_to_include, LLDynamicArray<S32> &attachments_to_include )
	{
		for( S32 i = 0; i < (S32)mCheckBoxList.size(); i++)
		{
			std::string name = mCheckBoxList[i].first;
			BOOL checked = childGetValue(name).asBoolean();
			if (i < WT_COUNT )
			{
				if( checked )
				{
					wearables_to_include.put(i);
				}
			}
			else
			{
				if( checked )
				{
					S32 attachment_pt = mCheckBoxList[i].second;
					attachments_to_include.put( attachment_pt );
				}
			}
		}
	}

	static void onSave( void* userdata )
	{
		LLMakeOutfitDialog* self = (LLMakeOutfitDialog*) userdata;
		self->mFolderName = self->childGetValue("name ed").asString();
		LLStringUtil::trim(self->mFolderName);
		if( !self->mFolderName.empty() )
		{
			if( self->mCommitCallback )
			{
				self->mCommitCallback( self, self->mCallbackUserData );
			}
			self->close(); // destroys this object
		}
	}

	static void onCancel( void* userdata )
	{
		LLMakeOutfitDialog* self = (LLMakeOutfitDialog*) userdata;
		self->close(); // destroys this object
	}
};

/////////////////////////////////////////////////////////////////////
// LLPanelEditWearable

enum ESubpart {
	SUBPART_SHAPE_HEAD = 1, // avoid 0
	SUBPART_SHAPE_EYES,
	SUBPART_SHAPE_EARS,
	SUBPART_SHAPE_NOSE,
	SUBPART_SHAPE_MOUTH,
	SUBPART_SHAPE_CHIN,
	SUBPART_SHAPE_TORSO,
	SUBPART_SHAPE_LEGS,
	SUBPART_SHAPE_WHOLE,
	SUBPART_SHAPE_DETAIL,
	SUBPART_SKIN_COLOR,
	SUBPART_SKIN_FACEDETAIL,
	SUBPART_SKIN_MAKEUP,
	SUBPART_SKIN_BODYDETAIL,
	SUBPART_HAIR_COLOR,
	SUBPART_HAIR_STYLE,
	SUBPART_HAIR_EYEBROWS,
	SUBPART_HAIR_FACIAL,
	SUBPART_EYES,
	SUBPART_SHIRT,
	SUBPART_PANTS,
	SUBPART_SHOES,
	SUBPART_SOCKS,
	SUBPART_JACKET,
	SUBPART_GLOVES,
	SUBPART_UNDERSHIRT,
	SUBPART_UNDERPANTS,
	SUBPART_SKIRT
 };

struct LLSubpart
{
	LLSubpart() : mSex( SEX_BOTH ) {}

	std::string			mButtonName;
	std::string			mTargetJoint;
	std::string			mEditGroup;
	LLVector3d			mTargetOffset;
	LLVector3d			mCameraOffset;
	ESex				mSex;
};

////////////////////////////////////////////////////////////////////////////

class LLPanelEditWearable : public LLPanel, public LLEditMenuHandler
{
public:
	LLPanelEditWearable( EWearableType type );
	virtual ~LLPanelEditWearable();

	virtual BOOL 		postBuild();
	virtual void		draw();
	virtual BOOL		isDirty() const;	// LLUICtrl
	
	void				addSubpart(const std::string& name, ESubpart id, LLSubpart* part );
	void				addTextureDropTarget( LLVOAvatar::ETextureIndex te, const std::string& name, const LLUUID& default_image_id, BOOL allow_no_texture );
	void				addColorSwatch( LLVOAvatar::ETextureIndex te, const std::string& name );

	const std::string&	getLabel()	{ return LLWearable::typeToTypeLabel( mType ); }
	EWearableType		getType()	{ return mType; }

	LLSubpart*			getCurrentSubpart() { return mSubpartList[mCurrentSubpart]; }
	ESubpart			getDefaultSubpart();
	void				setSubpart( ESubpart subpart );
	void				switchToDefaultSubpart();

	void 				setWearable(LLWearable* wearable, U32 perm_mask, BOOL is_complete);

	void				addVisualParamToUndoBuffer( S32 param_id, F32 current_weight );

	void 				setUIPermissions(U32 perm_mask, BOOL is_complete);
	
	virtual void		setVisible( BOOL visible );

	// Inherted methods from LLEditMenuHandler
	virtual void		undo()			{ mUndoBuffer->undoAction(); }
	virtual BOOL		canUndo() const	{ return mUndoBuffer->canUndo(); }
	virtual void		redo()			{ mUndoBuffer->redoAction(); }
	virtual BOOL		canRedo() const	{ return mUndoBuffer->canRedo(); }

	// Callbacks
	static void			onBtnSubpart( void* userdata );
	static void			onBtnTakeOff( void* userdata );
	static void			onBtnRandomize( void* userdata );
	static void			onBtnSave( void* userdata );

	static void			onBtnSaveAs( void* userdata );
	static void			onSaveAsCommit( LLWearableSaveAsDialog* save_as_dialog, void* userdata );

	static void			onBtnRevert( void* userdata );
	static void			onBtnTakeOffDialog( S32 option, void* userdata );
	static void			onBtnCreateNew( void* userdata );
	static void			onTextureCommit( LLUICtrl* ctrl, void* userdata );
	static void			onColorCommit( LLUICtrl* ctrl, void* userdata );
	static void			onCommitSexChange( LLUICtrl*, void* userdata );
	static void			onSelectAutoWearOption(S32 option, void* data);



private:
	EWearableType		mType;
	BOOL				mCanTakeOff;
	std::map<std::string, S32> mTextureList;
	std::map<std::string, S32> mColorList;
	std::map<ESubpart, LLSubpart*> mSubpartList;
	ESubpart			mCurrentSubpart;
	LLUndoBuffer*		mUndoBuffer;
};

////////////////////////////////////////////////////////////////////////////

LLPanelEditWearable::LLPanelEditWearable( EWearableType type )
	: LLPanel( LLWearable::typeToTypeLabel( type ) ),
	  mType( type )
{
	const S32 NUM_DISTORTION_UNDO_ENTRIES = 50;
	mUndoBuffer = new LLUndoBuffer(LLUndoWearable::create, NUM_DISTORTION_UNDO_ENTRIES);
}

BOOL LLPanelEditWearable::postBuild()
{
	LLAssetType::EType asset_type = LLWearable::typeToAssetType( mType );
	std::string icon_name = (asset_type == LLAssetType::AT_CLOTHING ?
										 "inv_item_clothing.tga" :
										 "inv_item_skin.tga" );
	childSetValue("icon", icon_name);

	childSetAction("Create New", LLPanelEditWearable::onBtnCreateNew, this );

	// If PG, can't take off underclothing or shirt
	mCanTakeOff =
		LLWearable::typeToAssetType( mType ) == LLAssetType::AT_CLOTHING &&
		!( gAgent.isTeen() && (mType == WT_UNDERSHIRT || mType == WT_UNDERPANTS) );
	childSetVisible("Take Off", mCanTakeOff);
	childSetAction("Take Off", LLPanelEditWearable::onBtnTakeOff, this );

	childSetAction("Save",  &LLPanelEditWearable::onBtnSave, (void*)this );

	childSetAction("Save As", &LLPanelEditWearable::onBtnSaveAs, (void*)this );

	childSetAction("Revert", &LLPanelEditWearable::onBtnRevert, (void*)this );

	return TRUE;
}

LLPanelEditWearable::~LLPanelEditWearable()
{
	delete mUndoBuffer;

	std::for_each(mSubpartList.begin(), mSubpartList.end(), DeletePairedPointer());

	// Route menu back to the default
	if( gEditMenuHandler == this )
	{
		gEditMenuHandler = NULL;
	}
}

void LLPanelEditWearable::addSubpart( const std::string& name, ESubpart id, LLSubpart* part )
{
	if (!name.empty())
	{
		childSetAction(name, &LLPanelEditWearable::onBtnSubpart, (void*)id);
		part->mButtonName = name;
	}
	mSubpartList[id] = part;
	
}

// static
void LLPanelEditWearable::onBtnSubpart(void* userdata)
{
	LLFloaterCustomize* floater_customize = gFloaterCustomize;
	if (!floater_customize) return;
	LLPanelEditWearable* self = floater_customize->getCurrentWearablePanel();
	if (!self) return;
	ESubpart subpart = (ESubpart) (intptr_t)userdata;
	self->setSubpart( subpart );
}

void LLPanelEditWearable::setSubpart( ESubpart subpart )
{
	mCurrentSubpart = subpart;

	for (std::map<ESubpart, LLSubpart*>::iterator iter = mSubpartList.begin();
		 iter != mSubpartList.end(); ++iter)
	{
		LLButton* btn = getChild<LLButton>(iter->second->mButtonName);
		if (btn)
		{
			btn->setToggleState( subpart == iter->first );
		}
	}

	LLSubpart* part = get_if_there(mSubpartList, (ESubpart)subpart, (LLSubpart*)NULL);
	if( part )
	{
		// Update the thumbnails we display
		LLFloaterCustomize::param_map sorted_params;
		LLVOAvatar* avatar = gAgent.getAvatarObject();
		ESex avatar_sex = avatar->getSex();
		LLViewerInventoryItem* item;
		item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem(mType);
		U32 perm_mask = 0x0;
		BOOL is_complete = FALSE;
		if(item)
		{
			perm_mask = item->getPermissions().getMaskOwner();
			is_complete = item->isComplete();
		}
		setUIPermissions(perm_mask, is_complete);
		BOOL editable = ((perm_mask & PERM_MODIFY) && is_complete) ? TRUE : FALSE;
		
		for(LLViewerVisualParam* param = (LLViewerVisualParam *)avatar->getFirstVisualParam(); 
			param; 
			param = (LLViewerVisualParam *)avatar->getNextVisualParam())
		{
			if (param->getID() == -1
				|| param->getGroup() != VISUAL_PARAM_GROUP_TWEAKABLE 
				|| param->getEditGroup() != part->mEditGroup 
				|| !(param->getSex() & avatar_sex))
			{
				continue;
			}

			// negative getDisplayOrder() to make lowest order the highest priority
			LLFloaterCustomize::param_map::value_type vt(-param->getDisplayOrder(), LLFloaterCustomize::editable_param(editable, param));
			llassert( sorted_params.find(-param->getDisplayOrder()) == sorted_params.end() );  // Check for duplicates
			sorted_params.insert(vt);
		}
		gFloaterCustomize->generateVisualParamHints(NULL, sorted_params);
		gFloaterCustomize->updateScrollingPanelUI();


		// Update the camera
		gMorphView->setCameraTargetJoint( gAgent.getAvatarObject()->getJoint( part->mTargetJoint ) );
		gMorphView->setCameraTargetOffset( part->mTargetOffset );
		gMorphView->setCameraOffset( part->mCameraOffset );
		gMorphView->setCameraDistToDefault();
		if (gSavedSettings.getBOOL("AppearanceCameraMovement"))
		{
			gMorphView->updateCamera();
		}
	}
}

// static
void LLPanelEditWearable::onBtnRandomize( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem(self->mType);
	if(avatar
	   && item
	   && item->getPermissions().allowModifyBy(gAgent.getID(), gAgent.getGroupID())
	   && item->isComplete())
	{
		// Save current wearable parameters and textures to the undo buffer.
		LLUndoWearable* action = (LLUndoWearable*)(self->mUndoBuffer->getNextAction());
		action->setWearable( self->mType );

		ESex old_sex = avatar->getSex();

		gFloaterCustomize->spawnWearableAppearance( self->mType );
		
		gFloaterCustomize->updateScrollingPanelList(TRUE);

		ESex new_sex = avatar->getSex();
		if( old_sex != new_sex )
		{
			// Updates radio button
			gSavedSettings.setU32("AvatarSex", (new_sex == SEX_MALE) );

			// Assumes that we're in the "Shape" Panel.
			self->setSubpart( SUBPART_SHAPE_WHOLE );
		}	
	}
}


// static
void LLPanelEditWearable::onBtnTakeOff( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	
	LLWearable* wearable = gAgent.getWearable( self->mType );
	if( !wearable )
	{
		return;
	}

	gAgent.removeWearable( self->mType );
}

// static
void LLPanelEditWearable::onBtnSave( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	gAgent.saveWearable( self->mType );
}

// static
void LLPanelEditWearable::onBtnSaveAs( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	LLWearable* wearable = gAgent.getWearable( self->getType() );
	if( wearable )
	{
		LLWearableSaveAsDialog* save_as_dialog = new LLWearableSaveAsDialog( wearable->getName(), onSaveAsCommit, self );
		save_as_dialog->startModal();
		// LLWearableSaveAsDialog deletes itself.
	}
}

// static
void LLPanelEditWearable::onSaveAsCommit( LLWearableSaveAsDialog* save_as_dialog, void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( avatar )
	{
		gAgent.saveWearableAs( self->getType(), save_as_dialog->getItemName(), FALSE );
	}
}


// static
void LLPanelEditWearable::onBtnRevert( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	gAgent.revertWearable( self->mType );
}

// static
void LLPanelEditWearable::onBtnCreateNew( void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	gViewerWindow->alertXml("AutoWearNewClothing", onSelectAutoWearOption, self);
}
void LLPanelEditWearable::onSelectAutoWearOption(S32 option, void* data)
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) data;
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if(avatar)
	{
		// Create a new wearable in the default folder for the wearable's asset type.
		LLWearable* wearable = gWearableList.createNewWearable( self->getType() );
		LLAssetType::EType asset_type = wearable->getAssetType();

		LLUUID folder_id;
		// regular UI, items get created in normal folder
		folder_id = gInventory.findCategoryUUIDForType(asset_type);

		// Only auto wear the new item if the AutoWearNewClothing checkbox is selected.
		LLPointer<LLInventoryCallback> cb = option == 0 ? 
			new WearOnAvatarCallback : NULL;
		create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
			folder_id, wearable->getTransactionID(), wearable->getName(), wearable->getDescription(),
			asset_type, LLInventoryType::IT_WEARABLE, wearable->getType(),
			wearable->getPermissions().getMaskNextOwner(), cb);
	}
}
void LLPanelEditWearable::addColorSwatch( LLVOAvatar::ETextureIndex te, const std::string& name )
{
	childSetCommitCallback(name, LLPanelEditWearable::onColorCommit, this);
	mColorList[name] = te;
}

// static
void LLPanelEditWearable::onColorCommit( LLUICtrl* ctrl, void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	LLColorSwatchCtrl* color_ctrl = (LLColorSwatchCtrl*) ctrl;

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( avatar )
	{
		LLVOAvatar::ETextureIndex te = (LLVOAvatar::ETextureIndex)(self->mColorList[ctrl->getName()]);

		LLColor4 old_color = avatar->getClothesColor( te );
		const LLColor4& new_color = color_ctrl->get();
		if( old_color != new_color )
		{
			// Save the old version to the undo stack
			LLUndoWearable* action = (LLUndoWearable*)(self->mUndoBuffer->getNextAction());
			action->setColor( te, old_color );
			
			// Set the new version
			avatar->setClothesColor( te, new_color, TRUE );
			gAgent.sendAgentSetAppearance();

			LLVisualParamHint::requestHintUpdates();
		}
	}
}


void LLPanelEditWearable::addTextureDropTarget( LLVOAvatar::ETextureIndex te, const std::string& name,
												const LLUUID& default_image_id, BOOL allow_no_texture )
{
	childSetCommitCallback(name, LLPanelEditWearable::onTextureCommit, this);
	LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>(name);
	if (texture_ctrl)
	{
		texture_ctrl->setDefaultImageAssetID(default_image_id);
		texture_ctrl->setAllowNoTexture( allow_no_texture );
		// Don't allow (no copy) or (no transfer) textures to be selected.
		texture_ctrl->setImmediateFilterPermMask(PERM_NONE);//PERM_COPY | PERM_TRANSFER);
		texture_ctrl->setNonImmediateFilterPermMask(PERM_NONE);//PERM_COPY | PERM_TRANSFER);
	}
	mTextureList[name] = te;
}

// static
void LLPanelEditWearable::onTextureCommit( LLUICtrl* ctrl, void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;
	LLTextureCtrl* texture_ctrl = (LLTextureCtrl*) ctrl;

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( avatar )
	{
		LLVOAvatar::ETextureIndex te = (LLVOAvatar::ETextureIndex)(self->mTextureList[ctrl->getName()]);

		// Save the old version to the undo stack
		LLViewerImage* existing_image = avatar->getTEImage( te );
		if( existing_image )
		{
			LLUndoWearable* action = (LLUndoWearable*)(self->mUndoBuffer->getNextAction());
			action->setTexture( te, existing_image->getID() );
		}
		
		// Set the new version
		LLViewerImage* image = gImageList.getImage( texture_ctrl->getImageAssetID() );
		if( image->getID().isNull() )
		{
			image = gImageList.getImage(IMG_DEFAULT_AVATAR);
		}
		avatar->setLocTexTE( te, image, TRUE );
		gAgent.sendAgentSetAppearance();
	}
}


ESubpart LLPanelEditWearable::getDefaultSubpart()
{
	switch( mType )
	{
		case WT_SHAPE:		return SUBPART_SHAPE_WHOLE;
		case WT_SKIN:		return SUBPART_SKIN_COLOR;
		case WT_HAIR:		return SUBPART_HAIR_COLOR;
		case WT_EYES:		return SUBPART_EYES;
		case WT_SHIRT:		return SUBPART_SHIRT;
		case WT_PANTS:		return SUBPART_PANTS;
		case WT_SHOES:		return SUBPART_SHOES;
		case WT_SOCKS:		return SUBPART_SOCKS;
		case WT_JACKET:		return SUBPART_JACKET;
		case WT_GLOVES:		return SUBPART_GLOVES;
		case WT_UNDERSHIRT:	return SUBPART_UNDERSHIRT;
		case WT_UNDERPANTS:	return SUBPART_UNDERPANTS;
		case WT_SKIRT:		return SUBPART_SKIRT;

		default:	llassert(0);		return SUBPART_SHAPE_WHOLE;
	}
}


void LLPanelEditWearable::draw()
{
	if( gFloaterCustomize->isMinimized() )
	{
		return;
	}

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( !avatar )
	{
		return;
	}

	if( gFloaterCustomize->isFrontmost() && !gFocusMgr.getKeyboardFocus() )
	{
		// Route menu to this class
		gEditMenuHandler = this;
	}

	LLWearable* wearable = gAgent.getWearable( mType );
	BOOL has_wearable = (wearable != NULL );
	BOOL is_dirty = isDirty();
	BOOL is_modifiable = FALSE;
	BOOL is_copyable = FALSE;
	BOOL is_complete = FALSE;
	LLViewerInventoryItem* item;
	item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem(mType);
	if(item)
	{
		const LLPermissions& perm = item->getPermissions();
		is_modifiable = perm.allowModifyBy(gAgent.getID(), gAgent.getGroupID());
		is_copyable = perm.allowCopyBy(gAgent.getID(), gAgent.getGroupID());
		is_complete = item->isComplete();
	}

	childSetEnabled("Save", is_modifiable && is_complete && has_wearable && is_dirty);
	childSetEnabled("Save As", is_copyable && is_complete && has_wearable);
	childSetEnabled("Revert", has_wearable && is_dirty );
	childSetEnabled("Take Off",  has_wearable );
	childSetVisible("Take Off", mCanTakeOff  );
	childSetVisible("Create New", !has_wearable );

	childSetVisible("not worn instructions",  !has_wearable );
	childSetVisible("no modify instructions",  has_wearable && !is_modifiable);

	for (std::map<ESubpart, LLSubpart*>::iterator iter = mSubpartList.begin();
		 iter != mSubpartList.end(); ++iter)
	{
		if( has_wearable && is_complete && is_modifiable )
		{
			childSetEnabled(iter->second->mButtonName, iter->second->mSex & avatar->getSex() );
		}
		else
		{
			childSetEnabled(iter->second->mButtonName, FALSE );
		}
	}

	childSetVisible("square", !is_modifiable);

	childSetVisible("title", FALSE);
	childSetVisible("title_no_modify", FALSE);
	childSetVisible("title_not_worn", FALSE);
	childSetVisible("title_loading", FALSE);

	childSetVisible("path", FALSE);
	
	if(has_wearable && !is_modifiable)
	{
		// *TODO:Translate
		childSetVisible("title_no_modify", TRUE);
		childSetTextArg("title_no_modify", "[DESC]", std::string(LLWearable::typeToTypeLabel( mType )));
		
		for( std::map<std::string, S32>::iterator iter = mTextureList.begin();
			 iter != mTextureList.end(); ++iter )
		{
			childSetVisible(iter->first, FALSE );
		}
		for( std::map<std::string, S32>::iterator iter = mColorList.begin();
			 iter != mColorList.end(); ++iter )
		{
			childSetVisible(iter->first, FALSE );
		}
	}
	else if(has_wearable && !is_complete)
	{
		// *TODO:Translate
		childSetVisible("title_loading", TRUE);
		childSetTextArg("title_loading", "[DESC]", std::string(LLWearable::typeToTypeLabel( mType )));
			
		std::string path;
		const LLUUID& item_id = gAgent.getWearableItem( wearable->getType() );
		gInventory.appendPath(item_id, path);
		childSetVisible("path", TRUE);
		childSetTextArg("path", "[PATH]", path);

		for( std::map<std::string, S32>::iterator iter = mTextureList.begin();
			 iter != mTextureList.end(); ++iter )
		{
			childSetVisible(iter->first, FALSE );
		}
		for( std::map<std::string, S32>::iterator iter = mColorList.begin();
			 iter != mColorList.end(); ++iter )
		{
			childSetVisible(iter->first, FALSE );
		}
	}
	else if(has_wearable && is_modifiable)
	{
		childSetVisible("title", TRUE);
		childSetTextArg("title", "[DESC]", wearable->getName() );

		std::string path;
		const LLUUID& item_id = gAgent.getWearableItem( wearable->getType() );
		gInventory.appendPath(item_id, path);
		childSetVisible("path", TRUE);
		childSetTextArg("path", "[PATH]", path);

		for( std::map<std::string, S32>::iterator iter = mTextureList.begin();
			 iter != mTextureList.end(); ++iter )
		{
			std::string name = iter->first;
			LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>(name);
			S32 te_index = iter->second;
			childSetVisible(name, is_copyable && is_modifiable && is_complete );
			if (texture_ctrl)
			{
				const LLTextureEntry* te = avatar->getTE(te_index);
				if( te && (te->getID() != IMG_DEFAULT_AVATAR) )
				{
					texture_ctrl->setImageAssetID( te->getID() );
				}
				else
				{
					texture_ctrl->setImageAssetID( LLUUID::null );
				}
			}
		}

		for( std::map<std::string, S32>::iterator iter = mColorList.begin();
			 iter != mColorList.end(); ++iter )
		{
			std::string name = iter->first;
			S32 te_index = iter->second;
			childSetVisible(name, is_modifiable && is_complete );
			childSetEnabled(name, is_modifiable && is_complete );
			LLColorSwatchCtrl* ctrl = getChild<LLColorSwatchCtrl>(name);
			if (ctrl)
			{
				ctrl->set(avatar->getClothesColor( (LLVOAvatar::ETextureIndex)te_index ) );
			}
		}
	}
	else
	{
		// *TODO:Translate
		childSetVisible("title_not_worn", TRUE);
		childSetTextArg("title_not_worn", "[DESC]", std::string(LLWearable::typeToTypeLabel( mType )));

		for( std::map<std::string, S32>::iterator iter = mTextureList.begin();
			 iter != mTextureList.end(); ++iter )
		{
			childSetVisible(iter->first, FALSE );
		}
		for( std::map<std::string, S32>::iterator iter = mColorList.begin();
			 iter != mColorList.end(); ++iter )
		{
			childSetVisible(iter->first, FALSE );
		}
	}
	
	childSetVisible("icon", has_wearable && is_modifiable);

	LLPanel::draw();
}

void LLPanelEditWearable::setWearable(LLWearable* wearable, U32 perm_mask, BOOL is_complete)
{
	if( wearable )
	{
		setUIPermissions(perm_mask, is_complete);
	}
	mUndoBuffer->flushActions();
}

void LLPanelEditWearable::addVisualParamToUndoBuffer( S32 param_id, F32 current_weight )
{
	LLUndoWearable* action = (LLUndoWearable*)(mUndoBuffer->getNextAction());
	action->setVisualParam( param_id, current_weight );
}

void LLPanelEditWearable::switchToDefaultSubpart()
{
	setSubpart( getDefaultSubpart() );
}

void LLPanelEditWearable::setVisible(BOOL visible)
{
	LLPanel::setVisible( visible );
	if( !visible )
	{
		// Route menu back to the default
		if( gEditMenuHandler == this )
		{
			gEditMenuHandler = NULL;
		}

		for( std::map<std::string, S32>::iterator iter = mColorList.begin();
			 iter != mColorList.end(); ++iter )
		{
			// this forces any open color pickers to cancel their selection
			childSetEnabled(iter->first, FALSE );
		}
	}
}

BOOL LLPanelEditWearable::isDirty() const
{
	LLWearable* wearable = gAgent.getWearable( mType );
	if( !wearable )
	{
		return FALSE;
	}

	if( wearable->isDirty() )
	{
		return TRUE;
	}

	return FALSE;
}

// static
void LLPanelEditWearable::onCommitSexChange( LLUICtrl*, void* userdata )
{
	LLPanelEditWearable* self = (LLPanelEditWearable*) userdata;

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (!avatar)
	{
		return;
	}

	if( !gAgent.isWearableModifiable(self->mType))
	{
		return;
	}

	ESex new_sex = gSavedSettings.getU32("AvatarSex") ? SEX_MALE : SEX_FEMALE;

	LLViewerVisualParam* param = (LLViewerVisualParam*)avatar->getVisualParam( "male" );
	if( !param )
	{
		return;
	}

	self->addVisualParamToUndoBuffer( param->getID(), param->getWeight() ); 
	param->setWeight( (new_sex == SEX_MALE), TRUE );

	avatar->updateSexDependentLayerSets( TRUE );

	avatar->updateVisualParams();

	gFloaterCustomize->clearScrollingPanelList();

	// Assumes that we're in the "Shape" Panel.
	self->setSubpart( SUBPART_SHAPE_WHOLE );

	gAgent.sendAgentSetAppearance();
}

void LLPanelEditWearable::setUIPermissions(U32 perm_mask, BOOL is_complete)
{
	BOOL is_copyable = (perm_mask & PERM_COPY) ? TRUE : FALSE;
	BOOL is_modifiable = (perm_mask & PERM_MODIFY) ? TRUE : FALSE;

	childSetEnabled("Save", is_modifiable && is_complete);
	childSetEnabled("Save As", is_copyable && is_complete);
	childSetEnabled("Randomize", is_modifiable && is_complete);
	childSetEnabled("sex radio", is_modifiable && is_complete);
	for( std::map<std::string, S32>::iterator iter = mTextureList.begin();
		 iter != mTextureList.end(); ++iter )
	{
		childSetVisible(iter->first, is_copyable && is_modifiable && is_complete );
	}
	for( std::map<std::string, S32>::iterator iter = mColorList.begin();
		 iter != mColorList.end(); ++iter )
	{
		childSetVisible(iter->first, is_modifiable && is_complete );
	}
}

/////////////////////////////////////////////////////////////////////
// LLScrollingPanelParam

class LLScrollingPanelParam : public LLScrollingPanel
{
public:
	LLScrollingPanelParam( const std::string& name, LLViewerJointMesh* mesh, LLViewerVisualParam* param, BOOL allow_modify );
	virtual ~LLScrollingPanelParam();

	virtual void		draw();
	virtual void		setVisible( BOOL visible );
	virtual void		updatePanel(BOOL allow_modify);

	static void			onSliderMouseDown(LLUICtrl* ctrl, void* userdata);
	static void			onSliderMoved(LLUICtrl* ctrl, void* userdata);
	static void			onSliderMouseUp(LLUICtrl* ctrl, void* userdata);

	static void			onHintMinMouseDown(void* userdata);
	static void			onHintMinHeldDown(void* userdata);
	static void			onHintMaxMouseDown(void* userdata);
	static void			onHintMaxHeldDown(void* userdata);
	static void			onHintMinMouseUp(void* userdata);
	static void			onHintMaxMouseUp(void* userdata);

	void				onHintMouseDown( LLVisualParamHint* hint );
	void				onHintHeldDown( LLVisualParamHint* hint );

	F32					weightToPercent( F32 weight );
	F32					percentToWeight( F32 percent );

public:
	LLViewerVisualParam* mParam;
	LLVisualParamHint*	mHintMin;
	LLVisualParamHint*	mHintMax;
	static S32 			sUpdateDelayFrames;
	
protected:
	LLTimer				mMouseDownTimer;	// timer for how long mouse has been held down on a hint.
	F32					mLastHeldTime;

	BOOL mAllowModify;
};

//static
S32 LLScrollingPanelParam::sUpdateDelayFrames = 0;

const S32 BTN_BORDER = 2;
const S32 PARAM_HINT_WIDTH = 128;
const S32 PARAM_HINT_HEIGHT = 128;
const S32 PARAM_HINT_LABEL_HEIGHT = 16;
const S32 PARAM_PANEL_WIDTH = 2 * (3* BTN_BORDER + PARAM_HINT_WIDTH +  LLPANEL_BORDER_WIDTH);
const S32 PARAM_PANEL_HEIGHT = 2 * BTN_BORDER + PARAM_HINT_HEIGHT + PARAM_HINT_LABEL_HEIGHT + 4 * LLPANEL_BORDER_WIDTH; 

LLScrollingPanelParam::LLScrollingPanelParam( const std::string& name,
											  LLViewerJointMesh* mesh, LLViewerVisualParam* param, BOOL allow_modify )
	: LLScrollingPanel( name, LLRect( 0, PARAM_PANEL_HEIGHT, PARAM_PANEL_WIDTH, 0 ) ),
	  mParam(param),
	  mAllowModify(allow_modify)
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_scrolling_param.xml");

	S32 pos_x = 2 * LLPANEL_BORDER_WIDTH;
	S32 pos_y = 3 * LLPANEL_BORDER_WIDTH + SLIDERCTRL_HEIGHT;
	F32 min_weight = param->getMinWeight();
	F32 max_weight = param->getMaxWeight();

	mHintMin = new LLVisualParamHint( pos_x, pos_y, PARAM_HINT_WIDTH, PARAM_HINT_HEIGHT, mesh, param,  min_weight);
	pos_x += PARAM_HINT_WIDTH + 3 * BTN_BORDER;
	mHintMax = new LLVisualParamHint( pos_x, pos_y, PARAM_HINT_WIDTH, PARAM_HINT_HEIGHT, mesh, param, max_weight );
	
	mHintMin->setAllowsUpdates( FALSE );
	mHintMax->setAllowsUpdates( FALSE );
	childSetValue("param slider", weightToPercent(param->getWeight()));
	childSetLabelArg("param slider", "[DESC]", param->getDisplayName());
	childSetEnabled("param slider", mAllowModify);
	childSetCommitCallback("param slider", LLScrollingPanelParam::onSliderMoved, this);

	// *TODO::translate
	std::string min_name = param->getMinDisplayName();
	std::string max_name = param->getMaxDisplayName();
	childSetValue("min param text", min_name);
	childSetValue("max param text", max_name);

	LLButton* less = getChild<LLButton>("less");
	if (less)
	{
		less->setMouseDownCallback( LLScrollingPanelParam::onHintMinMouseDown );
		less->setMouseUpCallback( LLScrollingPanelParam::onHintMinMouseUp );
		less->setHeldDownCallback( LLScrollingPanelParam::onHintMinHeldDown );
		less->setHeldDownDelay( PARAM_STEP_TIME_THRESHOLD );
	}

	LLButton* more = getChild<LLButton>("more");
	if (more)
	{
		more->setMouseDownCallback( LLScrollingPanelParam::onHintMaxMouseDown );
		more->setMouseUpCallback( LLScrollingPanelParam::onHintMaxMouseUp );
		more->setHeldDownCallback( LLScrollingPanelParam::onHintMaxHeldDown );
		more->setHeldDownDelay( PARAM_STEP_TIME_THRESHOLD );
	}

	setVisible(FALSE);
	setBorderVisible( FALSE );
}

LLScrollingPanelParam::~LLScrollingPanelParam()
{
	delete mHintMin;
	delete mHintMax;
}

void LLScrollingPanelParam::updatePanel(BOOL allow_modify)
{
	LLViewerVisualParam* param = mHintMin->getVisualParam();
	childSetValue("param slider", weightToPercent( param->getWeight() ) );
	mHintMin->requestUpdate( sUpdateDelayFrames++ );
	mHintMax->requestUpdate( sUpdateDelayFrames++ );

	mAllowModify = allow_modify;
	childSetEnabled("param slider", mAllowModify);
	childSetEnabled("less", mAllowModify);
	childSetEnabled("more", mAllowModify);
}

void LLScrollingPanelParam::setVisible( BOOL visible )
{
	if( getVisible() != visible )
	{
		LLPanel::setVisible( visible );
		mHintMin->setAllowsUpdates( visible );
		mHintMax->setAllowsUpdates( visible );

		if( visible )
		{
			mHintMin->setUpdateDelayFrames( sUpdateDelayFrames++ );
			mHintMax->setUpdateDelayFrames( sUpdateDelayFrames++ );
		}
	}
}

void LLScrollingPanelParam::draw()
{
	if( gFloaterCustomize->isMinimized() )
	{
		return;
	}
	
	childSetVisible("less", mHintMin->getVisible());
	childSetVisible("more", mHintMax->getVisible());

	// Draw all the children except for the labels
	childSetVisible( "min param text", FALSE );
	childSetVisible( "max param text", FALSE );
	LLPanel::draw();

	// Draw the hints over the "less" and "more" buttons.
	glPushMatrix();
	{
		const LLRect& r = mHintMin->getRect();
		F32 left = (F32)(r.mLeft + BTN_BORDER);
		F32 bot  = (F32)(r.mBottom + BTN_BORDER);
		glTranslatef(left, bot, 0.f);
		mHintMin->draw();
	}
	glPopMatrix();

	glPushMatrix();
	{
		const LLRect& r = mHintMax->getRect();
		F32 left = (F32)(r.mLeft + BTN_BORDER);
		F32 bot  = (F32)(r.mBottom + BTN_BORDER);
		glTranslatef(left, bot, 0.f);
		mHintMax->draw();
	}
	glPopMatrix();


	// Draw labels on top of the buttons
	childSetVisible( "min param text", TRUE );
	drawChild(getChild<LLView>("min param text"), BTN_BORDER, BTN_BORDER);

	childSetVisible( "max param text", TRUE );
	drawChild(getChild<LLView>("max param text"), BTN_BORDER, BTN_BORDER);
}

// static
void LLScrollingPanelParam::onSliderMoved(LLUICtrl* ctrl, void* userdata)
{
	LLSliderCtrl* slider = (LLSliderCtrl*) ctrl;
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	LLViewerVisualParam* param = self->mParam;

	F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( param );
	F32 new_weight = self->percentToWeight( (F32)slider->getValue().asReal() );
	if (current_weight != new_weight )
	{
		LLFloaterCustomize* floater_customize = gFloaterCustomize;
		if (!floater_customize) return;

		//KOWs avatar height stuff
		LLVOAvatar* avatar = gAgent.getAvatarObject();
		F32 avatar_size = (avatar->mBodySize.mV[VZ]) + (F32)0.17; //mBodySize is actually quite a bit off.
		avatar_size += (F32)99; //mBodySize is actually quite a bit off.
		
		floater_customize->getChild<LLTextBox>("HeightText")->setValue(llformat("%.2f", avatar_size) + "m");
		floater_customize->getChild<LLTextBox>("HeightText2")->setValue(llformat("%.2f",llround(avatar_size / 0.3048)) + "'"
																	  + llformat("%.2f",llround(avatar_size * 39.37) % 12) + "\"");

		gAgent.getAvatarObject()->setVisualParamWeight( param, new_weight, FALSE);
		gAgent.getAvatarObject()->updateVisualParams();
	}
}

// static
void LLScrollingPanelParam::onSliderMouseDown(LLUICtrl* ctrl, void* userdata)
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	LLViewerVisualParam* param = self->mParam;

	// store existing values in undo buffer
	F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( param );

	if( gFloaterCustomize )
	{
		LLPanelEditWearable* panel = gFloaterCustomize->getCurrentWearablePanel();
		if( panel )
		{
			panel->addVisualParamToUndoBuffer( param->getID(), current_weight ); 
		}
	}
}

// static
void LLScrollingPanelParam::onSliderMouseUp(LLUICtrl* ctrl, void* userdata)
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;

	// store namevalue
	gAgent.sendAgentSetAppearance();

	LLVisualParamHint::requestHintUpdates( self->mHintMin, self->mHintMax );
}

// static
void LLScrollingPanelParam::onHintMinMouseDown( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	self->onHintMouseDown( self->mHintMin );
}

// static
void LLScrollingPanelParam::onHintMaxMouseDown( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	self->onHintMouseDown( self->mHintMax );
}


void LLScrollingPanelParam::onHintMouseDown( LLVisualParamHint* hint )
{
	// morph towards this result
	F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( hint->getVisualParam() );

	// if we have maxed out on this morph, we shouldn't be able to click it
	if( hint->getVisualParamWeight() != current_weight )
	{
		// store existing values in undo buffer
		if( gFloaterCustomize )
		{
			LLPanelEditWearable* panel = gFloaterCustomize->getCurrentWearablePanel();
			if( panel )
			{
				panel->addVisualParamToUndoBuffer( hint->getVisualParam()->getID(), current_weight ); 
			}
		}

		mMouseDownTimer.reset();
		mLastHeldTime = 0.f;
	}
}

// static
void LLScrollingPanelParam::onHintMinHeldDown( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	self->onHintHeldDown( self->mHintMin );
}

// static
void LLScrollingPanelParam::onHintMaxHeldDown( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;
	self->onHintHeldDown( self->mHintMax );
}
	
void LLScrollingPanelParam::onHintHeldDown( LLVisualParamHint* hint )
{
	F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( hint->getVisualParam() );

	if (current_weight != hint->getVisualParamWeight() )
	{
		const F32 FULL_BLEND_TIME = 2.f;
		F32 elapsed_time = mMouseDownTimer.getElapsedTimeF32() - mLastHeldTime;
		mLastHeldTime += elapsed_time;

		F32 new_weight;
		if (current_weight > hint->getVisualParamWeight() )
		{
			new_weight = current_weight - (elapsed_time / FULL_BLEND_TIME);
		}
		else
		{
			new_weight = current_weight + (elapsed_time / FULL_BLEND_TIME);
		}

		// Make sure we're not taking the slider out of bounds
		// (this is where some simple UI limits are stored)
		F32 new_percent = weightToPercent(new_weight);
		LLSliderCtrl* slider = getChild<LLSliderCtrl>("param slider");
		if (slider)
		{
			if (slider->getMinValue() < new_percent
				&& new_percent < slider->getMaxValue())
			{
				gAgent.getAvatarObject()->setVisualParamWeight( hint->getVisualParam(), new_weight, TRUE);
				gAgent.getAvatarObject()->updateVisualParams();

				slider->setValue( weightToPercent( new_weight ) );
			}
		}
	}
}

// static
void LLScrollingPanelParam::onHintMinMouseUp( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;

	F32 elapsed_time = self->mMouseDownTimer.getElapsedTimeF32();

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (avatar)
	{
		LLVisualParamHint* hint = self->mHintMin;

		if (elapsed_time < PARAM_STEP_TIME_THRESHOLD)
		{
			// step in direction
			F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( hint->getVisualParam() );
			F32 range = self->mHintMax->getVisualParamWeight() - self->mHintMin->getVisualParamWeight();
			// step a fraction in the negative directiona
			F32 new_weight = current_weight - (range / 10.f);
			F32 new_percent = self->weightToPercent(new_weight);
			LLSliderCtrl* slider = self->getChild<LLSliderCtrl>("param slider");
			if (slider)
			{
				if (slider->getMinValue() < new_percent
					&& new_percent < slider->getMaxValue())
				{
					avatar->setVisualParamWeight(hint->getVisualParam(), new_weight, TRUE);
					slider->setValue( self->weightToPercent( new_weight ) );
				}
			}
		}

		// store namevalue
		gAgent.sendAgentSetAppearance();
	}

	LLVisualParamHint::requestHintUpdates( self->mHintMin, self->mHintMax );
}

void LLScrollingPanelParam::onHintMaxMouseUp( void* userdata )
{
	LLScrollingPanelParam* self = (LLScrollingPanelParam*) userdata;

	F32 elapsed_time = self->mMouseDownTimer.getElapsedTimeF32();

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if (avatar)
	{
		LLVisualParamHint* hint = self->mHintMax;

		if (elapsed_time < PARAM_STEP_TIME_THRESHOLD)
		{
			// step in direction
			F32 current_weight = gAgent.getAvatarObject()->getVisualParamWeight( hint->getVisualParam() );
			F32 range = self->mHintMax->getVisualParamWeight() - self->mHintMin->getVisualParamWeight();
			// step a fraction in the negative direction
			F32 new_weight = current_weight + (range / 10.f);
			F32 new_percent = self->weightToPercent(new_weight);
			LLSliderCtrl* slider = self->getChild<LLSliderCtrl>("param slider");
			if (slider)
			{
				if (slider->getMinValue() < new_percent
					&& new_percent < slider->getMaxValue())
				{
					avatar->setVisualParamWeight(hint->getVisualParam(), new_weight, TRUE);
					slider->setValue( self->weightToPercent( new_weight ) );
				}
			}
		}

		// store namevalue
		gAgent.sendAgentSetAppearance();
	}

	LLVisualParamHint::requestHintUpdates( self->mHintMin, self->mHintMax );
}


F32 LLScrollingPanelParam::weightToPercent( F32 weight )
{
	LLViewerVisualParam* param = mParam;
	return (weight - param->getMinWeight()) /  (param->getMaxWeight() - param->getMinWeight()) * 100.f;
}

F32 LLScrollingPanelParam::percentToWeight( F32 percent )
{
	LLViewerVisualParam* param = mParam;
	return percent / 100.f * (param->getMaxWeight() - param->getMinWeight()) + param->getMinWeight();
}

const std::string& LLFloaterCustomize::getEditGroup()
{
	return getCurrentWearablePanel()->getCurrentSubpart()->mEditGroup;
}


/////////////////////////////////////////////////////////////////////
// LLFloaterCustomize

// statics
EWearableType	LLFloaterCustomize::sCurrentWearableType = WT_SHAPE;

struct WearablePanelData
{
	WearablePanelData(LLFloaterCustomize* floater, EWearableType type)
		: mFloater(floater), mType(type) {}
	LLFloaterCustomize* mFloater;
	EWearableType mType;
};

LLFloaterCustomize::LLFloaterCustomize()
:	LLFloater(std::string("customize")),
	mScrollingPanelList( NULL ),
	mGenePool( NULL ),
	mInventoryObserver(NULL),
	mNextStepAfterSaveAllCallback( NULL ),
	mNextStepAfterSaveAllUserdata( NULL )
{
	gSavedSettings.setU32("AvatarSex", (gAgent.getAvatarObject()->getSex() == SEX_MALE) );

	mResetParams = new LLVisualParamReset();
	
	// create the observer which will watch for matching incoming inventory
	mInventoryObserver = new LLFloaterCustomizeObserver(this);
	gInventory.addObserver(mInventoryObserver);

	LLCallbackMap::map_t factory_map;
	factory_map["Shape"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SHAPE) ) );
	factory_map["Skin"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SKIN) ) );
	factory_map["Hair"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_HAIR) ) );
	factory_map["Eyes"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_EYES) ) );
	factory_map["Shirt"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SHIRT) ) );
	factory_map["Pants"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_PANTS) ) );
	factory_map["Shoes"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SHOES) ) );
	factory_map["Socks"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SOCKS) ) );
	factory_map["Jacket"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_JACKET) ) );
	factory_map["Gloves"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_GLOVES) ) );
	factory_map["Undershirt"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_UNDERSHIRT) ) );
	factory_map["Underpants"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_UNDERPANTS) ) );
	factory_map["Skirt"] = LLCallbackMap(createWearablePanel, (void*)(new WearablePanelData(this, WT_SKIRT) ) );
	
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_customize.xml", &factory_map);
	
}

BOOL LLFloaterCustomize::postBuild()
{
	childSetAction("Make Outfit", LLFloaterCustomize::onBtnMakeOutfit, (void*)this);
	childSetAction("Save All", LLFloaterCustomize::onBtnSaveAll, (void*)this);
	childSetAction("Close", LLFloater::onClickClose, (void*)this);

	// Wearable panels
	initWearablePanels();

	// Tab container
	childSetTabChangeCallback("customize tab container", "Shape", onTabChanged, (void*)WT_SHAPE );
	childSetTabChangeCallback("customize tab container", "Skin", onTabChanged, (void*)WT_SKIN );
	childSetTabChangeCallback("customize tab container", "Hair", onTabChanged, (void*)WT_HAIR );
	childSetTabChangeCallback("customize tab container", "Eyes", onTabChanged, (void*)WT_EYES );
	childSetTabChangeCallback("customize tab container", "Shirt", onTabChanged, (void*)WT_SHIRT );
	childSetTabChangeCallback("customize tab container", "Pants", onTabChanged, (void*)WT_PANTS );
	childSetTabChangeCallback("customize tab container", "Shoes", onTabChanged, (void*)WT_SHOES );
	childSetTabChangeCallback("customize tab container", "Socks", onTabChanged, (void*)WT_SOCKS );
	childSetTabChangeCallback("customize tab container", "Jacket", onTabChanged, (void*)WT_JACKET );
	childSetTabChangeCallback("customize tab container", "Gloves", onTabChanged, (void*)WT_GLOVES );
	childSetTabChangeCallback("customize tab container", "Undershirt", onTabChanged, (void*)WT_UNDERSHIRT );
	childSetTabChangeCallback("customize tab container", "Underpants", onTabChanged, (void*)WT_UNDERPANTS );
	childSetTabChangeCallback("customize tab container", "Skirt", onTabChanged, (void*)WT_SKIRT );

	// Remove underwear panels for teens
	if (gAgent.isTeen())
	{
		LLTabContainer* tab_container = getChild<LLTabContainer>("customize tab container");
		if (tab_container)
		{
			LLPanel* panel;
			panel = tab_container->getPanelByName("Undershirt");
			if (panel) tab_container->removeTabPanel(panel);
			panel = tab_container->getPanelByName("Underpants");
			if (panel) tab_container->removeTabPanel(panel);
		}
	}
	
	// Scrolling Panel
	initScrollingPanelList();

	childShowTab("customize tab container", "Shape", true);
	
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////

// static
void LLFloaterCustomize::setCurrentWearableType( EWearableType type )
{
	if( LLFloaterCustomize::sCurrentWearableType != type )
	{
		LLFloaterCustomize::sCurrentWearableType = type; 

		S32 type_int = (S32)type;
		if( gFloaterCustomize
			&& gFloaterCustomize->mWearablePanelList[type_int])
		{
			std::string panelname = gFloaterCustomize->mWearablePanelList[type_int]->getName();
			gFloaterCustomize->childShowTab("customize tab container", panelname);
			gFloaterCustomize->switchToDefaultSubpart();
		}
	}
}

// static
void LLFloaterCustomize::onBtnSaveAll( void* userdata )
{
	gAgent.saveAllWearables();
}


// static
void LLFloaterCustomize::onBtnSnapshot( void* userdata )
{
	// Trigger noise, but not animation
	send_sound_trigger(LLUUID(gSavedSettings.getString("UISndSnapshot")), 1.0f);

	LLPointer<LLImageRaw> raw = new LLImageRaw;
	BOOL success = gViewerWindow->rawSnapshot(raw,
											  gViewerWindow->getWindowWidth(),
											  gViewerWindow->getWindowHeight(),
											  TRUE,	// keep window aspect ratio
											  FALSE,
											  FALSE,	// UI in snapshot off
											  FALSE);	// do_rebuild off
	if (!success) return;

	LLPointer<LLImageJPEG> jpeg_image = new LLImageJPEG;
	success = jpeg_image->encode(raw, 0.0f);
	if(!success) return;

	std::string filepath("C:\\snapshot");
	filepath += ".jpg";

	success = jpeg_image->save(filepath);
}

// static
void LLFloaterCustomize::onBtnMakeOutfit( void* userdata )
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if(avatar)
	{
		LLMakeOutfitDialog* dialog = new LLMakeOutfitDialog( onMakeOutfitCommit, NULL );
		// LLMakeOutfitDialog deletes itself.

		for( S32 i = 0; i < WT_COUNT; i++ )
		{
			BOOL enabled = (gAgent.getWearable( (EWearableType) i ) != NULL);
			BOOL selected = (enabled && (WT_SHIRT <= i) && (i < WT_COUNT)); // only select clothing by default
			if (gAgent.isTeen()
				&& !edit_wearable_for_teens((EWearableType)i))
			{
				dialog->setWearableToInclude( i, FALSE, FALSE );
			}
			else
			{
				dialog->setWearableToInclude( i, enabled, selected );
			}
		}
		dialog->startModal();
	}
}

// static
void LLFloaterCustomize::onMakeOutfitCommit( LLMakeOutfitDialog* dialog, void* userdata )
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if(avatar)
	{
		LLDynamicArray<S32> wearables_to_include;
		LLDynamicArray<S32> attachments_to_include;  // attachment points

		dialog->getIncludedItems( wearables_to_include, attachments_to_include );

		gAgent.makeNewOutfit( dialog->getFolderName(), wearables_to_include, attachments_to_include, dialog->getRenameClothing() );
	}
}

////////////////////////////////////////////////////////////////////////////

// static
void* LLFloaterCustomize::createWearablePanel(void* userdata)
{
	WearablePanelData* data = (WearablePanelData*)userdata;
	EWearableType type = data->mType;
	LLPanelEditWearable* panel;
	if ((gAgent.isTeen() && !edit_wearable_for_teens(data->mType) ))
	{
		panel = NULL;
	}
	else
	{
		panel = new LLPanelEditWearable( type );
	}
	data->mFloater->mWearablePanelList[type] = panel;
	delete data;
	return panel;
}

void LLFloaterCustomize::initWearablePanels()
{
	LLSubpart* part;
	
	/////////////////////////////////////////
	// Shape
	LLPanelEditWearable* panel = mWearablePanelList[ WT_SHAPE ];

	// body
	part = new LLSubpart();
	part->mTargetJoint = "mPelvis";
	part->mEditGroup = "shape_body";
	part->mTargetOffset.setVec(0.f, 0.f, 0.1f);
	part->mCameraOffset.setVec(-2.5f, 0.5f, 0.8f);
	panel->addSubpart( "Body", SUBPART_SHAPE_WHOLE, part );

	// head supparts
	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_head";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Head", SUBPART_SHAPE_HEAD, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_eyes";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Eyes", SUBPART_SHAPE_EYES, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_ears";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Ears", SUBPART_SHAPE_EARS, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_nose";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Nose", SUBPART_SHAPE_NOSE, part );


	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_mouth";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Mouth", SUBPART_SHAPE_MOUTH, part );


	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "shape_chin";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f );
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f );
	panel->addSubpart( "Chin", SUBPART_SHAPE_CHIN, part );

	// torso
	part = new LLSubpart();
	part->mTargetJoint = "mTorso";
	part->mEditGroup = "shape_torso";
	part->mTargetOffset.setVec(0.f, 0.f, 0.3f);
	part->mCameraOffset.setVec(-1.f, 0.15f, 0.3f);
	panel->addSubpart( "Torso", SUBPART_SHAPE_TORSO, part );

	// legs
	part = new LLSubpart();
	part->mTargetJoint = "mPelvis";
	part->mEditGroup = "shape_legs";
	part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
	part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
	panel->addSubpart( "Legs", SUBPART_SHAPE_LEGS, part );

	panel->childSetCommitCallback("sex radio", LLPanelEditWearable::onCommitSexChange, panel);
	panel->childSetAction("Randomize", &LLPanelEditWearable::onBtnRandomize, panel);

	/////////////////////////////////////////
	// Skin
	panel = mWearablePanelList[ WT_SKIN ];

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "skin_color";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( "Skin Color", SUBPART_SKIN_COLOR, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "skin_facedetail";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( "Face Detail", SUBPART_SKIN_FACEDETAIL, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "skin_makeup";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( "Makeup", SUBPART_SKIN_MAKEUP, part );

	part = new LLSubpart();
	part->mTargetJoint = "mPelvis";
	part->mEditGroup = "skin_bodydetail";
	part->mTargetOffset.setVec(0.f, 0.f, -0.2f);
	part->mCameraOffset.setVec(-2.5f, 0.5f, 0.5f);
	panel->addSubpart( "Body Detail", SUBPART_SKIN_BODYDETAIL, part );

	panel->addTextureDropTarget( LLVOAvatar::TEX_HEAD_BODYPAINT,  "Head Tattoos", 	LLUUID::null, TRUE );
	panel->addTextureDropTarget( LLVOAvatar::TEX_UPPER_BODYPAINT, "Upper Tattoos", 	LLUUID::null, TRUE );
	panel->addTextureDropTarget( LLVOAvatar::TEX_LOWER_BODYPAINT, "Lower Tattoos", 	LLUUID::null, TRUE );

	panel->childSetAction("Randomize", &LLPanelEditWearable::onBtnRandomize, panel);

	/////////////////////////////////////////
	// Hair
	panel = mWearablePanelList[ WT_HAIR ];

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "hair_color";
	part->mTargetOffset.setVec(0.f, 0.f, 0.10f);
	part->mCameraOffset.setVec(-0.4f, 0.05f, 0.10f);
	panel->addSubpart( "Color", SUBPART_HAIR_COLOR, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "hair_style";
	part->mTargetOffset.setVec(0.f, 0.f, 0.10f);
	part->mCameraOffset.setVec(-0.4f, 0.05f, 0.10f);
	panel->addSubpart( "Style", SUBPART_HAIR_STYLE, part );

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "hair_eyebrows";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( "Eyebrows", SUBPART_HAIR_EYEBROWS, part );

	part = new LLSubpart();
	part->mSex = SEX_MALE;
	part->mTargetJoint = "mHead";
	part->mEditGroup = "hair_facial";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( "Facial", SUBPART_HAIR_FACIAL, part );

	panel->addTextureDropTarget(LLVOAvatar::TEX_HAIR, "Texture",
								LLUUID( gSavedSettings.getString( "UIImgDefaultHairUUID" ) ),
								FALSE );

	panel->childSetAction("Randomize", &LLPanelEditWearable::onBtnRandomize, panel);

	/////////////////////////////////////////
	// Eyes
	panel = mWearablePanelList[ WT_EYES ];

	part = new LLSubpart();
	part->mTargetJoint = "mHead";
	part->mEditGroup = "eyes";
	part->mTargetOffset.setVec(0.f, 0.f, 0.05f);
	part->mCameraOffset.setVec(-0.5f, 0.05f, 0.07f);
	panel->addSubpart( LLStringUtil::null, SUBPART_EYES, part );

	panel->addTextureDropTarget(LLVOAvatar::TEX_EYES_IRIS, "Iris",
								LLUUID( gSavedSettings.getString( "UIImgDefaultEyesUUID" ) ),
								FALSE );

	panel->childSetAction("Randomize", &LLPanelEditWearable::onBtnRandomize, panel);

	/////////////////////////////////////////
	// Shirt
	panel = mWearablePanelList[ WT_SHIRT ];

	part = new LLSubpart();
	part->mTargetJoint = "mTorso";
	part->mEditGroup = "shirt";
	part->mTargetOffset.setVec(0.f, 0.f, 0.3f);
	part->mCameraOffset.setVec(-1.f, 0.15f, 0.3f);
	panel->addSubpart( LLStringUtil::null, SUBPART_SHIRT, part );

	panel->addTextureDropTarget( LLVOAvatar::TEX_UPPER_SHIRT, "Fabric",
								 LLUUID( gSavedSettings.getString( "UIImgDefaultShirtUUID" ) ),
								 FALSE );

	panel->addColorSwatch( LLVOAvatar::TEX_UPPER_SHIRT, "Color/Tint" );


	/////////////////////////////////////////
	// Pants
	panel = mWearablePanelList[ WT_PANTS ];

	part = new LLSubpart();
	part->mTargetJoint = "mPelvis";
	part->mEditGroup = "pants";
	part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
	part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
	panel->addSubpart( LLStringUtil::null, SUBPART_PANTS, part );

	panel->addTextureDropTarget(LLVOAvatar::TEX_LOWER_PANTS, "Fabric",
								LLUUID( gSavedSettings.getString( "UIImgDefaultPantsUUID" ) ),
								FALSE );

	panel->addColorSwatch( LLVOAvatar::TEX_LOWER_PANTS, "Color/Tint" );


	/////////////////////////////////////////
	// Shoes
	panel = mWearablePanelList[ WT_SHOES ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "shoes";
		part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
		part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
		panel->addSubpart( LLStringUtil::null, SUBPART_SHOES, part );

		panel->addTextureDropTarget( LLVOAvatar::TEX_LOWER_SHOES, "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultShoesUUID" ) ),
									 FALSE );

		panel->addColorSwatch( LLVOAvatar::TEX_LOWER_SHOES, "Color/Tint" );
	}


	/////////////////////////////////////////
	// Socks
	panel = mWearablePanelList[ WT_SOCKS ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "socks";
		part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
		part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
		panel->addSubpart( LLStringUtil::null, SUBPART_SOCKS, part );

		panel->addTextureDropTarget( LLVOAvatar::TEX_LOWER_SOCKS, "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultSocksUUID" ) ),
									 FALSE );

		panel->addColorSwatch( LLVOAvatar::TEX_LOWER_SOCKS, "Color/Tint" );
	}

	/////////////////////////////////////////
	// Jacket
	panel = mWearablePanelList[ WT_JACKET ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "jacket";
		part->mTargetOffset.setVec(0.f, 0.f, 0.f);
		part->mCameraOffset.setVec(-2.f, 0.1f, 0.3f);
		panel->addSubpart( LLStringUtil::null, SUBPART_JACKET, part );

		panel->addTextureDropTarget( LLVOAvatar::TEX_UPPER_JACKET, "Upper Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultJacketUUID" ) ),
									 FALSE );
		panel->addTextureDropTarget( LLVOAvatar::TEX_LOWER_JACKET, "Lower Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultJacketUUID" ) ),
									 FALSE );

		panel->addColorSwatch( LLVOAvatar::TEX_UPPER_JACKET, "Color/Tint" );
	}

	/////////////////////////////////////////
	// Skirt
	panel = mWearablePanelList[ WT_SKIRT ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "skirt";
		part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
		part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
		panel->addSubpart( LLStringUtil::null, SUBPART_SKIRT, part );

		panel->addTextureDropTarget( LLVOAvatar::TEX_SKIRT,  "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultSkirtUUID" ) ),
									 FALSE );

		panel->addColorSwatch( LLVOAvatar::TEX_SKIRT, "Color/Tint" );
	}


	/////////////////////////////////////////
	// Gloves
	panel = mWearablePanelList[ WT_GLOVES ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "gloves";
		part->mTargetOffset.setVec(0.f, 0.f, 0.f);
		part->mCameraOffset.setVec(-1.f, 0.15f, 0.f);
		panel->addSubpart( LLStringUtil::null, SUBPART_GLOVES, part );

		panel->addTextureDropTarget( LLVOAvatar::TEX_UPPER_GLOVES,  "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultGlovesUUID" ) ),
									 FALSE );

		panel->addColorSwatch( LLVOAvatar::TEX_UPPER_GLOVES, "Color/Tint" );
	}


	/////////////////////////////////////////
	// Undershirt
	panel = mWearablePanelList[ WT_UNDERSHIRT ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mTorso";
		part->mEditGroup = "undershirt";
		part->mTargetOffset.setVec(0.f, 0.f, 0.3f);
		part->mCameraOffset.setVec(-1.f, 0.15f, 0.3f);
		panel->addSubpart( LLStringUtil::null, SUBPART_UNDERSHIRT, part );

		panel->addTextureDropTarget( LLVOAvatar::TEX_UPPER_UNDERSHIRT,  "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultUnderwearUUID" ) ),
									 FALSE );

		panel->addColorSwatch( LLVOAvatar::TEX_UPPER_UNDERSHIRT, "Color/Tint" );
	}

	/////////////////////////////////////////
	// Underpants
	panel = mWearablePanelList[ WT_UNDERPANTS ];

	if (panel)
	{
		part = new LLSubpart();
		part->mTargetJoint = "mPelvis";
		part->mEditGroup = "underpants";
		part->mTargetOffset.setVec(0.f, 0.f, -0.5f);
		part->mCameraOffset.setVec(-1.6f, 0.15f, -0.5f);
		panel->addSubpart( LLStringUtil::null, SUBPART_UNDERPANTS, part );

		panel->addTextureDropTarget( LLVOAvatar::TEX_LOWER_UNDERPANTS, "Fabric",
									 LLUUID( gSavedSettings.getString( "UIImgDefaultUnderwearUUID" ) ),
									 FALSE );

		panel->addColorSwatch( LLVOAvatar::TEX_LOWER_UNDERPANTS, "Color/Tint" );
	}
}

////////////////////////////////////////////////////////////////////////////

LLFloaterCustomize::~LLFloaterCustomize()
{
	llinfos << "Destroying LLFloaterCustomize" << llendl;
	delete mGenePool;
	delete mResetParams;
	gInventory.removeObserver(mInventoryObserver);
	delete mInventoryObserver;
}

void LLFloaterCustomize::switchToDefaultSubpart()
{
	getCurrentWearablePanel()->switchToDefaultSubpart();
}

void LLFloaterCustomize::spawnWearableAppearance(EWearableType type)
{
	if( !mGenePool )
	{
		mGenePool = new LLGenePool();
	}

	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( avatar )
	{
		mGenePool->spawn( type );
	}
}


void LLFloaterCustomize::draw()
{
	if( isMinimized() )
	{
		LLFloater::draw();
		return;
	}

	// only do this if we are in the customize avatar mode
	// and not transitioning into or out of it
	// *TODO: This is a sort of expensive call, which only needs
	// to be called when the tabs change or an inventory item
	// arrives. Figure out some way to avoid this if possible.
	updateInventoryUI();

	LLFloaterCustomize* floater_customize = gFloaterCustomize;
	if (!floater_customize) return;

	//KOWs avatar height stuff
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	F32 avatar_size = (avatar->mBodySize.mV[VZ]) + (F32)0.17; //mBodySize is actually quite a bit off.
		
	floater_customize->getChild<LLTextBox>("HeightText")->setValue(llformat("%.2f", avatar_size) + "m");
	//inches = avatar_size * 39.37
	//round(inches) + inches % 12
	std::string temp = llformat("%.0f",(F32)llfloor(avatar_size / 0.3048));
	std::string temp2 = llformat("%.0f",(F32)(llround(avatar_size * 39.37) % 12));
	floater_customize->getChild<LLTextBox>("HeightText2")->setValue(temp + "'"
																  + temp2 + "\"");

	LLScrollingPanelParam::sUpdateDelayFrames = 0;
	
	childSetEnabled("Save All",  isDirty() );
	LLFloater::draw();
}

BOOL LLFloaterCustomize::isDirty() const
{
	for(S32 i = 0; i < WT_COUNT; i++)
	{
		if( mWearablePanelList[i]
			&& mWearablePanelList[i]->isDirty() )
		{
			return TRUE;
		}
	}
	return FALSE;
}


// static
void LLFloaterCustomize::onTabChanged( void* userdata, bool from_click )
{
	EWearableType wearable_type = (EWearableType) (intptr_t)userdata;
	LLFloaterCustomize::setCurrentWearableType( wearable_type );
}

void LLFloaterCustomize::onClose(bool app_quitting)
{
	// since this window is potentially staying open, push to back to let next window take focus
	gFloaterView->sendChildToBack(this);
	handle_reset_view();  // Calls askToSaveAllIfDirty
}


////////////////////////////////////////////////////////////////////////////

const S32 LOWER_BTN_HEIGHT = 18 + 8;

const S32 FLOATER_CUSTOMIZE_BUTTON_WIDTH = 82;
const S32 FLOATER_CUSTOMIZE_BOTTOM_PAD = 30;
const S32 LINE_HEIGHT = 16;
const S32 HEADER_PAD = 8;
const S32 HEADER_HEIGHT = 3 * (LINE_HEIGHT + LLFLOATER_VPAD) + (2 * LLPANEL_BORDER_WIDTH) + HEADER_PAD; 

void LLFloaterCustomize::initScrollingPanelList()
{
	LLScrollableContainerView* scroll_container =
		getChild<LLScrollableContainerView>("panel_container");
	// LLScrollingPanelList's do not import correctly 
// 	mScrollingPanelList = LLUICtrlFactory::getScrollingPanelList(this, "panel_list");
	mScrollingPanelList = new LLScrollingPanelList(std::string("panel_list"), LLRect());
	if (scroll_container)
	{
		scroll_container->setScrolledView(mScrollingPanelList);
		scroll_container->addChild(mScrollingPanelList);
	}
}

void LLFloaterCustomize::clearScrollingPanelList()
{
	if( mScrollingPanelList )
	{
		mScrollingPanelList->clearPanels();
	}
}

void LLFloaterCustomize::generateVisualParamHints(LLViewerJointMesh* joint_mesh, LLFloaterCustomize::param_map& params)
{
	// sorted_params is sorted according to magnitude of effect from
	// least to greatest.  Adding to the front of the child list
	// reverses that order.
	if( mScrollingPanelList )
	{
		mScrollingPanelList->clearPanels();
		param_map::iterator end = params.end();
		for(param_map::iterator it = params.begin(); it != end; ++it)
		{
			mScrollingPanelList->addPanel( new LLScrollingPanelParam( "LLScrollingPanelParam", joint_mesh, (*it).second.second, (*it).second.first) );
		}
	}
}

void LLFloaterCustomize::setWearable(EWearableType type, LLWearable* wearable, U32 perm_mask, BOOL is_complete)
{
	llassert( type < WT_COUNT );
	gSavedSettings.setU32("AvatarSex", (gAgent.getAvatarObject()->getSex() == SEX_MALE) );
	
	LLPanelEditWearable* panel = mWearablePanelList[ type ];
	if( panel )
	{
		panel->setWearable(wearable, perm_mask, is_complete);
		updateScrollingPanelList((perm_mask & PERM_MODIFY) ? is_complete : FALSE);
	}
}

void LLFloaterCustomize::updateScrollingPanelList(BOOL allow_modify)
{
	if( mScrollingPanelList )
	{
		LLScrollingPanelParam::sUpdateDelayFrames = 0;
		mScrollingPanelList->updatePanels(allow_modify );
	}
}


void LLFloaterCustomize::askToSaveAllIfDirty( void(*next_step_callback)(BOOL proceed, void* userdata), void* userdata )
{
	if( isDirty())
	{
		// Ask if user wants to save, then continue to next step afterwards
		mNextStepAfterSaveAllCallback = next_step_callback;
		mNextStepAfterSaveAllUserdata = userdata;

		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		gViewerWindow->alertXml("SaveClothingBodyChanges", 
			LLFloaterCustomize::onSaveAllDialog, this);
		return;
	}

	// Try to move to the next step
	if( next_step_callback )
	{
		next_step_callback( TRUE, userdata );
	}
}


// static
void LLFloaterCustomize::onSaveAllDialog( S32 option, void* userdata )
{
	LLFloaterCustomize* self = (LLFloaterCustomize*) userdata;

	BOOL proceed = FALSE;

	switch( option )
	{
	case 0:  // "Save All"
		gAgent.saveAllWearables();
		proceed = TRUE;
		break;

	case 1:  // "Don't Save"
		{

			EWearableType cur = getCurrentWearableType();
			gAgent.revertAllWearables();
			setCurrentWearableType( cur );
			proceed = TRUE;
		}
		break;

	case 2: // "Cancel"
		break;

	default:
		llassert(0);
		break;
	}

	if( self->mNextStepAfterSaveAllCallback )
	{
		self->mNextStepAfterSaveAllCallback( proceed, self->mNextStepAfterSaveAllUserdata );
	}
}

// fetch observer
class LLCurrentlyWorn : public LLInventoryFetchObserver
{
public:
	LLCurrentlyWorn() {}
	~LLCurrentlyWorn() {}
	virtual void done() { /* no operation necessary */}
};

void LLFloaterCustomize::fetchInventory()
{
	// Fetch currently worn items
	LLInventoryFetchObserver::item_ref_t ids;
	LLUUID item_id;
	for(S32 type = (S32)WT_SHAPE; type < (S32)WT_COUNT; ++type)
	{
		item_id = gAgent.getWearableItem((EWearableType)type);
		if(item_id.notNull())
		{
			ids.push_back(item_id);
		}
	}

	// Fire & forget. The mInventoryObserver will catch inventory
	// updates and correct the UI as necessary.
	LLCurrentlyWorn worn;
	worn.fetchItems(ids);
}

void LLFloaterCustomize::updateInventoryUI()
{
	BOOL all_complete = TRUE;
	BOOL is_complete = FALSE;
	U32 perm_mask = 0x0;
	LLPanelEditWearable* panel;
	LLViewerInventoryItem* item;
	for(S32 i = 0; i < WT_COUNT; ++i)
	{
		item = NULL;
		panel = mWearablePanelList[i];
		if(panel)
		{
			item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem(panel->getType());
		}
		if(item)
		{
			is_complete = item->isComplete();
			if(!is_complete)
			{
				all_complete = FALSE;
			}
			perm_mask = item->getPermissions().getMaskOwner();
		}
		else
		{
			is_complete = false;
			perm_mask = 0x0;
		}
		if(i == sCurrentWearableType)
		{
			if(panel)
			{
				panel->setUIPermissions(perm_mask, is_complete);
			}
			BOOL is_vis = panel && item && is_complete && (perm_mask & PERM_MODIFY);
			childSetVisible("panel_container", is_vis);
		}
	}
	childSetEnabled("Make Outfit", all_complete);
}

void LLFloaterCustomize::updateScrollingPanelUI()
{
	LLPanelEditWearable* panel = mWearablePanelList[sCurrentWearableType];
	if(panel)
	{
		LLViewerInventoryItem* item = (LLViewerInventoryItem*)gAgent.getWearableInventoryItem(panel->getType());
		if(item)
		{
			U32 perm_mask = item->getPermissions().getMaskOwner();
			BOOL is_complete = item->isComplete();
			updateScrollingPanelList((perm_mask & PERM_MODIFY) ? is_complete : FALSE);
		}
	}
}

/////////////////////////////////////////////////////////////////////
// LLUndoWearable

void LLUndoWearable::setVisualParam( S32 param_id, F32 weight)
{
	mAppearance.clear();
	mAppearance.addParam( param_id, weight );
}

void LLUndoWearable::setTexture( LLVOAvatar::ETextureIndex te, const LLUUID& asset_id )
{
	mAppearance.clear();
	mAppearance.addTexture( te, asset_id );
}

void LLUndoWearable::setColor( LLVOAvatar::ETextureIndex te, const LLColor4& color )
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( !avatar )
	{
		return;
	}

	const char* param_name[3];
	if( avatar->teToColorParams( te, param_name ) )
	{
		mAppearance.clear();
		LLVisualParam* param;
		param = avatar->getVisualParam( param_name[0] );
		if( param )
		{
			mAppearance.addParam( param->getID(), color.mV[VX] );
		}
		param = avatar->getVisualParam( param_name[1] );
		if( param )
		{
			mAppearance.addParam( param->getID(), color.mV[VY] );
		}
		param = avatar->getVisualParam( param_name[2] );
		if( param )
		{
			mAppearance.addParam( param->getID(), color.mV[VZ] );
		}
	}
}

void LLUndoWearable::setWearable( EWearableType type )
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( !avatar )
	{
		return;
	}

	mAppearance.clear();

	for( LLVisualParam* param = avatar->getFirstVisualParam(); param; param = avatar->getNextVisualParam() )
	{
		LLViewerVisualParam* viewer_param = (LLViewerVisualParam*)param;
		if( (viewer_param->getWearableType() == type) && 
			(viewer_param->getGroup() == VISUAL_PARAM_GROUP_TWEAKABLE) )
		{
			mAppearance.addParam( viewer_param->getID(), viewer_param->getWeight() );
		}
	}

	for( S32 te = 0; te < LLVOAvatar::TEX_NUM_ENTRIES; te++ )
	{
		if( LLVOAvatar::getTEWearableType( te ) == type )
		{
			LLViewerImage* te_image = avatar->getTEImage( te );
			if( te_image )
			{
				mAppearance.addTexture( te, te_image->getID() );
			}
		}
	}
}


void LLUndoWearable::applyUndoRedo()
{
	LLVOAvatar* avatar = gAgent.getAvatarObject();
	if( !avatar )
	{
		return;
	}

	ESex old_sex = avatar->getSex();

	// Parameters
	for (LLAppearance::param_map_t::iterator iter = mAppearance.mParamMap.begin();
		 iter != mAppearance.mParamMap.end(); ++iter)
	{
		S32 param_id = iter->first;
		F32 weight = iter->second;
		F32 existing_weight = gAgent.getAvatarObject()->getVisualParamWeight( param_id );
		avatar->setVisualParamWeight(param_id, weight, TRUE);
		iter->second = existing_weight;
	}

	// Textures
	for( S32 i = 0; i < LLVOAvatar::TEX_NUM_ENTRIES; i++ )
	{
		const LLUUID& image_id = mAppearance.mTextures[i];
		if( !image_id.isNull() )
		{
			LLViewerImage* existing_image = avatar->getTEImage( i );
			if( existing_image )
			{
				const LLUUID& existing_asset_id = existing_image->getID();
				avatar->setLocTexTE( i, gImageList.getImage( mAppearance.mTextures[i] ), TRUE );
				mAppearance.mTextures[i] = existing_asset_id;
			}
		}
	}

	avatar->updateVisualParams();
	
	ESex new_sex = avatar->getSex();
	if( old_sex != new_sex )
	{
		gSavedSettings.setU32( "AvatarSex", (new_sex == SEX_MALE) );
		avatar->updateSexDependentLayerSets( TRUE );
	}	
	
	LLVisualParamHint::requestHintUpdates(); 

	if( gFloaterCustomize )
	{
		gFloaterCustomize->updateScrollingPanelList(TRUE);
	}

	gAgent.sendAgentSetAppearance();
}
