
#include "llviewerprecompiledheaders.h"

// system library includes
#include <iostream>
#include <fstream>
#include <sstream>

// linden library includes
#include "llfilepicker.h"
#include "indra_constants.h"
#include "llsdserialize.h"
#include "llsdutil.h"

#include "llcallbacklist.h"

// newview includes
#include "llagent.h"
#include "llselectmgr.h"
#include "lltoolplacer.h"
#include "lltexturecache.h"
#include "llnotify.h"
#include "llapr.h"
#include "lldir.h"
#include "llimage.h"
#include "lllfsthread.h"
#include "llassetuploadresponders.h"
#include "lleconomy.h"
#include "llhttpclient.h"
#include "lluploaddialog.h"
#include "lldir.h"
#include "llinventorymodel.h"	// gInventory
#include "llviewercontrol.h"	// gSavedSettings
#include "llviewermenu.h"	// gMenuHolder
#include "llfilepicker.h"
#include "llfloateranimpreview.h"
#include "llfloaterbuycurrency.h"
#include "llfloaterimagepreview.h"
#include "llfloaternamedesc.h"
#include "llfloatersnapshot.h"
#include "llresourcedata.h"
#include "llstartup.h"			// gIsInSecondLife
#include "llstatusbar.h"
#include "llviewerimagelist.h"
#include "lluictrlfactory.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "lluploaddialog.h"
// Included to allow LLTextureCache::purgeTextures() to pause watchdog timeout
#include "llappviewer.h" 
#include "lltransactiontypes.h"
#include "llviewerobjectlist.h"

#include "primbackup.h" 

primbackup* primbackup::sInstance = 0;

// Note: these default textures are initialized with hard coded values to
// prevent cheating. When not in SL, the user-configurable values are used
// instead (see setDefaultTextures() below).
static LLUUID LL_TEXTURE_PLYWOOD		= LLUUID("89556747-24cb-43ed-920b-47caed15465f");
static LLUUID LL_TEXTURE_BLANK			= LLUUID("5748decc-f629-461c-9a36-a35a221fe21f");
static LLUUID LL_TEXTURE_INVISIBLE		= LLUUID("38b86f85-2575-52a9-a531-23108d8da837");
static LLUUID LL_TEXTURE_TRANSPARENT	= LLUUID("8dcd4a48-2d37-4909-9f78-f7a9eb4ef903");
static LLUUID LL_TEXTURE_MEDIA			= LLUUID("8b5fec65-8d8d-9dc5-cda8-8fdf2716e361");

void setDefaultTextures()
{
	if (!gIsInSecondLife)
	{
		// When not in SL (no texture perm check needed), we can get these
		// defaults from the user settings...
		LL_TEXTURE_PLYWOOD = LLUUID(gSavedSettings.getString("DefaultObjectTexture"));
		LL_TEXTURE_BLANK = LLUUID(gSavedSettings.getString("UIImgWhiteUUID"));
		if (gSavedSettings.controlExists("UIImgInvisibleUUID"))
		{
			// This control only exists in the Cool VL Viewer (added by the
			// AllowInvisibleTextureInPicker patch)
			LL_TEXTURE_INVISIBLE = LLUUID(gSavedSettings.getString("UIImgInvisibleUUID"));
		}
	}
}

class importResponder: public LLNewAgentInventoryResponder
{
	public:

	importResponder(const LLSD& post_data,
		const LLUUID& vfile_id,
		LLAssetType::EType asset_type)
	: LLNewAgentInventoryResponder(post_data, vfile_id, asset_type)
	{
	}

	//virtual 
	virtual void uploadComplete(const LLSD& content)
	{
		lldebugs << "LLNewAgentInventoryResponder::result from capabilities" << llendl;

		LLAssetType::EType asset_type = LLAssetType::lookup(mPostData["asset_type"].asString());
		LLInventoryType::EType inventory_type = LLInventoryType::lookup(mPostData["inventory_type"].asString());

		// Update L$ and ownership credit information
		// since it probably changed on the server
		if (asset_type == LLAssetType::AT_TEXTURE ||
			asset_type == LLAssetType::AT_SOUND ||
			asset_type == LLAssetType::AT_ANIMATION)
		{
			gMessageSystem->newMessageFast(_PREHASH_MoneyBalanceRequest);
			gMessageSystem->nextBlockFast(_PREHASH_AgentData);
			gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			gMessageSystem->nextBlockFast(_PREHASH_MoneyData);
			gMessageSystem->addUUIDFast(_PREHASH_TransactionID, LLUUID::null );
			gAgent.sendReliableMessage();
		}

		// Actually add the upload to viewer inventory
		llinfos << "Adding " << content["new_inventory_item"].asUUID() << " "
			<< content["new_asset"].asUUID() << " to inventory." << llendl;
		if (mPostData["folder_id"].asUUID().notNull())
		{
			LLPermissions perm;
			U32 next_owner_perm;
			perm.init(gAgent.getID(), gAgent.getID(), LLUUID::null, LLUUID::null);
			if (mPostData["inventory_type"].asString() == "snapshot")
			{
				next_owner_perm = PERM_ALL;
			}
			else
			{
				next_owner_perm = PERM_MOVE | PERM_TRANSFER;
			}
			perm.initMasks(PERM_ALL, PERM_ALL, PERM_NONE, PERM_NONE, next_owner_perm);
			S32 creation_date_now = time_corrected();
			LLPointer<LLViewerInventoryItem> item
				= new LLViewerInventoryItem(content["new_inventory_item"].asUUID(),
										mPostData["folder_id"].asUUID(),
										perm,
										content["new_asset"].asUUID(),
										asset_type,
										inventory_type,
										mPostData["name"].asString(),
										mPostData["description"].asString(),
										LLSaleInfo::DEFAULT,
										LLInventoryItem::II_FLAGS_NONE,
										creation_date_now);
			gInventory.updateItem(item);
			gInventory.notifyObservers();
		}
		else
		{
			llwarns << "Can't find a folder to put it into" << llendl;
		}

		// remove the "Uploading..." message
		LLUploadDialog::modalUploadFinished();
	
		primbackup::getInstance()->update_map(content["new_asset"].asUUID());
		primbackup::getInstance()->upload_next_asset();
	}
};

class CacheReadResponder : public LLTextureCache::ReadResponder
{
	public:
		CacheReadResponder(const LLUUID& id, LLImageFormatted* image)
			:  mFormattedImage(image), mID(id)
		{
			setImage(image);
		}
		void setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat, BOOL imagelocal)
		{
			if(imageformat==IMG_CODEC_TGA && mFormattedImage->getCodec()==IMG_CODEC_J2C)
			{
				llwarns<<"FAILED: texture "<<mID<<" is formatted as TGA. Not saving." << llendl;
				primbackup::getInstance()->mNonExportedTextures |= primbackup::TEXTURE_BAD_ENCODING;
				mFormattedImage=NULL;
				mImageSize=0;
				return;
			}

			if (mFormattedImage.notNull())
			{	
				llassert_always(mFormattedImage->getCodec() == imageformat);
				mFormattedImage->appendData(data, datasize);
			}
			else
			{
				mFormattedImage = LLImageFormatted::createFromType(imageformat);
				mFormattedImage->setData(data,datasize);
			}
			mImageSize = imagesize;
			mImageLocal = imagelocal;
		}

		virtual void completed(bool success)
		{
			if(success && (mFormattedImage.notNull()) && mImageSize>0)
			{
				
				llinfos << "SUCCESS getting texture " << mID << llendl;
				std::string name;
				mID.toString(name);
				name = primbackup::getInstance()->getfolder()+"//"+name;
				llinfos << "Saving to "<< name << llendl;			
				if (!mFormattedImage->save(name))
				{
					llinfos << "FAILED to save texture " << mID << llendl;
					primbackup::getInstance()->mNonExportedTextures |= primbackup::TEXTURE_SAVED_FAILED;
				}
			}
			else
			{
				if(!success)
				{
					llwarns << "FAILED to get texture " << mID << llendl;
					primbackup::getInstance()->mNonExportedTextures |= primbackup::TEXTURE_MISSING;
				}
				if(mFormattedImage.isNull())
				{
					llwarns << "FAILED: NULL texture " << mID << llendl;
					primbackup::getInstance()->mNonExportedTextures |= primbackup::TEXTURE_IS_NULL;
				}	
			}	

			primbackup::getInstance()->m_nexttextureready=true;
			//JUST SAY NO TO APR DEADLOCKING
			//primbackup::getInstance()->export_next_texture();
		}
	private:
		LLPointer<LLImageFormatted> mFormattedImage;
		LLUUID mID;
};

primbackup::primbackup()
:	LLFloater(std::string("Prim Import Floater"), std::string("FloaterPrimImportRect"), LLStringUtil::null)
{
	LLUICtrlFactory::getInstance()->buildFloater( this, "floater_prim_import.xml" );
	running=false;
	textures.clear();
	assetmap.clear();
	current_asset=LLUUID::null;
	m_retexture=false;
	close();
}

primbackup* primbackup::getInstance()
{
    if ( ! sInstance )
        sInstance = new primbackup();
	return sInstance;
}

primbackup::~primbackup()
{
	// save position of floater
	gSavedSettings.setRect("FloaterPrimImportRect", getRect());
	sInstance = 0;
}

void primbackup::draw()
{
	LLFloater::draw();
}

void primbackup::show(bool exporting)
{
	// set the title
	if (exporting)
	{
		setTitle("Object export");
	}
	else
	{
		setTitle("Object import");
	}
	m_curobject=1;
	m_curprim=0;
	m_objects=0;
	m_prims=0;
	rezcount=0;

	// make floater appear
	setVisibleAndFrontmost();
}


void primbackup::onClose( bool app_quitting )
{
	setVisible( false );
	// HACK for fast XML iteration replace with:
	// destroy();
}

void primbackup::updateexportnumbers()
{

	std::stringstream sstr;	
	LLUICtrl * ctrl=this->getChild<LLUICtrl>("name_label");	

	sstr<<"Export Progress \n";

	sstr << "Remaining Textures " << textures.size() << "\n";
	ctrl->setValue(LLSD("Text")=sstr.str());
}

void primbackup::updateimportnumbers()
{
	std::stringstream sstr;	
	LLUICtrl * ctrl=this->getChild<LLUICtrl>("name_label");	

	if(m_retexture)
	{
		sstr << " Textures uploads remaining : " << textures.size() << "\n";
		ctrl->setValue(LLSD("Text")=sstr.str());
	}
	else
	{
		sstr << " Textures uploads N/A \n";
		ctrl->setValue(LLSD("Text")=sstr.str());
	}

	sstr << " Objects " << this->m_curobject << "/" << this->m_objects << "\n";
	ctrl->setValue(LLSD("Text")=sstr.str());
	
	sstr << " Rez " << this->rezcount << "/" << this->m_prims;
	ctrl->setValue(LLSD("Text")=sstr.str());

	sstr << " Build " << this->m_curprim << "/" << this->m_prims;
	ctrl->setValue(LLSD("Text")=sstr.str());
}

void primbackup::pre_export_object()
{
	textures.clear();
	llsd.clear();
	this_group.clear();

	setDefaultTextures();

	// Open the file save dialog
	LLFilePicker& file_picker = LLFilePicker::instance();
	if( !file_picker.getSaveFile( LLFilePicker::FFSAVE_XML ) )
	{
		// User canceled save.
		return;
	}
	 
	file_name = file_picker.getCurFile();
	folder = gDirUtilp->getDirName(file_name);

	mNonExportedTextures = TEXTURE_OK;

	export_state=EXPORT_INIT;
	gIdleCallbacks.addFunction(exportworker, NULL);
}

bool primbackup::validatePerms(const LLPermissions *item_permissions)
{
	if (gIsInSecondLife)
	{
		// In Second Life, you must be the creator to be permitted to export the asset.
		return (gAgent.getID() == item_permissions->getOwner() &&
				gAgent.getID() == item_permissions->getCreator());
	}
	else
	{
		// Out of Second Life, simply check that the asset is full perms.
		return (gAgent.getID() == item_permissions->getOwner() &&
				(item_permissions->getMaskOwner() & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED);
	}
}

LLUUID primbackup::validateTextureID(LLUUID asset_id)
{
	if (!gIsInSecondLife)
	{
		// If we are not in Second Life, don't bother.
		return asset_id;
	}
	LLUUID texture = LL_TEXTURE_PLYWOOD;
	if (asset_id == texture ||
		asset_id == LL_TEXTURE_BLANK ||
		asset_id == LL_TEXTURE_INVISIBLE ||
		asset_id == LL_TEXTURE_TRANSPARENT ||
		asset_id == LL_TEXTURE_MEDIA)
	{
		// Allow to export a few default SL textures.
		return asset_id;
	}
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLAssetIDMatches asset_id_matches(asset_id);
	gInventory.collectDescendentsIf(LLUUID::null,
							cats,
							items,
							LLInventoryModel::INCLUDE_TRASH,
							asset_id_matches);

	if (items.count())
	{
		for (S32 i = 0; i < items.count(); i++)
		{
			const LLPermissions item_permissions = items[i]->getPermissions();
			if (validatePerms(&item_permissions))
			{
				texture = asset_id;
			}
		}
	}

	if (texture != asset_id)
	{
		mNonExportedTextures |= TEXTURE_BAD_PERM;
	}

	return texture;
}

void primbackup::exportworker(void *userdata)
{	
	primbackup::getInstance()->updateexportnumbers();

	switch(primbackup::getInstance()->export_state)
	{
		case EXPORT_INIT: {
			primbackup::getInstance()->show(true);		
			LLSelectMgr::getInstance()->getSelection()->ref();
			struct ff : public LLSelectedNodeFunctor
			{
				virtual bool apply(LLSelectNode* node)
				{
					return primbackup::getInstance()->validatePerms(node->mPermissions);
				}
			} func;

			if(LLSelectMgr::getInstance()->getSelection()->applyToNodes(&func,false))
			{
				primbackup::getInstance()->export_state=EXPORT_STRUCTURE;
			}
			else
			{
				llwarns << "Incorrect permission to export" << llendl;
				primbackup::getInstance()->export_state=EXPORT_FAILED;
				LLSelectMgr::getInstance()->getSelection()->unref();
			}
			break;
		}

		case EXPORT_STRUCTURE: {
			struct ff : public LLSelectedObjectFunctor
			{
				virtual bool apply(LLViewerObject* object)
				{
					bool is_attachment = object->isAttachment();
					object->boostTexturePriority(TRUE);
					LLViewerObject::child_list_t children = object->getChildren();
					children.push_front(object); //push root onto list
					LLSD prim_llsd = primbackup::getInstance()->primsToLLSD(children, is_attachment);				
					LLSD stuff;
					if (is_attachment)
					{
						stuff["root_position"] = object->getPositionEdit().getValue();
						stuff["root_rotation"] = ll_sd_from_quaternion(object->getRotationEdit());
					}
					else
					{
						stuff["root_position"] = object->getPosition().getValue();
						stuff["root_rotation"] = ll_sd_from_quaternion(object->getRotation());
					}
					stuff["group_body"] = prim_llsd;
					primbackup::getInstance()->llsd["data"].append(stuff);
					return true;
				}
			} func;

			primbackup::getInstance()->export_state=EXPORT_LLSD;
			LLSelectMgr::getInstance()->getSelection()->applyToRootObjects(&func,false);
			LLSelectMgr::getInstance()->getSelection()->unref();

			break;
		}

		case EXPORT_TEXTURES: {
			if(primbackup::getInstance()->m_nexttextureready==false)
				return;

			//Ok we got work to do
			primbackup::getInstance()->m_nexttextureready=false;

			if(primbackup::getInstance()->textures.empty())
			{
				primbackup::getInstance()->export_state=EXPORT_DONE;
				return;
			}

			primbackup::getInstance()->export_next_texture();
			break;
		}

		case EXPORT_LLSD: {
			// Create a file stream and write to it
			llofstream export_file(primbackup::getInstance()->file_name);
			LLSDSerialize::toPrettyXML(primbackup::getInstance()->llsd, export_file);
			export_file.close();
			primbackup::getInstance()->m_nexttextureready=true;	
			primbackup::getInstance()->export_state=EXPORT_TEXTURES;
			break;
		}	

		case EXPORT_DONE: {
			gIdleCallbacks.deleteFunction(exportworker);
			if (primbackup::getInstance()->mNonExportedTextures == primbackup::TEXTURE_OK)
			{
				llinfos << "Export successful and complete." << llendl;
				LLNotifyBox::showXml("ExportSuccessful");
			}
			else
			{
				llinfos << "Export successful but incomplete: some texture(s) were not saved." << llendl;
				std::string reason;
				if (primbackup::getInstance()->mNonExportedTextures & primbackup::TEXTURE_BAD_PERM)
				{
					reason += "\nBad permissions/creator.";
				}
				if (primbackup::getInstance()->mNonExportedTextures & primbackup::TEXTURE_MISSING)
				{
					reason += "\nMissing texture (retrying after full rezzing might work).";
				}
				if (primbackup::getInstance()->mNonExportedTextures & primbackup::TEXTURE_BAD_ENCODING)
				{
					reason += "\nBad texture encoding.";
				}
				if (primbackup::getInstance()->mNonExportedTextures & primbackup::TEXTURE_IS_NULL)
				{
					reason += "\nNull texture.";
				}
				if (primbackup::getInstance()->mNonExportedTextures & primbackup::TEXTURE_SAVED_FAILED)
				{
					reason += "\nCould not write to disk.";
				}
				LLStringUtil::format_map_t args;
				args["[REASON]"] = reason;
				gViewerWindow->alertXml("ExportPartial", args);
			}
			primbackup::getInstance()->close();
			break;
		}

		case EXPORT_FAILED: {
			gIdleCallbacks.deleteFunction(exportworker);
			llwarns << "Export process aborted." << llendl;
			gViewerWindow->alertXml("ExportFailed");
			primbackup::getInstance()->close();
			break;
		}
	}
}

LLSD primbackup::primsToLLSD(LLViewerObject::child_list_t child_list, bool is_attachment)
{
	LLViewerObject* object;
	LLSD llsd;
	char localid[16];

	for (LLViewerObject::child_list_t::iterator i = child_list.begin(); i != child_list.end(); ++i)
	{
		object = (*i);
		LLUUID id = object->getID();

		llinfos << "Exporting prim " << object->getID().asString() << llendl;

		// Create an LLSD object that represents this prim. It will be injected in to the overall LLSD
		// tree structure
		LLSD prim_llsd;

		if (!object->isRoot())
		{
			// Parent id
			snprintf(localid, sizeof(localid), "%u", object->getSubParent()->getLocalID());
			prim_llsd["parent"] = localid;
		}

		// Name and description
		LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->findNode(object);
		if (node)
		{
			prim_llsd["name"] = node->mName;
			prim_llsd["description"] = node->mDescription;
		}

		// Transforms
		if (is_attachment)
		{
			prim_llsd["position"] = object->getPositionEdit().getValue();
			prim_llsd["rotation"] = ll_sd_from_quaternion(object->getRotationEdit());
		}
		else
		{
			prim_llsd["position"] = object->getPosition().getValue();
			prim_llsd["rotation"] = ll_sd_from_quaternion(object->getRotation());
		}
		prim_llsd["scale"] = object->getScale().getValue();

		// Flags
		prim_llsd["shadows"] = object->flagCastShadows();
		prim_llsd["phantom"] = object->flagPhantom();
		prim_llsd["physical"] = (BOOL)(object->mFlags & FLAGS_USE_PHYSICS);

		// Volume params
		LLVolumeParams params = object->getVolume()->getParams();
		prim_llsd["volume"] = params.asLLSD();

		// Extra paramsb6fab961-af18-77f8-cf08-f021377a7244
		if (object->isFlexible())
		{
			// Flexible
			LLFlexibleObjectData* flex = (LLFlexibleObjectData*)object->getParameterEntry(LLNetworkData::PARAMS_FLEXIBLE);
			prim_llsd["flexible"] = flex->asLLSD();
		}
		if (object->getParameterEntryInUse(LLNetworkData::PARAMS_LIGHT))
		{
			// Light
			LLLightParams* light = (LLLightParams*)object->getParameterEntry(LLNetworkData::PARAMS_LIGHT);
			prim_llsd["light"] = light->asLLSD();
		}
		if (object->getParameterEntryInUse(LLNetworkData::PARAMS_SCULPT))
		{
			// Sculpt
			LLSculptParams* sculpt = (LLSculptParams*)object->getParameterEntry(LLNetworkData::PARAMS_SCULPT);
			prim_llsd["sculpt"] = sculpt->asLLSD();
			
			LLUUID sculpt_texture=sculpt->getSculptTexture();
			if (sculpt_texture == validateTextureID(sculpt_texture))
			{
				bool alreadyseen=false;
				std::list<LLUUID>::iterator iter;
				for(iter = textures.begin(); iter != textures.end() ; iter++) 
				{
					if( (*iter)==sculpt_texture)
					alreadyseen=true;
				}
				if(alreadyseen==false)
				{
					llinfos << "Found a sculpt texture, adding to list " << sculpt_texture << llendl;
					textures.push_back(sculpt_texture);
				}
			}
			else
			{
				llwarns << "Incorrect permission to export a sculpt texture." << llendl;
				primbackup::getInstance()->mExportState = EXPORT_FAILED;
			}
		}

		// Textures
		LLSD te_llsd;
		LLSD this_te_llsd;
		LLUUID t_id;
		U8 te_count = object->getNumTEs();
		for (U8 i = 0; i < te_count; i++)
		{
			bool alreadyseen=false;
			t_id = validateTextureID(object->getTE(i)->getID());
			this_te_llsd = object->getTE(i)->asLLSD();
			this_te_llsd["imageid"] = t_id;
			te_llsd.append(this_te_llsd);
			if (t_id != LL_TEXTURE_BLANK && t_id != LL_TEXTURE_INVISIBLE) // Do not export non-existent default textures
 			{
				std::list<LLUUID>::iterator iter;
				for(iter = textures.begin(); iter != textures.end() ; iter++) 
				{
					if ((*iter) == t_id)
					alreadyseen=true;
				}
				if(alreadyseen==false)
					textures.push_back(t_id);
			}
		}
		prim_llsd["textures"] = te_llsd;

		// The keys in the primitive maps do not have to be localids, they can be any
		// string. We simply use localids because they are a unique identifier
		snprintf(localid, sizeof(localid), "%u", object->getLocalID());
		llsd[(const char*)localid] = prim_llsd;
	}
	updateexportnumbers();
	return llsd;
}

void primbackup::export_next_texture()
{
	if(textures.empty())
	{
		llinfos << "Finished exporting textures " << llendl;
		return;
	}

	LLUUID id;
	std::list<LLUUID>::iterator iter;
	iter = textures.begin();

	while (true)
	{
		if(iter==textures.end())
		{
			m_nexttextureready=true;
			return;
		}

		id = (*iter);
		if (id.isNull())
		{
			// NULL texture id: just remove and ignore.
			textures.remove(id);
			iter = textures.begin();
			continue;
		}

		LLViewerImage * imagep = gImageList.hasImage(id);
		if(imagep!=NULL)
		{
			S32 cur_discard = imagep->getDiscardLevel();
			if(cur_discard>0)
			{
				if(imagep->getBoostLevel()!=LLViewerImage::BOOST_PREVIEW)
					imagep->setBoostLevel(LLViewerImage::BOOST_PREVIEW); //we want to force discard 0; this one does this.
			}
			else
			{
				break;
			}
		}
		else
		{
			llwarns << " We *DON'T* have the texture " << llendl;
			mNonExportedTextures |= TEXTURE_MISSING;
			textures.remove(id);
			return;
		}
		iter++;
	}

	textures.remove(id);

	llinfos << "Requesting texture " << id << llendl;
	LLImageJ2C * mFormattedImage = new LLImageJ2C;
	CacheReadResponder* responder = new CacheReadResponder(id, mFormattedImage);
  	LLAppViewer::getTextureCache()->readFromCache(id,LLWorkerThread::PRIORITY_HIGH,0,999999,responder);
}

void primbackup::import_object(bool upload)
{
	textures.clear();
	assetmap.clear();
	current_asset=LLUUID::null;

	setDefaultTextures();
	this->m_retexture=upload;

	// Open the file open dialog
	LLFilePicker& file_picker = LLFilePicker::instance();
	if( !file_picker.getOpenFile( LLFilePicker::FFLOAD_XML ) )
	{
		// User canceled save.
		return;
	}

	std::string file_name = file_picker.getFirstFile().c_str();
	folder = gDirUtilp->getDirName(file_name);
	llifstream import_file(file_name);
	LLSDSerialize::fromXML(llsd, import_file);
	import_file.close();
	show(false);

	//Get the texture map
	
	LLSD::map_const_iterator prim_it;
	LLSD::array_const_iterator prim_arr_it;
		
	this->m_curobject=1;
	this->m_curprim=1;
	this->m_objects=llsd["data"].size();
	this->m_prims=0;
	rezcount=0;
	updateimportnumbers();

	for( prim_arr_it = llsd["data"].beginArray(); prim_arr_it != llsd["data"].endArray(); prim_arr_it++)
	{
		LLSD llsd2 = (*prim_arr_it)["group_body"];

		for( prim_it = llsd2.beginMap(); prim_it != llsd2.endMap(); prim_it++)
		{
			LLSD prim_llsd = llsd2[prim_it->first];
			LLSD::array_iterator text_it;
			std::list<LLUUID>::iterator iter;

			if(prim_llsd.has("sculpt"))
			{
				LLSculptParams* sculpt=new LLSculptParams();
				sculpt->fromLLSD(prim_llsd["sculpt"]);
				LLUUID orig=sculpt->getSculptTexture();
				bool alreadyseen=false;
				for(iter = textures.begin(); iter != textures.end() ; iter++) 
				{
					if( (*iter)==orig)
						alreadyseen=true;
				}
				if(alreadyseen==false)
				{
					llinfos << "Found a new SCULPT texture to upload " << orig << llendl;			
					textures.push_back(orig);
				}
			}

			LLSD te_llsd = prim_llsd["textures"];

			for(text_it=te_llsd.beginArray(); text_it !=te_llsd.endArray(); text_it++)
			{
				LLSD the_te = (*text_it);
				LLTextureEntry te;
				te.fromLLSD(the_te);

				LLUUID id = te.getID();
				if (id != LL_TEXTURE_PLYWOOD && id != LL_TEXTURE_BLANK && id != LL_TEXTURE_INVISIBLE) // Do not upload the default textures
 				{
					bool alreadyseen = false;

					for(iter = textures.begin(); iter != textures.end() ; iter++) 
					{
						if( (*iter)==te.getID())
						alreadyseen=true;
					}
					if(alreadyseen==false)
					{
						llinfos << "Found a new texture to upload " << te.getID() << llendl;			
						textures.push_back(te.getID());
					}	     
				}
			}
		}
	}

	if(m_retexture==TRUE)
		upload_next_asset();
	else
		import_object1a();
}

LLVector3 primbackup::offset_agent(LLVector3 offset)
{
	LLVector3 pos= gAgent.getPositionAgent();
	LLQuaternion agent_rot=LLQuaternion(gAgent.getAtAxis(),gAgent.getLeftAxis(),gAgent.getUpAxis());
	pos=(offset*agent_rot+pos);
	return pos;
}

void primbackup::rez_agent_offset(LLVector3 offset)
{
	// This will break for a sitting agent
	LLToolPlacer* mPlacer = new LLToolPlacer();
	mPlacer->setObjectType(LL_PCODE_CUBE);	
	//LLVector3 pos=offset_agent(offset);
	mPlacer->placeObject((S32)offset.mV[0], (S32)offset.mV[1], MASK_NONE);
}

void primbackup::import_object1a()
{
	running=true;

	show(false);

	group_prim_import_iter=llsd["data"].beginArray();	
	root_root_pos=(*group_prim_import_iter)["root_position"];

	this->m_objects=llsd["data"].size();
	this->m_curobject=1;
	import_next_object();
}

void primbackup::import_next_object()
{
	toselect.clear();
	rezcount=0;

	this_group=(*group_prim_import_iter)["group_body"];
	prim_import_iter=this_group.beginMap();

	m_curprim=0;
	m_prims=this_group.size();
	updateimportnumbers();

	LLVector3 lgpos=(*group_prim_import_iter)["root_position"];
	group_offset=lgpos-root_root_pos;
	root_pos=offset_agent(LLVector3(2.0,0,0));
	root_rot=ll_quaternion_from_sd((*group_prim_import_iter)["root_rotation"]);

	rez_agent_offset(LLVector3(0.0,2.0,0.0));
	// Now we must wait for the callback when ViewerObjectList gets the new objects and we have the correct number selected
}

// This function takes a pointer to a viewerobject and applies the prim definition that prim_llsd has
void primbackup::xmltoprim(LLSD prim_llsd,LLViewerObject * object)
{
	LLUUID id = object->getID();
	expecting_update = object->getID();
	LLSelectMgr::getInstance()->selectObjectAndFamily(object);

	if (prim_llsd.has("name"))
	{
		LLSelectMgr::getInstance()->selectionSetObjectName(prim_llsd["name"]);
	}

	if (prim_llsd.has("description"))
	{
		LLSelectMgr::getInstance()->selectionSetObjectDescription(prim_llsd["description"]);
	}

	if (prim_llsd.has("parent"))
	{
		//we are not the root node.
		LLVector3 pos = prim_llsd["position"];
		LLQuaternion rot = ll_quaternion_from_sd(prim_llsd["rotation"]);
		object->setPositionRegion((pos*root_rot)+(root_pos+group_offset));
		object->setRotation(rot*root_rot);
	}
	else
	{
		object->setPositionRegion(root_pos+group_offset);
		LLQuaternion rot=ll_quaternion_from_sd(prim_llsd["rotation"]);
		object->setRotation(rot);
	}

	object->setScale(prim_llsd["scale"]);

	if (prim_llsd.has("shadows"))
		if (prim_llsd["shadows"].asInteger() == 1)
			object->setFlags(FLAGS_CAST_SHADOWS, true);

	if (prim_llsd.has("phantom"))
		if (prim_llsd["phantom"].asInteger() == 1)
			object->setFlags(FLAGS_PHANTOM, true);

	if (prim_llsd.has("physical"))
		if (prim_llsd["physical"].asInteger() == 1)
			object->setFlags(FLAGS_USE_PHYSICS, true);

	// Volume params
	LLVolumeParams volume_params = object->getVolume()->getParams();
	volume_params.fromLLSD(prim_llsd["volume"]);
	object->updateVolume(volume_params);

	if (prim_llsd.has("sculpt"))
	{
		LLSculptParams* sculpt = new LLSculptParams();
		sculpt->fromLLSD(prim_llsd["sculpt"]);

		// TODO: check if map is valid and only set texture if map is valid and changes

		if(assetmap[sculpt->getSculptTexture()].notNull())
		{
			LLUUID replacment=assetmap[sculpt->getSculptTexture()];
			sculpt->setSculptTexture(replacment);
		}

		object->setParameterEntry(LLNetworkData::PARAMS_SCULPT,(LLNetworkData&)(*sculpt), true);
	}
		
	if (prim_llsd.has("light"))
	{
		LLLightParams* light = new LLLightParams();
		light->fromLLSD(prim_llsd["light"]);
		object->setParameterEntry(LLNetworkData::PARAMS_LIGHT,(LLNetworkData&)(*light), true);
	}

	if (prim_llsd.has("flexible"))
	{
		LLFlexibleObjectData* flex = new LLFlexibleObjectData();
		flex->fromLLSD(prim_llsd["flexible"]);
		object->setParameterEntry(LLNetworkData::PARAMS_FLEXIBLE,(LLNetworkData&)(*flex), true);
	}

	// Textures
	llinfos << "Processing textures for prim" << llendl;
	LLSD te_llsd = prim_llsd["textures"];
	LLSD::array_iterator text_it;
	U8 i = 0;

	for (text_it = te_llsd.beginArray(); text_it != te_llsd.endArray(); text_it++)
	{
	    LLSD the_te = (*text_it);
	    LLTextureEntry te;
	    te.fromLLSD(the_te);

		if(assetmap[te.getID()].notNull())
		{
			LLUUID replacment=assetmap[te.getID()];
			te.setID(replacment);
		}

	    object->setTE(i++, te);
	}

	llinfos << "Textures done!" << llendl;

	//bump the iterator now so the callbacks hook together nicely
	//if (prim_import_iter != this_group.endMap())
	//	prim_import_iter++;

	object->sendRotationUpdate();
	object->sendTEUpdate();	
	object->sendShapeUpdate();
	LLSelectMgr::getInstance()->sendMultipleUpdate(UPD_SCALE | UPD_POSITION);

	LLSelectMgr::getInstance()->deselectAll();
}

// This is fired when the update packet is processed so we know the prim settings have stuck
void primbackup::prim_update(LLViewerObject* object)
{
	if(!running)
		return;

	if (object != NULL)
		if(object->mID!=expecting_update)
			return;

	m_curprim++;
	updateimportnumbers();
	prim_import_iter++;

	LLUUID x;
	expecting_update=x.null;

	if (prim_import_iter == this_group.endMap())
	{
		llinfos << "Trying to link" << llendl;

		if (toselect.size() > 1)
		{
			std::reverse(toselect.begin(), toselect.end());
			// Now link
			LLSelectMgr::getInstance()->deselectAll();
			LLSelectMgr::getInstance()->selectObjectAndFamily(toselect,true);
			LLSelectMgr::getInstance()->sendLink();
			LLViewerObject * root=toselect.back();
			root->setRotation(root_rot);
		}

		this->m_curobject++;
		group_prim_import_iter++;
		if(group_prim_import_iter!=llsd["data"].endArray())
		{
			import_next_object();
			return;
		}

		running=false;
		this->close();
		return;
	}

	LLSD prim_llsd = this_group[prim_import_iter->first];

	if(toselect.empty())
	{
		llwarns << "error: ran out of objects to mod" << llendl;
		return;
	}

	if(prim_import_iter!=this_group.endMap())
	{
		//rez_agent_offset(LLVector3(1.0,0,0));
		LLSD prim_llsd = this_group[prim_import_iter->first];
		process_iter++;
		xmltoprim(prim_llsd,(*process_iter));	
	}
}

// Callback when we rez a new object when the importer is running.
bool primbackup::newprim(LLViewerObject * pobject)
{
	if (running)
	{
		rezcount++;
		toselect.push_back(pobject);
		updateimportnumbers();
		prim_import_iter++;

		pobject->setPosition(this->offset_agent(LLVector3(0.0, 1.0, 0.0)));
		LLSelectMgr::getInstance()->sendMultipleUpdate(UPD_POSITION);

		if (prim_import_iter != this_group.endMap())
		{
			rez_agent_offset(LLVector3(1.0, 0.0 ,0.0));
		}
		else
		{
			llinfos << "All prims rezzed, moving to build stage" << llendl;
			// Deselecting is required to ensure that the first child prim
			// in the link set (which is also the last rezzed prim and thus
			// currently selected) will be properly renamed and desced.
			LLSelectMgr::getInstance()->deselectAll();
			prim_import_iter = this_group.beginMap();
			LLSD prim_llsd = this_group[prim_import_iter->first];
			process_iter = toselect.begin();
			xmltoprim(prim_llsd, (*process_iter));	
		}
	}
	return true;
}

void primbackup::update_map(LLUUID uploaded_asset)
{
	if (current_asset.isNull())
		return;

	assetmap.insert(std::pair<LLUUID, LLUUID>(current_asset, uploaded_asset));
	llinfos << "Mapping " << current_asset << " to " << uploaded_asset << llendl;
}

void myupload_new_resource(const LLTransactionID &tid, LLAssetType::EType asset_type,
						 std::string name, std::string desc, S32 compression_info,
						 LLAssetType::EType destination_folder_type,
						 LLInventoryType::EType inv_type, U32 next_owner_perm,
						 const std::string& display_name,
						 LLAssetStorage::LLStoreAssetCallback callback,
						 void *userdata)
{
	if (gDisconnected)
	{
		return;
	}

	LLAssetID uuid = tid.makeAssetID(gAgent.getSecureSessionID());	

	// At this point, we're ready for the upload.
	std::string upload_message = "Uploading...\n\n";
	upload_message.append(display_name);
	LLUploadDialog::modalUploadDialog(upload_message);

	std::string url = gAgent.getRegion()->getCapability("NewFileAgentInventory");
	if (!url.empty())
	{
		LLSD body;
		body["folder_id"] = gInventory.findCategoryUUIDForType((destination_folder_type == LLAssetType::AT_NONE) ? asset_type : destination_folder_type);
		body["asset_type"] = LLAssetType::lookup(asset_type);
		body["inventory_type"] = LLInventoryType::lookup(inv_type);
		body["name"] = name;
		body["description"] = desc;

		std::ostringstream llsdxml;
		LLSDSerialize::toXML(body, llsdxml);
		lldebugs << "posting body to capability: " << llsdxml.str() << llendl;
		//LLHTTPClient::post(url, body, new LLNewAgentInventoryResponder(body, uuid, asset_type));
		LLHTTPClient::post(url, body, new importResponder(body, uuid, asset_type));
	}
	else
	{
		llinfos << "NewAgentInventory capability not found. Can't upload!" << llendl;	
	}
}

void primbackup::upload_next_asset()
{
	if(textures.empty())
	{
		llinfos << " Texture list is empty, moving to rez stage" << llendl;
		current_asset=LLUUID::null;
		import_object1a();
		return;
	}

	this->updateimportnumbers();

	std::list<LLUUID>::iterator iter;
	iter=textures.begin();
	LLUUID id=(*iter);
	textures.pop_front();

	llinfos << "Got texture ID " << id << "trying to upload" << llendl;

	current_asset=id;
	std::string struid;
	id.toString(struid);
	std::string filename=folder+"//"+struid;
	LLAssetID uuid;
	LLTransactionID tid;

	// generate a new transaction ID for this asset
	tid.generate();
	uuid = tid.makeAssetID(gAgent.getSecureSessionID());

	S32 file_size;
	apr_file_t* fp = ll_apr_file_open(filename, LL_APR_RB, &file_size);
	if (fp)
	{
		const S32 buf_size = 65536;	
		U8 copy_buf[buf_size];
		LLVFile file(gVFS, uuid,  LLAssetType::AT_TEXTURE, LLVFile::WRITE);
		file.setMaxSize(file_size);
		
		while ((file_size = ll_apr_file_read(fp, copy_buf, buf_size)))
		{
			file.write(copy_buf, file_size);
		}
		apr_file_close(fp);
	}
	else
	{
		llwarns << "Unable to access output file " << filename << llendl;
		upload_next_asset();
		return;
	}

	myupload_new_resource(tid, LLAssetType::AT_TEXTURE, struid, struid, 0,
		LLAssetType::AT_TEXTURE, LLInventoryType::defaultForAssetType(LLAssetType::AT_TEXTURE),
		0x0, "Uploaded texture", NULL, NULL);
}
