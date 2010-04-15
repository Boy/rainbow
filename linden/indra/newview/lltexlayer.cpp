/** 
 * @file lltexlayer.cpp
 * @brief A texture layer. Used for avatars.
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

#include "imageids.h"
#include "llagent.h"
#include "llcrc.h"
#include "lldir.h"
#include "llglheaders.h"
#include "llimagebmp.h"
#include "llimagej2c.h"
#include "llimagetga.h"
#include "llpolymorph.h"
#include "llquantize.h"
#include "lltexlayer.h"
#include "llui.h"
#include "llvfile.h"
#include "llviewerimagelist.h"
#include "llviewerimagelist.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llxmltree.h"
#include "pipeline.h"
#include "v4coloru.h"
#include "llrender.h"

//#include "../tools/imdebug/imdebug.h"


// SJB: We really always want to use the GL cache;
// let GL page textures in and out of video RAM instead of trying to do so by hand.

// static
S32 LLTexLayerSetBuffer::sGLByteCount = 0;

//-----------------------------------------------------------------------------
// LLBakedUploadData()
//-----------------------------------------------------------------------------
LLBakedUploadData::LLBakedUploadData(LLVOAvatar* avatar,
									 LLTexLayerSet* layerset,
									 LLTexLayerSetBuffer* layerset_buffer,
									 const LLUUID & id) : 
	mAvatar(avatar),
	mLayerSet(layerset),
	mLayerSetBuffer(layerset_buffer),
	mID(id)
{ 
	mStartTime = LLFrameTimer::getTotalTime();		// Record starting time
	for( S32 i = 0; i < WT_COUNT; i++ )
	{
		LLWearable* wearable = gAgent.getWearable( (EWearableType)i);
		if( wearable )
		{
			mWearableAssets[i] = wearable->getID();
		}
	}
}

//-----------------------------------------------------------------------------
// LLTexLayerSetBuffer
// The composite image that a LLTexLayerSet writes to.  Each LLTexLayerSet has one.
//-----------------------------------------------------------------------------
LLTexLayerSetBuffer::LLTexLayerSetBuffer(LLTexLayerSet* owner, S32 width, S32 height)
	:
	// ORDER_LAST => must render these after the hints are created.
	LLDynamicTexture( width, height, 4, LLDynamicTexture::ORDER_LAST, TRUE ), 
	mNeedsUpdate( TRUE ),
	mNeedsUpload( FALSE ),
	mUploadPending( FALSE ), // Not used for any logic here, just to sync sending of updates
	mTexLayerSet( owner )	
{
	LLTexLayerSetBuffer::sGLByteCount += getSize();
}

LLTexLayerSetBuffer::~LLTexLayerSetBuffer()
{
	LLTexLayerSetBuffer::sGLByteCount -= getSize();
	destroyGLTexture();
	for (S32 order = 0; order < ORDER_COUNT; order++)
	{
		LLDynamicTexture::sInstances[order].erase(this);  // will fail in all but one case.
	}
	if (mTexLayerSet->mComposite == this)
	{
		// Destroy the pointer on this now gone buffer.
		mTexLayerSet->mComposite = NULL;
	}
}

//virtual 
void LLTexLayerSetBuffer::restoreGLTexture() 
{	
	LLDynamicTexture::restoreGLTexture() ;
}

//virtual 
void LLTexLayerSetBuffer::destroyGLTexture() 
{
	LLDynamicTexture::destroyGLTexture() ;
}

// static
void LLTexLayerSetBuffer::dumpTotalByteCount()
{
	llinfos << "Composite System GL Buffers: " << (LLTexLayerSetBuffer::sGLByteCount/1024) << "KB" << llendl;
}

void LLTexLayerSetBuffer::requestUpdate()
{
	mNeedsUpdate = TRUE;

	// If we're in the middle of uploading a baked texture, we don't care about it any more.
	// When it's downloaded, ignore it.
	mUploadID.setNull();
}

void LLTexLayerSetBuffer::requestUpload()
{
	if (!mNeedsUpload)
	{
		mNeedsUpload = TRUE;
		mUploadPending = TRUE;
	}
}

void LLTexLayerSetBuffer::cancelUpload()
{
	if (mNeedsUpload)
	{
		mNeedsUpload = FALSE;
	}
	mUploadPending = FALSE;
}

void LLTexLayerSetBuffer::pushProjection()
{
	glMatrixMode(GL_PROJECTION);
	gGL.pushMatrix();
	glLoadIdentity();
	glOrtho(0.0f, mWidth, 0.0f, mHeight, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	gGL.pushMatrix();
	glLoadIdentity();
}

void LLTexLayerSetBuffer::popProjection()
{
	glMatrixMode(GL_PROJECTION);
	gGL.popMatrix();

	glMatrixMode(GL_MODELVIEW);
	gGL.popMatrix();
}

BOOL LLTexLayerSetBuffer::needsRender()
{
	LLVOAvatar* avatar = mTexLayerSet->getAvatar();
	BOOL upload_now = mNeedsUpload && mTexLayerSet->isLocalTextureDataFinal() && gAgent.mNumPendingQueries == 0;
	BOOL needs_update = (mNeedsUpdate || upload_now) && !avatar->mAppearanceAnimating;
	if (needs_update)
	{
		BOOL invalid_skirt = avatar->getBakedTE(mTexLayerSet) == LLVOAvatar::TEX_SKIRT_BAKED && !avatar->isWearingWearableType(WT_SKIRT);
		if (invalid_skirt)
		{
			// we were trying to create a skirt texture
			// but we're no longer wearing a skirt...
			needs_update = FALSE;
			cancelUpload();
		}
		else
		{
			needs_update &= (avatar->isSelf() || (avatar->isVisible() && !avatar->isCulled()));
			needs_update &= mTexLayerSet->isLocalTextureDataAvailable();
		}
	}
	return needs_update;
}

void LLTexLayerSetBuffer::preRender(BOOL clear_depth)
{
	// Set up an ortho projection
	pushProjection();
	
	// keep depth buffer, we don't need to clear it
	LLDynamicTexture::preRender(FALSE);
}

void LLTexLayerSetBuffer::postRender(BOOL success)
{
	popProjection();

	LLDynamicTexture::postRender(success);
}

BOOL LLTexLayerSetBuffer::render()
{
	// Default color mask for tex layer render
	gGL.setColorMask(true, true);

	// do we need to upload, and do we have sufficient data to create an uploadable composite?
	// When do we upload the texture if gAgent.mNumPendingQueries is non-zero?
	BOOL upload_now = (gAgent.mNumPendingQueries == 0 && mNeedsUpload && mTexLayerSet->isLocalTextureDataFinal());
	BOOL success = TRUE;

	// Composite the color data
	LLGLSUIDefault gls_ui;
	success &= mTexLayerSet->render( mOrigin.mX, mOrigin.mY, mWidth, mHeight );
	gGL.flush();

	if( upload_now )
	{
		if (!success)
		{
			llinfos << "Failed attempt to bake " << mTexLayerSet->getBodyRegion() << llendl;
			mUploadPending = FALSE;
		}
		else
		{
			if (mTexLayerSet->isVisible())
			{
				readBackAndUpload();
			}
			else
			{
				mUploadPending = FALSE;
				mNeedsUpload = FALSE;
				LLVOAvatar*	avatar = mTexLayerSet->getAvatar();
				if (avatar)
				{
					avatar->setNewBakedTexture(avatar->getBakedTE(mTexLayerSet), IMG_INVISIBLE);
					llinfos << "Invisible baked texture set for " << mTexLayerSet->getBodyRegion() << llendl;
				}
			}
		}
	}

	// reset GL state
	gGL.setColorMask(true, true);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	// we have valid texture data now
	mTexture->setGLTextureCreated(true);
	mNeedsUpdate = FALSE;

	return success;
}

bool LLTexLayerSetBuffer::isInitialized(void) const
{
	return mTexture.notNull() && mTexture->isGLTextureCreated();
}

BOOL LLTexLayerSetBuffer::updateImmediate()
{
	mNeedsUpdate = TRUE;
	BOOL result = FALSE;

	if (needsRender())
	{
		preRender(FALSE);
		result = render();
		postRender(result);
	}

	return result;
}

void LLTexLayerSetBuffer::readBackAndUpload()
{
	// pointers for storing data to upload
	U8* baked_color_data = new U8[ mWidth * mHeight * 4 ];
	
	glReadPixels(mOrigin.mX, mOrigin.mY, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, baked_color_data );
	stop_glerror();

	llinfos << "Baked " << mTexLayerSet->getBodyRegion() << llendl;
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_TEX_BAKES);

	llassert( gAgent.getAvatarObject() == mTexLayerSet->getAvatar() );

	// We won't need our caches since we're baked now.  (Techically, we won't 
	// really be baked until this image is sent to the server and the Avatar
	// Appearance message is received.)
	mTexLayerSet->deleteCaches();

	LLGLSUIDefault gls_ui;

	LLPointer<LLImageRaw> baked_mask_image = new LLImageRaw(mWidth, mHeight, 1 );
	U8* baked_mask_data = baked_mask_image->getData(); 

	mTexLayerSet->gatherAlphaMasks(baked_mask_data, mWidth, mHeight);
//	imdebug("lum b=8 w=%d h=%d %p", mWidth, mHeight, baked_mask_data);


	// writes into baked_color_data
	const char* comment_text = NULL;

	S32 baked_image_components = 5; // red green blue bump clothing
	LLPointer<LLImageRaw> baked_image = new LLImageRaw( mWidth, mHeight, baked_image_components );
	U8* baked_image_data = baked_image->getData();
	
	comment_text = LINDEN_J2C_COMMENT_PREFIX "RGBHM"; // 5 channels: rgb, heightfield/alpha, mask

	S32 i = 0;
	for (S32 u = 0; u < mWidth; u++)
	{
		for (S32 v = 0; v < mHeight; v++)
		{
			baked_image_data[5 * i + 0] = baked_color_data[4 * i + 0];
			baked_image_data[5 * i + 1] = baked_color_data[4 * i + 1];
			baked_image_data[5 * i + 2] = baked_color_data[4 * i + 2];
			baked_image_data[5 * i + 3] = baked_color_data[4 * i + 3]; // alpha should be correct for eyelashes.
			baked_image_data[5 * i + 4] = baked_mask_data[i];
			i++;
		}
	}
	
	LLPointer<LLImageJ2C> compressedImage = new LLImageJ2C;
	compressedImage->setRate(0.f);
	LLTransactionID tid;
	LLAssetID asset_id;
	tid.generate();
	asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

	BOOL res = false;
	if( compressedImage->encode(baked_image, comment_text))
	{
		res = LLVFile::writeFile(compressedImage->getData(), compressedImage->getDataSize(),
								 gVFS, asset_id, LLAssetType::AT_TEXTURE);
		if (res)
		{
			LLPointer<LLImageJ2C> integrity_test = new LLImageJ2C;
			BOOL valid = FALSE;
			S32 file_size;
			U8* data = LLVFile::readFile(gVFS, asset_id, LLAssetType::AT_TEXTURE, &file_size);
			if (data)
			{
				valid = integrity_test->validate(data, file_size); // integrity_test will delete 'data'
			}
			else
			{
				integrity_test->setLastError("Unable to read entire file");
			}
			
			if( valid )
			{
				LLBakedUploadData* baked_upload_data = new LLBakedUploadData(gAgent.getAvatarObject(), this->mTexLayerSet, this, asset_id);
				mUploadID = asset_id;

				gAssetStorage->storeAssetData(tid,
											  LLAssetType::AT_TEXTURE,
											  LLTexLayerSetBuffer::onTextureUploadComplete,
											  baked_upload_data,
											  TRUE,		// temp_file
											  TRUE,		// is_priority
											  TRUE);	// store_local
		
				mNeedsUpload = FALSE;
			}
			else
			{
				mUploadPending = FALSE;
				llinfos << "unable to create baked upload file: corrupted" << llendl;
				LLVFile file(gVFS, asset_id, LLAssetType::AT_TEXTURE, LLVFile::WRITE);
				file.remove();
			}
		}
	}
	if (!res)
	{
		mUploadPending = FALSE;
		llinfos << "unable to create baked upload file" << llendl;
	}

	delete [] baked_color_data;
}


// static
void LLTexLayerSetBuffer::onTextureUploadComplete(const LLUUID& uuid, void* userdata, S32 result, LLExtStat ext_status) // StoreAssetData callback (not fixed)
{
	LLBakedUploadData* baked_upload_data = (LLBakedUploadData*)userdata;

	LLVOAvatar* avatar = gAgent.getAvatarObject();

	if (0 == result &&
		avatar && !avatar->isDead() &&
		baked_upload_data->mAvatar == avatar && // Sanity check: only the user's avatar should be uploading textures.
		baked_upload_data->mLayerSet->hasComposite())
	{
		LLTexLayerSetBuffer* layerset_buffer = baked_upload_data->mLayerSet->getComposite();

		if (layerset_buffer->mUploadID.isNull())
		{
			// The upload got canceled, we should be in the
			// process of baking a new texture so request an
			// upload with the new data

			// BAP: does this really belong in this callback, as
			// opposed to where the cancellation takes place?
			// suspect this does nothing.
			layerset_buffer->requestUpload();
		}
		else if (baked_upload_data->mID == layerset_buffer->mUploadID)
		{
			// This is the upload we're currently waiting for.
			layerset_buffer->mUploadID.setNull();
			layerset_buffer->mUploadPending = FALSE;

			if (result >= 0)
			{
				LLVOAvatar::ETextureIndex baked_te = avatar->getBakedTE(layerset_buffer->mTexLayerSet);
				U64 now = LLFrameTimer::getTotalTime();		// Record starting time
				llinfos << "Baked texture upload took " << (S32)((now - baked_upload_data->mStartTime) / 1000) << " ms" << llendl;
				avatar->setNewBakedTexture( baked_te, uuid );
			}
			else
			{
				// Avatar appearance is changing, ignore the upload results
				llinfos << "Baked upload failed. Reason: " << result << llendl;
				// *FIX: retry upload after n seconds, asset server could be busy
			}
		}
		else
		{
			llinfos << "Received baked texture out of date, ignored." << llendl;
		}

		avatar->dirtyMesh();
	}
	else
	{
		// Baked texture failed to upload (in which case since we
		// didn't set the new baked texture, it means that they'll try
		// and rebake it at some point in the future (after login?)),
		// or this response to upload is out of date, in which case a
		// current response should be on the way or already processed.
		llwarns << "Baked upload failed" << llendl;
	}

	delete baked_upload_data;
}

//-----------------------------------------------------------------------------
// LLTexLayerSet
// An ordered set of texture layers that get composited into a single texture.
//-----------------------------------------------------------------------------

LLTexLayerSetInfo::LLTexLayerSetInfo( )
	:
	mBodyRegion( "" ),
	mWidth( 512 ),
	mHeight( 512 ),
	mClearAlpha( TRUE )
{
}

LLTexLayerSetInfo::~LLTexLayerSetInfo( )
{
	std::for_each(mLayerInfoList.begin(), mLayerInfoList.end(), DeletePointer());
}

BOOL LLTexLayerSetInfo::parseXml(LLXmlTreeNode* node)
{
	llassert( node->hasName( "layer_set" ) );
	if( !node->hasName( "layer_set" ) )
	{
		return FALSE;
	}

	// body_region
	static LLStdStringHandle body_region_string = LLXmlTree::addAttributeString("body_region");
	if( !node->getFastAttributeString( body_region_string, mBodyRegion ) )
	{
		llwarns << "<layer_set> is missing body_region attribute" << llendl;
		return FALSE;
	}

	// width, height
	static LLStdStringHandle width_string = LLXmlTree::addAttributeString("width");
	if( !node->getFastAttributeS32( width_string, mWidth ) )
	{
		return FALSE;
	}

	static LLStdStringHandle height_string = LLXmlTree::addAttributeString("height");
	if( !node->getFastAttributeS32( height_string, mHeight ) )
	{
		return FALSE;
	}

	// Optional alpha component to apply after all compositing is complete.
	static LLStdStringHandle alpha_tga_file_string = LLXmlTree::addAttributeString("alpha_tga_file");
	node->getFastAttributeString( alpha_tga_file_string, mStaticAlphaFileName );

	static LLStdStringHandle clear_alpha_string = LLXmlTree::addAttributeString("clear_alpha");
	node->getFastAttributeBOOL( clear_alpha_string, mClearAlpha );

	// <layer>
	for (LLXmlTreeNode* child = node->getChildByName( "layer" );
		 child;
		 child = node->getNextNamedChild())
	{
		LLTexLayerInfo* info = new LLTexLayerInfo();
		if( !info->parseXml( child ))
		{
			delete info;
			return FALSE;
		}
		mLayerInfoList.push_back( info );		
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLTexLayerSet
// An ordered set of texture layers that get composited into a single texture.
//-----------------------------------------------------------------------------

BOOL LLTexLayerSet::sHasCaches = FALSE;

LLTexLayerSet::LLTexLayerSet( LLVOAvatar* avatar )
	:
	mComposite( NULL ),
	mAvatar( avatar ),
	mUpdatesEnabled( FALSE ),
	mIsVisible(TRUE),
	mBakedTexIndex(BAKED_HEAD),
	mInfo( NULL )
{
}

LLTexLayerSet::~LLTexLayerSet()
{
	deleteCaches();
	std::for_each(mLayerList.begin(), mLayerList.end(), DeletePointer());
	std::for_each(mMaskLayerList.begin(), mMaskLayerList.end(), DeletePointer());
	delete mComposite;
	mComposite = NULL;
}

//-----------------------------------------------------------------------------
// setInfo
//-----------------------------------------------------------------------------

BOOL LLTexLayerSet::setInfo(LLTexLayerSetInfo *info)
{
	llassert(mInfo == NULL);
	mInfo = info;
	//mID = info->mID; // No ID

	LLTexLayerSetInfo::layer_info_list_t::iterator iter;
	mLayerList.reserve(info->mLayerInfoList.size());
	for (iter = info->mLayerInfoList.begin(); iter != info->mLayerInfoList.end(); iter++)
	{
		LLTexLayer* layer = new LLTexLayer( this );
		if (!layer->setInfo(*iter))
		{
			mInfo = NULL;
			return FALSE;
		}
		if (!layer->isVisibilityMask())
		{
			mLayerList.push_back(layer);
		}
		else
		{
			mMaskLayerList.push_back(layer);
		}
	}

	requestUpdate();

	stop_glerror();

	return TRUE;
}

#if 0 // obsolete
//-----------------------------------------------------------------------------
// parseData
//-----------------------------------------------------------------------------

BOOL LLTexLayerSet::parseData(LLXmlTreeNode* node)
{
	LLTexLayerSetInfo *info = new LLTexLayerSetInfo;

	if (!info->parseXml(node))
	{
		delete info;
		return FALSE;
	}
	if (!setInfo(info))
	{
		delete info;
		return FALSE;
	}
	return TRUE;
}
#endif

void LLTexLayerSet::deleteCaches()
{
	for( layer_list_t::iterator iter = mLayerList.begin(); iter != mLayerList.end(); iter++ )
	{
		LLTexLayer* layer = *iter;
		layer->deleteCaches();
	}
	for (layer_list_t::iterator iter = mMaskLayerList.begin(); iter != mMaskLayerList.end(); iter++)
	{
		LLTexLayer* layer = *iter;
		layer->deleteCaches();
	}
}

// Returns TRUE if at least one packet of data has been received for each of the textures that this layerset depends on.
BOOL LLTexLayerSet::isLocalTextureDataAvailable()
{
	return mAvatar->isLocalTextureDataAvailable( this );
}


// Returns TRUE if all of the data for the textures that this layerset depends on have arrived.
BOOL LLTexLayerSet::isLocalTextureDataFinal()
{
	return mAvatar->isLocalTextureDataFinal( this );
}


void LLTexLayerSet::renderAlphaMaskTextures(S32 x, S32 y, S32 width, S32 height, bool forceClear)
{
	const LLTexLayerSetInfo *info = getInfo();

	gGL.setColorMask(false, true);
	gGL.setSceneBlendType(LLRender::BT_REPLACE);

	// (Optionally) replace alpha with a single component image from a tga file.
	if (!info->mStaticAlphaFileName.empty())
	{
		LLGLSNoAlphaTest gls_no_alpha_test;
		gGL.flush();
		{
			LLImageGL* image_gl = gTexStaticImageList.getImageGL(info->mStaticAlphaFileName, TRUE);
			if (image_gl)
			{
				LLGLSUIDefault gls_ui;
				gGL.getTexUnit(0)->bind(image_gl);
				gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_REPLACE);
				gl_rect_2d_simple_tex(width, height);
			}
		}
		gGL.flush();
	}
	else if (forceClear || info->mClearAlpha || (mMaskLayerList.size() > 0))
	{
		// Set the alpha channel to one (clean up after previous blending)
		gGL.flush();
		LLGLDisable no_alpha(GL_ALPHA_TEST);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f( 0.f, 0.f, 0.f, 1.f );

		gl_rect_2d_simple( width, height );

		gGL.flush();
	}

	// (Optional) Mask out part of the baked texture with alpha masks
	// will still have an effect even if mClearAlpha is set or the alpha component was replaced
	if (mMaskLayerList.size() > 0)
	{
		gGL.setSceneBlendType(LLRender::BT_MULT_ALPHA);
		gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_REPLACE);
		for (layer_list_t::iterator iter = mMaskLayerList.begin(); iter != mMaskLayerList.end(); iter++)
		{
			LLTexLayer* layer = *iter;
			gGL.flush();
			layer->blendAlphaTexture(x, y, width, height);
			gGL.flush();
		}
	}

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);
	gGL.setColorMask(true, true);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

BOOL LLTexLayerSet::render( S32 x, S32 y, S32 width, S32 height )
{
	BOOL success = TRUE;
	mIsVisible = TRUE;

	if (mMaskLayerList.size() > 0)
	{
		for (layer_list_t::iterator iter = mMaskLayerList.begin(); iter != mMaskLayerList.end(); iter++)
		{
			LLTexLayer* layer = *iter;
			if (layer->isInvisibleAlphaMask())
			{
				mIsVisible = FALSE;
			}
		}
	}

	LLGLSUIDefault gls_ui;
	LLGLDepthTest gls_depth(GL_FALSE, GL_FALSE);
	gGL.setColorMask(true, true);

	// clear buffer area to ensure we don't pick up UI elements
	{
		gGL.flush();
		LLGLDisable no_alpha(GL_ALPHA_TEST);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f( 0.f, 0.f, 0.f, 1.f );

		gl_rect_2d_simple( width, height );

		gGL.flush();
	}

	if (mIsVisible)
	{
		// composite color layers
		for (layer_list_t::iterator iter = mLayerList.begin(); iter != mLayerList.end(); iter++)
		{
			LLTexLayer* layer = *iter;
			if (layer->getRenderPass() == RP_COLOR || layer->getRenderPass() == RP_BUMP)
			{
				gGL.flush();
				success &= layer->render(x, y, width, height);
				gGL.flush();
			}
		}

		renderAlphaMaskTextures(x, y, width, height, false);

		stop_glerror();
	}
	else
	{
		gGL.flush();

		gGL.setSceneBlendType(LLRender::BT_REPLACE);
		LLGLDisable no_alpha(GL_ALPHA_TEST);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f( 0.f, 0.f, 0.f, 0.f );

		gl_rect_2d_simple( width, height );
		gGL.setSceneBlendType(LLRender::BT_ALPHA);

		gGL.flush();
	}

	return success;
}

void LLTexLayerSet::requestUpdate()
{
	if( mUpdatesEnabled )
	{
		createComposite();
		mComposite->requestUpdate(); 
	}
}

void LLTexLayerSet::requestUpload()
{
	createComposite();
	mComposite->requestUpload();
}

void LLTexLayerSet::cancelUpload()
{
	if(mComposite)
	{
		mComposite->cancelUpload();
	}
}

void LLTexLayerSet::createComposite()
{
	if( !mComposite )
	{
		S32 width = mInfo->mWidth;
		S32 height = mInfo->mHeight;
		// Composite other avatars at reduced resolution
		if( !mAvatar->mIsSelf )
		{
			width /= 2;
			height /= 2;
		}
		mComposite = new LLTexLayerSetBuffer(this, width, height);
	}
}

void LLTexLayerSet::destroyComposite()
{
	if( mComposite )
	{
		delete mComposite;
		mComposite = NULL;
	}
}

void LLTexLayerSet::setUpdatesEnabled( BOOL b )
{
	mUpdatesEnabled = b; 
}


void LLTexLayerSet::updateComposite()
{
	createComposite();
	mComposite->updateImmediate();
}

LLTexLayerSetBuffer* LLTexLayerSet::getComposite()
{
	createComposite();
	return mComposite;
}

void LLTexLayerSet::gatherAlphaMasks(U8 *data, S32 width, S32 height)
{
	S32 size = width * height;

	memset(data, 255, width * height);

	for( layer_list_t::iterator iter = mLayerList.begin(); iter != mLayerList.end(); iter++ )
	{
		LLTexLayer* layer = *iter;
		U8* alphaData = layer->getAlphaData();
		if (!alphaData && layer->hasAlphaParams())
		{
			LLColor4 net_color;
			layer->findNetColor( &net_color );
			layer->invalidateMorphMasks();
			layer->renderAlphaMasks(mComposite->getOriginX(), mComposite->getOriginY(), width, height, &net_color);
			alphaData = layer->getAlphaData();
		}
		if (alphaData)
		{
			for( S32 i = 0; i < size; i++ )
			{
				U8 curAlpha = data[i];
				U16 resultAlpha = curAlpha;
				resultAlpha *= (alphaData[i] + 1);
				resultAlpha = resultAlpha >> 8;
				data[i] = (U8)resultAlpha;
			}
		}
	}
	
	// Set alpha back to that of our alpha masks.
	renderAlphaMaskTextures(mComposite->getOriginX(), mComposite->getOriginY(), width, height, true);
}

void LLTexLayerSet::applyMorphMask(U8* tex_data, S32 width, S32 height, S32 num_components)
{
	for( layer_list_t::iterator iter = mLayerList.begin(); iter != mLayerList.end(); iter++ )
	{
		LLTexLayer* layer = *iter;
		layer->applyMorphMask(tex_data, width, height, num_components);
	}
}

//-----------------------------------------------------------------------------
// LLTexLayerInfo
//-----------------------------------------------------------------------------
LLTexLayerInfo::LLTexLayerInfo( )
	:
	mWriteAllChannels( FALSE ),
	mRenderPass( RP_COLOR ),
	mFixedColor( 0.f, 0.f, 0.f, 0.f ),
	mLocalTexture( -1 ),
	mStaticImageIsMask( FALSE ),
	mUseLocalTextureAlphaOnly(FALSE),
	mIsVisibilityMask(FALSE)
{
}

LLTexLayerInfo::~LLTexLayerInfo( )
{
	std::for_each(mColorInfoList.begin(), mColorInfoList.end(), DeletePointer());
	std::for_each(mAlphaInfoList.begin(), mAlphaInfoList.end(), DeletePointer());
}

BOOL LLTexLayerInfo::parseXml(LLXmlTreeNode* node)
{
	llassert( node->hasName( "layer" ) );

	// name attribute
	static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
	if( !node->getFastAttributeString( name_string, mName ) )
	{
		return FALSE;
	}
	
	static LLStdStringHandle write_all_channels_string = LLXmlTree::addAttributeString("write_all_channels");
	node->getFastAttributeBOOL( write_all_channels_string, mWriteAllChannels );

	std::string render_pass_name;
	static LLStdStringHandle render_pass_string = LLXmlTree::addAttributeString("render_pass");
	if( node->getFastAttributeString( render_pass_string, render_pass_name ) )
	{
		if( render_pass_name == "bump" )
		{
			mRenderPass = RP_BUMP;
		}
	}

	// Note: layers can have either a "global_color" attrib, a "fixed_color" attrib, or a <param_color> child.
	// global color attribute (optional)
	static LLStdStringHandle global_color_string = LLXmlTree::addAttributeString("global_color");
	node->getFastAttributeString( global_color_string, mGlobalColor );

	// Visibility mask (optional)
	BOOL is_visibility;
	static LLStdStringHandle visibility_mask_string = LLXmlTree::addAttributeString("visibility_mask");
	if (node->getFastAttributeBOOL(visibility_mask_string, is_visibility))
	{
		mIsVisibilityMask = is_visibility;
	}

	// color attribute (optional)
	LLColor4U color4u;
	static LLStdStringHandle fixed_color_string = LLXmlTree::addAttributeString("fixed_color");
	if( node->getFastAttributeColor4U( fixed_color_string, color4u ) )
	{
		mFixedColor.setVec( color4u );
	}

		// <texture> optional sub-element
	for (LLXmlTreeNode* texture_node = node->getChildByName( "texture" );
		 texture_node;
		 texture_node = node->getNextNamedChild())
	{
		std::string local_texture;
		static LLStdStringHandle tga_file_string = LLXmlTree::addAttributeString("tga_file");
		static LLStdStringHandle local_texture_string = LLXmlTree::addAttributeString("local_texture");
		static LLStdStringHandle file_is_mask_string = LLXmlTree::addAttributeString("file_is_mask");
		static LLStdStringHandle local_texture_alpha_only_string = LLXmlTree::addAttributeString("local_texture_alpha_only");
		if( texture_node->getFastAttributeString( tga_file_string, mStaticImageFileName ) )
		{
			texture_node->getFastAttributeBOOL( file_is_mask_string, mStaticImageIsMask );
		}
		else if( texture_node->getFastAttributeString( local_texture_string, local_texture ) )
		{
			texture_node->getFastAttributeBOOL( local_texture_alpha_only_string, mUseLocalTextureAlphaOnly );

			if( "upper_shirt" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_UPPER_SHIRT;
			}
			else if( "upper_bodypaint" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_UPPER_BODYPAINT;
			}
			else if( "lower_pants" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_LOWER_PANTS;
			}
			else if( "lower_bodypaint" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_LOWER_BODYPAINT;
			}
			else if( "lower_shoes" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_LOWER_SHOES;
			}
			else if( "head_bodypaint" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_HEAD_BODYPAINT;
			}
			else if( "lower_socks" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_LOWER_SOCKS;
			}
			else if( "upper_jacket" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_UPPER_JACKET;
			}
			else if( "lower_jacket" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_LOWER_JACKET;
			}
			else if( "upper_gloves" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_UPPER_GLOVES;
			}
			else if( "upper_undershirt" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_UPPER_UNDERSHIRT;
			}
			else if( "lower_underpants" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_LOWER_UNDERPANTS;
			}
			else if( "eyes_iris" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_EYES_IRIS;
			}
			else if( "skirt" == local_texture )
			{
				mLocalTexture = LLVOAvatar::LOCTEX_SKIRT;
			}
			else if ("hair_grain" == local_texture)
			{
				mLocalTexture = LLVOAvatar::LOCTEX_HAIR;
			}
			else if ("hair_alpha" == local_texture)
			{
				mLocalTexture = LLVOAvatar::LOCTEX_HAIR_ALPHA;
			}
			else if ("head_alpha" == local_texture)
			{
				mLocalTexture = LLVOAvatar::LOCTEX_HEAD_ALPHA;
			}
			else if ("upper_alpha" == local_texture)
			{
				mLocalTexture = LLVOAvatar::LOCTEX_UPPER_ALPHA;
			}
			else if ("lower_alpha" == local_texture)
			{
				mLocalTexture = LLVOAvatar::LOCTEX_LOWER_ALPHA;
			}
			else if ("eyes_alpha" == local_texture)
			{
				mLocalTexture = LLVOAvatar::LOCTEX_EYES_ALPHA;
			}
			else if ("head_tattoo" == local_texture)
			{
				mLocalTexture = LLVOAvatar::LOCTEX_HEAD_TATTOO;
			}
			else if ("upper_tattoo" == local_texture)
			{
				mLocalTexture = LLVOAvatar::LOCTEX_UPPER_TATTOO;
			}
			else if ("lower_tattoo" == local_texture)
			{
				mLocalTexture = LLVOAvatar::LOCTEX_LOWER_TATTOO;
			}
			else
			{
				llwarns << "<texture> element has invalid local_texture attribute: " << mName << " " << local_texture << llendl;
				return FALSE;
			}
		}
		else	
		{
			llwarns << "<texture> element is missing a required attribute. " << mName << llendl;
			return FALSE;
		}
	}

	for (LLXmlTreeNode* maskNode = node->getChildByName( "morph_mask" );
		 maskNode;
		 maskNode = node->getNextNamedChild())
	{
		std::string morph_name;
		static LLStdStringHandle morph_name_string = LLXmlTree::addAttributeString("morph_name");
		if (maskNode->getFastAttributeString(morph_name_string, morph_name))
		{
			BOOL invert = FALSE;
			static LLStdStringHandle invert_string = LLXmlTree::addAttributeString("invert");
			maskNode->getFastAttributeBOOL(invert_string, invert);			
			mMorphNameList.push_back(std::pair<std::string,BOOL>(morph_name,invert));
		}
	}

	// <param> optional sub-element (color or alpha params)
	for (LLXmlTreeNode* child = node->getChildByName( "param" );
		 child;
		 child = node->getNextNamedChild())
	{
		if( child->getChildByName( "param_color" ) )
		{
			// <param><param_color/></param>
			LLTexParamColorInfo* info = new LLTexParamColorInfo( );
			if (!info->parseXml(child))
			{
				delete info;
				return FALSE;
			}
			mColorInfoList.push_back( info );
		}
		else if( child->getChildByName( "param_alpha" ) )
		{
			// <param><param_alpha/></param>
			LLTexLayerParamAlphaInfo* info = new LLTexLayerParamAlphaInfo( );
			if (!info->parseXml(child))
			{
				delete info;
				return FALSE;
			}
 			mAlphaInfoList.push_back( info );
		}
	}
	
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLTexLayer
// A single texture layer, consisting of:
//		* color, consisting of either
//			* one or more color parameters (weighted colors)
//			* a reference to a global color
//			* a fixed color with non-zero alpha
//			* opaque white (the default)
//		* (optional) a texture defined by either
//			* a GUID
//			* a texture entry index (TE)
//		* (optional) one or more alpha parameters (weighted alpha textures)
//-----------------------------------------------------------------------------
LLTexLayer::LLTexLayer( LLTexLayerSet* layer_set )
	:
	mTexLayerSet( layer_set ),
	mMorphMasksValid( FALSE ),
	mStaticImageInvalid( FALSE ),
	mInfo( NULL )
{
}

LLTexLayer::~LLTexLayer()
{
	// mParamAlphaList and mParamColorList are LLViewerVisualParam's and get
	// deleted with ~LLCharacter()
	//std::for_each(mParamAlphaList.begin(), mParamAlphaList.end(), DeletePointer());
	//std::for_each(mParamColorList.begin(), mParamColorList.end(), DeletePointer());
	
	for( alpha_cache_t::iterator iter = mAlphaCache.begin();
		 iter != mAlphaCache.end(); iter++ )
	{
		U8* alpha_data = iter->second;
		delete [] alpha_data;
	}
}

//-----------------------------------------------------------------------------
// setInfo
//-----------------------------------------------------------------------------

BOOL LLTexLayer::setInfo(LLTexLayerInfo* info)
{
	//llassert(mInfo == NULL); // nyx says this is probably bogus but needs investigating
	if (mInfo != NULL) // above llassert(), but softened into a warning
	{
		llwarns << "BAD STUFF!  mInfo != NULL" << llendl;
	}
	mInfo = info;
	//mID = info->mID; // No ID

	{
		LLTexLayerInfo::morph_name_list_t::iterator iter;
		for (iter = mInfo->mMorphNameList.begin(); iter != mInfo->mMorphNameList.end(); iter++)
		{
			// *FIX: we assume that the referenced visual param is a
			// morph target, need a better way of actually looking
			// this up.
			LLPolyMorphTarget *morph_param;
			std::string *name = &(iter->first);
			morph_param = (LLPolyMorphTarget *)(getTexLayerSet()->getAvatar()->getVisualParam(name->c_str()));
			if (morph_param)
			{
				BOOL invert = iter->second;
				addMaskedMorph(morph_param, invert);
			}
		}
	}

	{
		LLTexLayerInfo::color_info_list_t::iterator iter;
		mParamColorList.reserve(mInfo->mColorInfoList.size());
		for (iter = mInfo->mColorInfoList.begin(); iter != mInfo->mColorInfoList.end(); iter++)
		{
			LLTexParamColor* param_color = new LLTexParamColor( this );
			if (!param_color->setInfo(*iter))
			{
				mInfo = NULL;
				return FALSE;
			}
			mParamColorList.push_back( param_color );
		}
	}
	{
		LLTexLayerInfo::alpha_info_list_t::iterator iter;
		mParamAlphaList.reserve(mInfo->mAlphaInfoList.size());
		for (iter = mInfo->mAlphaInfoList.begin(); iter != mInfo->mAlphaInfoList.end(); iter++)
		{
			LLTexLayerParamAlpha* param_alpha = new LLTexLayerParamAlpha( this );
			if (!param_alpha->setInfo(*iter))
			{
				mInfo = NULL;
				return FALSE;
			}
			mParamAlphaList.push_back( param_alpha );
		}
	}
	
	return TRUE;
}

#if 0 // obsolete
//-----------------------------------------------------------------------------
// parseData
//-----------------------------------------------------------------------------
BOOL LLTexLayer::parseData( LLXmlTreeNode* node )
{
	LLTexLayerInfo *info = new LLTexLayerInfo;

	if (!info->parseXml(node))
	{
		delete info;
		return FALSE;
	}
	if (!setInfo(info))
	{
		delete info;
		return FALSE;
	}
	return TRUE;
}
#endif

//-----------------------------------------------------------------------------


void LLTexLayer::deleteCaches()
{
	for( alpha_list_t::iterator iter = mParamAlphaList.begin();
		 iter != mParamAlphaList.end(); iter++ )
	{
		LLTexLayerParamAlpha* param = *iter;
		param->deleteCaches();
	}
	mStaticImageRaw = NULL;
}

BOOL LLTexLayer::render( S32 x, S32 y, S32 width, S32 height )
{
	LLGLEnable color_mat(GL_COLOR_MATERIAL);
	gPipeline.disableLights();

	LLColor4 net_color;
	BOOL color_specified = findNetColor(&net_color);
	if (mTexLayerSet->getAvatar()->mIsDummy)
	{
		color_specified = true;
		net_color = LLColor4(0.5, 0.5, 0.5, 1.0);
	}

	BOOL success = TRUE;
	
	// If you can't see the layer, don't render it.
	if( is_approx_zero( net_color.mV[VW] ) )
	{
		return success;
	}

	BOOL alpha_mask_specified = FALSE;
	alpha_list_t::iterator iter = mParamAlphaList.begin();
	if( iter != mParamAlphaList.end() )
	{
		// If we have alpha masks, but we're skipping all of them, skip the whole layer.
		// However, we can't do this optimization if we have morph masks that need updating.
/*		if( mMaskedMorphs.empty() )
		{
			BOOL skip_layer = TRUE;

			while( iter != mParamAlphaList.end() )
			{
				LLTexLayerParamAlpha* param = *iter;
		
				if( !param->getSkip() )
				{
					skip_layer = FALSE;
					break;
				}

				iter++;
			} 

			if( skip_layer )
			{
				return success;
			}
		}*/

		renderAlphaMasks( x, y, width, height, &net_color );
		alpha_mask_specified = TRUE;
		gGL.flush();
		gGL.blendFunc(LLRender::BF_DEST_ALPHA, LLRender::BF_ONE_MINUS_DEST_ALPHA);
	}

	gGL.color4fv( net_color.mV);

	if( getInfo()->mWriteAllChannels )
	{
		gGL.flush();
		gGL.setSceneBlendType(LLRender::BT_REPLACE);
	}
	else if (getInfo()->mUseLocalTextureAlphaOnly)
	{
		// Use the alpha channel only
		gGL.setColorMask(false, true);
	}

	if( (getInfo()->mLocalTexture != -1) && !getInfo()->mUseLocalTextureAlphaOnly )
	{
		{
			LLImageGL* image_gl = NULL;
			if( mTexLayerSet->getAvatar()->getLocalTextureGL( getInfo()->mLocalTexture, &image_gl ) )
			{
				if (mTexLayerSet->getAvatar()->getLocalTextureID((LLVOAvatar::ETextureIndex)getInfo()->mLocalTexture) == IMG_DEFAULT_AVATAR)
				{
					image_gl = NULL;
				}
				if( image_gl )
				{
					LLGLDisable alpha_test(getInfo()->mWriteAllChannels ? GL_ALPHA_TEST : 0);

					BOOL old_clamps = image_gl->getClampS();
					BOOL old_clampt = image_gl->getClampT();
					
					gGL.getTexUnit(0)->bind(image_gl);
					image_gl->setClamp(TRUE, TRUE);

					gl_rect_2d_simple_tex( width, height );

					image_gl->setClamp(old_clamps, old_clampt);
					gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				}
			}
		}
	}

	if( !getInfo()->mStaticImageFileName.empty() )
	{
		{
			LLImageGL* image_gl = gTexStaticImageList.getImageGL( getInfo()->mStaticImageFileName, getInfo()->mStaticImageIsMask );
			if( image_gl )
			{
				gGL.getTexUnit(0)->bind(image_gl);
				gl_rect_2d_simple_tex( width, height );
				gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			}
			else
			{
				success = FALSE;
			}
		}
	}

	if( ((-1 == getInfo()->mLocalTexture) ||
		 getInfo()->mUseLocalTextureAlphaOnly) &&
		getInfo()->mStaticImageFileName.empty() &&
		color_specified )
	{
		LLGLDisable no_alpha(GL_ALPHA_TEST);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4fv( net_color.mV);
		gl_rect_2d_simple( width, height );
	}

	if( alpha_mask_specified || getInfo()->mWriteAllChannels )
	{
		// Restore standard blend func value
		gGL.flush();
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
		stop_glerror();
	}

	if (getInfo()->mUseLocalTextureAlphaOnly)
	{
		// Restore color + alpha mode.
		gGL.setColorMask(true, true);
	}

	if( !success )
	{
		llinfos << "LLTexLayer::render() partial: " << getInfo()->mName << llendl;
	}
	return success;
}

BOOL LLTexLayer::blendAlphaTexture(S32 x, S32 y, S32 width, S32 height)
{
	BOOL success = TRUE;

	gGL.flush();
	
	if (!getInfo()->mStaticImageFileName.empty())
	{
		LLImageGL* image_gl = gTexStaticImageList.getImageGL(getInfo()->mStaticImageFileName, getInfo()->mStaticImageIsMask);
		if (image_gl)
		{
			LLGLSNoAlphaTest gls_no_alpha_test;
			gGL.getTexUnit(0)->bind(image_gl, TRUE);
			gl_rect_2d_simple_tex(width, height);
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		}
		else
		{
			success = FALSE;
		}
	}
	else
	{
		if (getInfo()->mLocalTexture >=0 && getInfo()->mLocalTexture < LLVOAvatar::TEX_NUM_ENTRIES)
		{
			LLImageGL* image_gl = NULL;
			if (mTexLayerSet->getAvatar()->getLocalTextureGL((LLVOAvatar::ETextureIndex)getInfo()->mLocalTexture, &image_gl))
			{
				if (image_gl)
				{
					LLGLSNoAlphaTest gls_no_alpha_test;
					gGL.getTexUnit(0)->bind(image_gl);
					gl_rect_2d_simple_tex(width, height);
					gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
					success = TRUE;
				}
			}
		}
	}
	
	return success;
}

U8*	LLTexLayer::getAlphaData()
{
	LLCRC alpha_mask_crc;
	const LLUUID& uuid = mTexLayerSet->getAvatar()->getLocalTextureID(getInfo()->mLocalTexture);
	alpha_mask_crc.update((U8*)(&uuid.mData), UUID_BYTES);

	for( alpha_list_t::iterator iter = mParamAlphaList.begin(); iter != mParamAlphaList.end(); iter++ )
	{
		LLTexLayerParamAlpha* param = *iter;
		F32 param_weight = param->getWeight();
		alpha_mask_crc.update((U8*)&param_weight, sizeof(F32));
	}

	U32 cache_index = alpha_mask_crc.getCRC();

	alpha_cache_t::iterator iter2 = mAlphaCache.find(cache_index);
	return (iter2 == mAlphaCache.end()) ? 0 : iter2->second;
}

BOOL LLTexLayer::findNetColor( LLColor4* net_color )
{
	// Color is either:
	//	* one or more color parameters (weighted colors)  (which may make use of a global color or fixed color)
	//	* a reference to a global color
	//	* a fixed color with non-zero alpha
	//	* opaque white (the default)

	if( !mParamColorList.empty() )
	{
		if( !getGlobalColor().empty() )
		{
			net_color->setVec( mTexLayerSet->getAvatar()->getGlobalColor( getInfo()->mGlobalColor ) );
		}
		else
		if( getInfo()->mFixedColor.mV[VW] )
		{
			net_color->setVec( getInfo()->mFixedColor );
		}
		else
		{
			net_color->setVec( 0.f, 0.f, 0.f, 0.f );
		}
		
		for( color_list_t::iterator iter = mParamColorList.begin();
			 iter != mParamColorList.end(); iter++ )
		{
			LLTexParamColor* param = *iter;
			LLColor4 param_net = param->getNetColor();
			switch( param->getOperation() )
			{
			case OP_ADD:
				*net_color += param_net;
				break;
			case OP_MULTIPLY:
				net_color->mV[VX] *= param_net.mV[VX];
				net_color->mV[VY] *= param_net.mV[VY];
				net_color->mV[VZ] *= param_net.mV[VZ];
				net_color->mV[VW] *= param_net.mV[VW];
				break;
			case OP_BLEND:
				net_color->setVec( lerp(*net_color, param_net, param->getWeight()) );
				break;
			default:
				llassert(0);
				break;
			}
		}
		return TRUE;
	}

	if( !getGlobalColor().empty() )
	{
		net_color->setVec( mTexLayerSet->getAvatar()->getGlobalColor( getGlobalColor() ) );
		return TRUE;
	}

	if( getInfo()->mFixedColor.mV[VW] )
	{
		net_color->setVec( getInfo()->mFixedColor );
		return TRUE;
	}

	net_color->setToWhite();

	return FALSE; // No need to draw a separate colored polygon
}


BOOL LLTexLayer::renderAlphaMasks( S32 x, S32 y, S32 width, S32 height, LLColor4* colorp )
{
	BOOL success = TRUE;

	llassert( !mParamAlphaList.empty() );

	gGL.setColorMask(false, true);

	alpha_list_t::iterator iter = mParamAlphaList.begin();
	LLTexLayerParamAlpha* first_param = *iter;

	// Note: if the first param is a mulitply, multiply against the current buffer's alpha
	if( !first_param || !first_param->getMultiplyBlend() )
	{
		LLGLDisable no_alpha(GL_ALPHA_TEST);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	
		// Clear the alpha
		gGL.flush();
		gGL.setSceneBlendType(LLRender::BT_REPLACE);

		gGL.color4f( 0.f, 0.f, 0.f, 0.f );
		gl_rect_2d_simple( width, height );
	}

	// Accumulate alphas
	LLGLSNoAlphaTest gls_no_alpha_test;
	gGL.color4f( 1.f, 1.f, 1.f, 1.f );

	for( iter = mParamAlphaList.begin(); iter != mParamAlphaList.end(); iter++ )
	{
		LLTexLayerParamAlpha* param = *iter;
		success &= param->render( x, y, width, height );
	}

	// Approximates a min() function
	gGL.flush();
	gGL.setSceneBlendType(LLRender::BT_MULT_ALPHA);

	// Accumulate the alpha component of the texture
	if( getInfo()->mLocalTexture != -1 )
	{
		{
			LLImageGL* image_gl = NULL;
			if( mTexLayerSet->getAvatar()->getLocalTextureGL( getInfo()->mLocalTexture, &image_gl ) )
			{
				if( image_gl && (image_gl->getComponents() == 4) )
				{
					LLGLSNoAlphaTest gls_no_alpha_test;

					BOOL old_clamps = image_gl->getClampS();
					BOOL old_clampt = image_gl->getClampT();					
					gGL.getTexUnit(0)->bind(image_gl);
					image_gl->setClamp(TRUE, TRUE);

					gl_rect_2d_simple_tex( width, height );

					image_gl->setClamp(old_clamps, old_clampt);
					gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				}
			}
		}
	}

	if( !getInfo()->mStaticImageFileName.empty() )
	{
		{
			LLImageGL* image_gl = gTexStaticImageList.getImageGL( getInfo()->mStaticImageFileName, getInfo()->mStaticImageIsMask );
			if( image_gl )
			{
				if(	(image_gl->getComponents() == 4) ||
					( (image_gl->getComponents() == 1) && getInfo()->mStaticImageIsMask ) )
				{
					LLGLSNoAlphaTest gls_no_alpha_test;
					gGL.getTexUnit(0)->bind(image_gl);
					gl_rect_2d_simple_tex( width, height );
					gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				}
			}
		}
	}

	// Draw a rectangle with the layer color to multiply the alpha by that color's alpha.
	// Note: we're still using gGL.blendFunc( GL_DST_ALPHA, GL_ZERO );
	if( colorp->mV[VW] != 1.f )
	{
		LLGLDisable no_alpha(GL_ALPHA_TEST);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4fv( colorp->mV );
		gl_rect_2d_simple( width, height );
	}


	LLGLSUIDefault gls_ui;

	gGL.setColorMask(true, true);
	
	if (success && !mMorphMasksValid && !mMaskedMorphs.empty())
	{
		LLCRC alpha_mask_crc;
		const LLUUID& uuid = mTexLayerSet->getAvatar()->getLocalTextureID(getInfo()->mLocalTexture);
		alpha_mask_crc.update((U8*)(&uuid.mData), UUID_BYTES);
		
		for( alpha_list_t::iterator iter = mParamAlphaList.begin(); iter != mParamAlphaList.end(); iter++ )
		{
			LLTexLayerParamAlpha* param = *iter;
			F32 param_weight = param->getWeight();
			alpha_mask_crc.update((U8*)&param_weight, sizeof(F32));
		}

		U32 cache_index = alpha_mask_crc.getCRC();

		alpha_cache_t::iterator iter2 = mAlphaCache.find(cache_index);
		U8* alpha_data;
		if (iter2 != mAlphaCache.end())
		{
			alpha_data = iter2->second;
		}
		else
		{
			// clear out a slot if we have filled our cache
			S32 max_cache_entries = getTexLayerSet()->getAvatar()->mIsSelf ? 4 : 1;
			while ((S32)mAlphaCache.size() >= max_cache_entries)
			{
				iter2 = mAlphaCache.begin(); // arbitrarily grab the first entry
				alpha_data = iter2->second;
				delete [] alpha_data;
				mAlphaCache.erase(iter2);
			}
			alpha_data = new U8[width * height];
			mAlphaCache[cache_index] = alpha_data;
			glReadPixels(x, y, width, height, GL_ALPHA, GL_UNSIGNED_BYTE, alpha_data);
		}
		
		getTexLayerSet()->getAvatar()->dirtyMesh();

		mMorphMasksValid = TRUE;

		for( morph_list_t::iterator iter3 = mMaskedMorphs.begin();
			 iter3 != mMaskedMorphs.end(); iter3++ )
		{
			LLMaskedMorph* maskedMorph = &(*iter3);
			maskedMorph->mMorphTarget->applyMask(alpha_data, width, height, 1, maskedMorph->mInvert);
		}
	}

	return success;
}

void LLTexLayer::applyMorphMask(U8* tex_data, S32 width, S32 height, S32 num_components)
{
	for( morph_list_t::iterator iter = mMaskedMorphs.begin();
		 iter != mMaskedMorphs.end(); iter++ )
	{
		LLMaskedMorph* maskedMorph = &(*iter);
		maskedMorph->mMorphTarget->applyMask(tex_data, width, height, num_components, maskedMorph->mInvert);
	}
}

// Returns TRUE on success.
BOOL LLTexLayer::renderImageRaw( U8* in_data, S32 in_width, S32 in_height, S32 in_components, S32 width, S32 height, BOOL is_mask )
{
	if (!in_data)
	{
		return FALSE;
	}
	GLenum format_options[4] = { GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_RGB, GL_RGBA };
	GLenum format = format_options[in_components-1];
	if( is_mask )
	{
		llassert( 1 == in_components );
		format = GL_ALPHA;
	}

	if( (in_width != VOAVATAR_SCRATCH_TEX_WIDTH) || (in_height != VOAVATAR_SCRATCH_TEX_HEIGHT) )
	{
		LLGLSNoAlphaTest gls_no_alpha_test;

		GLenum internal_format_options[4] = { GL_LUMINANCE8, GL_LUMINANCE8_ALPHA8, GL_RGB8, GL_RGBA8 };
		GLenum internal_format = internal_format_options[in_components-1];
		if( is_mask )
		{
			llassert( 1 == in_components );
			internal_format = GL_ALPHA8;
		}
		
		GLuint name = 0;
		glGenTextures(1, &name );
		stop_glerror();

		gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, name);
		stop_glerror();

		glTexImage2D(
			GL_TEXTURE_2D, 0, internal_format, 
			in_width, in_height,
			0, format, GL_UNSIGNED_BYTE, in_data );
		stop_glerror();

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

		gl_rect_2d_simple_tex( width, height );

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		glDeleteTextures(1, &name );
		stop_glerror();
	}
	else
	{
		LLGLSNoAlphaTest gls_no_alpha_test;

		if( !mTexLayerSet->getAvatar()->bindScratchTexture(format) )
		{
			return FALSE;
		}

		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, in_width, in_height, format, GL_UNSIGNED_BYTE, in_data );
		stop_glerror();

		gl_rect_2d_simple_tex( width, height );

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	}

	return TRUE;
}

void LLTexLayer::requestUpdate()
{
	mTexLayerSet->requestUpdate();
}

void LLTexLayer::addMaskedMorph(LLPolyMorphTarget* morph_target, BOOL invert)
{ 
	mMaskedMorphs.push_front(LLMaskedMorph(morph_target, invert));
}

void LLTexLayer::invalidateMorphMasks()
{
	mMorphMasksValid = FALSE;
}

BOOL LLTexLayer::isVisibilityMask() const
{
	return mInfo->mIsVisibilityMask;
}

BOOL LLTexLayer::isInvisibleAlphaMask()
{
	const LLTexLayerInfo *info = getInfo();

	if (info && info->mLocalTexture >= 0 && info->mLocalTexture < LLVOAvatar::TEX_NUM_ENTRIES)
	{
		if (mTexLayerSet->getAvatar()->getLocalTextureID((LLVOAvatar::ETextureIndex)info->mLocalTexture) == IMG_INVISIBLE)
		{
			return TRUE;
		}
	}

	return FALSE;
}

//-----------------------------------------------------------------------------
// LLTexLayerParamAlphaInfo
//-----------------------------------------------------------------------------
LLTexLayerParamAlphaInfo::LLTexLayerParamAlphaInfo( )
	:
	mMultiplyBlend( FALSE ),
	mSkipIfZeroWeight( FALSE ),
	mDomain( 0.f )
{
}

BOOL LLTexLayerParamAlphaInfo::parseXml(LLXmlTreeNode* node)
{
	llassert( node->hasName( "param" ) && node->getChildByName( "param_alpha" ) );

	if( !LLViewerVisualParamInfo::parseXml(node) )
		return FALSE;

	LLXmlTreeNode* param_alpha_node = node->getChildByName( "param_alpha" );
	if( !param_alpha_node )
	{
		return FALSE;
	}

	static LLStdStringHandle tga_file_string = LLXmlTree::addAttributeString("tga_file");
	if( param_alpha_node->getFastAttributeString( tga_file_string, mStaticImageFileName ) )
	{
		// Don't load the image file until it's actually needed.
	}
//	else
//	{
//		llwarns << "<param_alpha> element is missing tga_file attribute." << llendl;
//	}
	
	static LLStdStringHandle multiply_blend_string = LLXmlTree::addAttributeString("multiply_blend");
	param_alpha_node->getFastAttributeBOOL( multiply_blend_string, mMultiplyBlend );

	static LLStdStringHandle skip_if_zero_string = LLXmlTree::addAttributeString("skip_if_zero");
	param_alpha_node->getFastAttributeBOOL( skip_if_zero_string, mSkipIfZeroWeight );

	static LLStdStringHandle domain_string = LLXmlTree::addAttributeString("domain");
	param_alpha_node->getFastAttributeF32( domain_string, mDomain );

	return TRUE;
}

//-----------------------------------------------------------------------------
// LLTexLayerParamAlpha
//-----------------------------------------------------------------------------

// static 
LLTexLayerParamAlpha::param_alpha_ptr_list_t LLTexLayerParamAlpha::sInstances;

// static 
void LLTexLayerParamAlpha::dumpCacheByteCount()
{
	S32 gl_bytes = 0;
	getCacheByteCount( &gl_bytes );
	llinfos << "Processed Alpha Texture Cache GL:" << (gl_bytes/1024) << "KB" << llendl;
}

// static 
void LLTexLayerParamAlpha::getCacheByteCount( S32* gl_bytes )
{
	*gl_bytes = 0;

	for( param_alpha_ptr_list_t::iterator iter = sInstances.begin();
		 iter != sInstances.end(); iter++ )
	{
		LLTexLayerParamAlpha* instance = *iter;
		LLImageGL* image_gl = instance->mCachedProcessedImageGL;
		if( image_gl )
		{
			S32 bytes = (S32)image_gl->getWidth() * image_gl->getHeight() * image_gl->getComponents();

			if( image_gl->getHasGLTexture() )
			{
				*gl_bytes += bytes;
			}
		}
	}
}

LLTexLayerParamAlpha::LLTexLayerParamAlpha( LLTexLayer* layer )
	:
	mCachedProcessedImageGL( NULL ),
	mTexLayer( layer ),
	mNeedsCreateTexture( FALSE ),
	mStaticImageInvalid( FALSE ),
	mAvgDistortionVec(1.f, 1.f, 1.f),
	mCachedEffectiveWeight(0.f)
{
	sInstances.push_front( this );
}

LLTexLayerParamAlpha::~LLTexLayerParamAlpha()
{
	deleteCaches();
	sInstances.remove( this );
}

//-----------------------------------------------------------------------------
// setInfo()
//-----------------------------------------------------------------------------
BOOL LLTexLayerParamAlpha::setInfo(LLTexLayerParamAlphaInfo *info)
{
	llassert(mInfo == NULL);
	if (info->mID < 0)
		return FALSE;
	mInfo = info;
	mID = info->mID;

	mTexLayer->getTexLayerSet()->getAvatar()->addVisualParam( this );
	setWeight(getDefaultWeight(), FALSE );
	
	return TRUE;
}

//-----------------------------------------------------------------------------

void LLTexLayerParamAlpha::deleteCaches()
{
	mStaticImageTGA = NULL; // deletes image
	mCachedProcessedImageGL = NULL;
	mStaticImageRaw = NULL;
	mNeedsCreateTexture = FALSE;
}

void LLTexLayerParamAlpha::setWeight(F32 weight, BOOL set_by_user)
{
	if (mIsAnimating)
	{
		return;
	}
	F32 min_weight = getMinWeight();
	F32 max_weight = getMaxWeight();
	F32 new_weight = llclamp(weight, min_weight, max_weight);
	U8 cur_u8 = F32_to_U8( mCurWeight, min_weight, max_weight );
	U8 new_u8 = F32_to_U8( new_weight, min_weight, max_weight );
	if( cur_u8 != new_u8)
	{
		mCurWeight = new_weight;

		LLVOAvatar* avatar = mTexLayer->getTexLayerSet()->getAvatar();
		if( avatar->getSex() & getSex() )
		{
			if ( gAgent.cameraCustomizeAvatar() )
			{
				set_by_user = FALSE;
			}
			avatar->invalidateComposite( mTexLayer->getTexLayerSet(), set_by_user );
			mTexLayer->invalidateMorphMasks();
			avatar->updateMeshTextures();
		}
	}
}

void LLTexLayerParamAlpha::setAnimationTarget(F32 target_value, BOOL set_by_user)
{ 
	mTargetWeight = target_value; 
	setWeight(target_value, set_by_user); 
	mIsAnimating = TRUE;
	if (mNext)
	{
		mNext->setAnimationTarget(target_value, set_by_user);
	}
}

void LLTexLayerParamAlpha::animate(F32 delta, BOOL set_by_user)
{
	if (mNext)
	{
		mNext->animate(delta, set_by_user);
	}
}

BOOL LLTexLayerParamAlpha::getSkip()
{
	LLVOAvatar *avatar = mTexLayer->getTexLayerSet()->getAvatar();

	if( getInfo()->mSkipIfZeroWeight )
	{
		F32 effective_weight = ( avatar->getSex() & getSex() ) ? mCurWeight : getDefaultWeight();
		if (is_approx_zero( effective_weight )) 
		{
			return TRUE;
		}
	}

	EWearableType type = (EWearableType)getWearableType();
	if( (type != WT_INVALID) && !avatar->isWearingWearableType( type ) )
	{
		return TRUE;
	}

	return FALSE;
}


BOOL LLTexLayerParamAlpha::render( S32 x, S32 y, S32 width, S32 height )
{
	BOOL success = TRUE;

	F32 effective_weight = ( mTexLayer->getTexLayerSet()->getAvatar()->getSex() & getSex() ) ? mCurWeight : getDefaultWeight();
	BOOL weight_changed = effective_weight != mCachedEffectiveWeight;
	if( getSkip() )
	{
		return success;
	}

	gGL.flush();
	if( getInfo()->mMultiplyBlend )
	{
		gGL.blendFunc(LLRender::BF_DEST_ALPHA, LLRender::BF_ZERO); // Multiplication: approximates a min() function
	}
	else
	{
		gGL.setSceneBlendType(LLRender::BT_ADD);  // Addition: approximates a max() function
	}

	if( !getInfo()->mStaticImageFileName.empty() && !mStaticImageInvalid)
	{
		if( mStaticImageTGA.isNull() )
		{
			// Don't load the image file until we actually need it the first time.  Like now.
			mStaticImageTGA = gTexStaticImageList.getImageTGA( getInfo()->mStaticImageFileName );  
			// We now have something in one of our caches
			LLTexLayerSet::sHasCaches |= mStaticImageTGA.notNull() ? TRUE : FALSE;

			if( mStaticImageTGA.isNull() )
			{
				llwarns << "Unable to load static file: " << getInfo()->mStaticImageFileName << llendl;
				mStaticImageInvalid = TRUE; // don't try again.
				return FALSE;
			}
		}

		const S32 image_tga_width = mStaticImageTGA->getWidth();
		const S32 image_tga_height = mStaticImageTGA->getHeight(); 
		if(	!mCachedProcessedImageGL ||
			(mCachedProcessedImageGL->getWidth() != image_tga_width) ||
			(mCachedProcessedImageGL->getHeight() != image_tga_height) ||
			(weight_changed ))
		{
//			llinfos << "Building Cached Alpha: " << mName << ": (" << mStaticImageRaw->getWidth() << ", " << mStaticImageRaw->getHeight() << ") " << effective_weight << llendl;
			mCachedEffectiveWeight = effective_weight;

			if( !mCachedProcessedImageGL )
			{
				mCachedProcessedImageGL = new LLImageGL( image_tga_width, image_tga_height, 1, FALSE);

				// We now have something in one of our caches
				LLTexLayerSet::sHasCaches |= mCachedProcessedImageGL ? TRUE : FALSE;


				mCachedProcessedImageGL->setExplicitFormat( GL_ALPHA8, GL_ALPHA );
			}

			// Applies domain and effective weight to data as it is decoded. Also resizes the raw image if needed.
			mStaticImageRaw = NULL;
			mStaticImageRaw = new LLImageRaw;
			mStaticImageTGA->decodeAndProcess( mStaticImageRaw, getInfo()->mDomain, effective_weight );
			mNeedsCreateTexture = TRUE;
		}

		if( mCachedProcessedImageGL )
		{
			{
				// Create the GL texture, and then hang onto it for future use.
				if( mNeedsCreateTexture )
				{
					mCachedProcessedImageGL->createGLTexture(0, mStaticImageRaw);
					mNeedsCreateTexture = FALSE;
					
					gGL.getTexUnit(0)->bind(mCachedProcessedImageGL);
					mCachedProcessedImageGL->setClamp(TRUE, TRUE);
				}

				LLGLSNoAlphaTest gls_no_alpha_test;
				gGL.getTexUnit(0)->bind(mCachedProcessedImageGL);
				gl_rect_2d_simple_tex( width, height );
				gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				stop_glerror();
			}
		}

		// Don't keep the cache for other people's avatars
		// (It's not really a "cache" in that case, but the logic is the same)
		if( !mTexLayer->getTexLayerSet()->getAvatar()->mIsSelf )
		{
			mCachedProcessedImageGL = NULL;
		}
	}
	else
	{
		LLGLDisable no_alpha(GL_ALPHA_TEST);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f( 0.f, 0.f, 0.f, effective_weight );
		gl_rect_2d_simple( width, height );
	}

	return success;
}

//-----------------------------------------------------------------------------
// LLTexGlobalColorInfo
//-----------------------------------------------------------------------------

LLTexGlobalColorInfo::LLTexGlobalColorInfo()
{
}


LLTexGlobalColorInfo::~LLTexGlobalColorInfo()
{
	for_each(mColorInfoList.begin(), mColorInfoList.end(), DeletePointer());
}

BOOL LLTexGlobalColorInfo::parseXml(LLXmlTreeNode* node)
{
	// name attribute
	static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
	if( !node->getFastAttributeString( name_string, mName ) )
	{
		llwarns << "<global_color> element is missing name attribute." << llendl;
		return FALSE;
	}
	// <param> sub-element
	for (LLXmlTreeNode* child = node->getChildByName( "param" );
		 child;
		 child = node->getNextNamedChild())
	{
		if( child->getChildByName( "param_color" ) )
		{
			// <param><param_color/></param>
			LLTexParamColorInfo* info = new LLTexParamColorInfo();
			if (!info->parseXml(child))
			{
				delete info;
				return FALSE;
			}
			mColorInfoList.push_back( info );
		}
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLTexGlobalColor
//-----------------------------------------------------------------------------

LLTexGlobalColor::LLTexGlobalColor( LLVOAvatar* avatar )
	:
	mAvatar( avatar ),
	mInfo( NULL )
{
}


LLTexGlobalColor::~LLTexGlobalColor()
{
	// mParamList are LLViewerVisualParam's and get deleted with ~LLCharacter()
	//std::for_each(mParamList.begin(), mParamList.end(), DeletePointer());
}

BOOL LLTexGlobalColor::setInfo(LLTexGlobalColorInfo *info)
{
	llassert(mInfo == NULL);
	mInfo = info;
	//mID = info->mID; // No ID

	LLTexGlobalColorInfo::color_info_list_t::iterator iter;
	mParamList.reserve(mInfo->mColorInfoList.size());
	for (iter = mInfo->mColorInfoList.begin(); iter != mInfo->mColorInfoList.end(); iter++)
	{
		LLTexParamColor* param_color = new LLTexParamColor( this );
		if (!param_color->setInfo(*iter))
		{
			mInfo = NULL;
			return FALSE;
		}
		mParamList.push_back( param_color );
	}
	
	return TRUE;
}

//-----------------------------------------------------------------------------

LLColor4 LLTexGlobalColor::getColor()
{
	// Sum of color params
	if( !mParamList.empty() )
	{
		LLColor4 net_color( 0.f, 0.f, 0.f, 0.f );
		
		for( param_list_t::iterator iter = mParamList.begin();
			 iter != mParamList.end(); iter++ )
		{
			LLTexParamColor* param = *iter;
			LLColor4 param_net = param->getNetColor();
			switch( param->getOperation() )
			{
			case OP_ADD:
				net_color += param_net;
				break;
			case OP_MULTIPLY:
				net_color.mV[VX] *= param_net.mV[VX];
				net_color.mV[VY] *= param_net.mV[VY];
				net_color.mV[VZ] *= param_net.mV[VZ];
				net_color.mV[VW] *= param_net.mV[VW];
				break;
			case OP_BLEND:
				net_color = lerp(net_color, param_net, param->getWeight());
				break;
			default:
				llassert(0);
				break;
			}
		}
	
		net_color.mV[VX] = llclampf( net_color.mV[VX] );
		net_color.mV[VY] = llclampf( net_color.mV[VY] );
		net_color.mV[VZ] = llclampf( net_color.mV[VZ] );
		net_color.mV[VW] = llclampf( net_color.mV[VW] );

		return net_color;
	}
	return LLColor4( 1.f, 1.f, 1.f, 1.f );
}

//-----------------------------------------------------------------------------
// LLTexParamColorInfo
//-----------------------------------------------------------------------------
LLTexParamColorInfo::LLTexParamColorInfo()
	:
	mOperation( OP_ADD ),
	mNumColors( 0 )
{
}

BOOL LLTexParamColorInfo::parseXml(LLXmlTreeNode *node)
{
	llassert( node->hasName( "param" ) && node->getChildByName( "param_color" ) );

	if (!LLViewerVisualParamInfo::parseXml(node))
		return FALSE;

	LLXmlTreeNode* param_color_node = node->getChildByName( "param_color" );
	if( !param_color_node )
	{
		return FALSE;
	}

	std::string op_string;
	static LLStdStringHandle operation_string = LLXmlTree::addAttributeString("operation");
	if( param_color_node->getFastAttributeString( operation_string, op_string ) )
	{
		LLStringUtil::toLower(op_string);
		if		( op_string == "add" ) 		mOperation = OP_ADD;
		else if	( op_string == "multiply" )	mOperation = OP_MULTIPLY;
		else if	( op_string == "blend" )	mOperation = OP_BLEND;
	}

	mNumColors = 0;

	LLColor4U color4u;
	for (LLXmlTreeNode* child = param_color_node->getChildByName( "value" );
		 child;
		 child = param_color_node->getNextNamedChild())
	{
		if( (mNumColors < MAX_COLOR_VALUES) )
		{
			static LLStdStringHandle color_string = LLXmlTree::addAttributeString("color");
			if( child->getFastAttributeColor4U( color_string, color4u ) )
			{
				mColors[ mNumColors ].setVec(color4u);
				mNumColors++;
			}
		}
	}
	if( !mNumColors )
	{
		llwarns << "<param_color> is missing <value> sub-elements" << llendl;
		return FALSE;
	}

	if( (mOperation == OP_BLEND) && (mNumColors != 1) )
	{
		llwarns << "<param_color> with operation\"blend\" must have exactly one <value>" << llendl;
		return FALSE;
	}
	
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLTexParamColor
//-----------------------------------------------------------------------------
LLTexParamColor::LLTexParamColor( LLTexGlobalColor* tex_global_color )
	:
	mAvgDistortionVec(1.f, 1.f, 1.f),
	mTexGlobalColor( tex_global_color ),
	mTexLayer( NULL ),
	mAvatar( tex_global_color->getAvatar() )
{
}

LLTexParamColor::LLTexParamColor( LLTexLayer* layer )
	:
	mAvgDistortionVec(1.f, 1.f, 1.f),
	mTexGlobalColor( NULL ),
	mTexLayer( layer ),
	mAvatar( layer->getTexLayerSet()->getAvatar() )
{
}


LLTexParamColor::~LLTexParamColor()
{
}

//-----------------------------------------------------------------------------
// setInfo()
//-----------------------------------------------------------------------------

BOOL LLTexParamColor::setInfo(LLTexParamColorInfo *info)
{
	llassert(mInfo == NULL);
	if (info->mID < 0)
		return FALSE;
	mID = info->mID;
	mInfo = info;

	mAvatar->addVisualParam( this );
	setWeight( getDefaultWeight(), FALSE );

	return TRUE;
}

LLColor4 LLTexParamColor::getNetColor()
{
	llassert( getInfo()->mNumColors >= 1 );

	F32 effective_weight = ( mAvatar && (mAvatar->getSex() & getSex()) ) ? mCurWeight : getDefaultWeight();

	S32 index_last = getInfo()->mNumColors - 1;
	F32 scaled_weight = effective_weight * index_last;
	S32 index_start = (S32) scaled_weight;
	S32 index_end = index_start + 1;
	if( index_start == index_last )
	{
		return getInfo()->mColors[index_last];
	}
	else
	{
		F32 weight = scaled_weight - index_start;
		const LLColor4 *start = &getInfo()->mColors[ index_start ];
		const LLColor4 *end   = &getInfo()->mColors[ index_end ];
		return LLColor4( 
			(1.f - weight) * start->mV[VX] + weight * end->mV[VX],
			(1.f - weight) * start->mV[VY] + weight * end->mV[VY],
			(1.f - weight) * start->mV[VZ] + weight * end->mV[VZ],
			(1.f - weight) * start->mV[VW] + weight * end->mV[VW] );
	}
}

void LLTexParamColor::setWeight(F32 weight, BOOL set_by_user)
{
	if (mIsAnimating)
	{
		return;
	}
	F32 min_weight = getMinWeight();
	F32 max_weight = getMaxWeight();
	F32 new_weight = llclamp(weight, min_weight, max_weight);
	U8 cur_u8 = F32_to_U8( mCurWeight, min_weight, max_weight );
	U8 new_u8 = F32_to_U8( new_weight, min_weight, max_weight );
	if( cur_u8 != new_u8)
	{
		mCurWeight = new_weight;

		if( getInfo()->mNumColors <= 0 )
		{
			// This will happen when we set the default weight the first time.
			return;
		}

		if( mAvatar->getSex() & getSex() )
		{
			if( mTexGlobalColor )
			{
				mAvatar->onGlobalColorChanged( mTexGlobalColor, set_by_user );
			}
			else
			if( mTexLayer )
			{
				mAvatar->invalidateComposite( mTexLayer->getTexLayerSet(), set_by_user );
			}
		}
//		llinfos << "param " << mName << " = " << new_weight << llendl;
	}
}

void LLTexParamColor::setAnimationTarget(F32 target_value, BOOL set_by_user)
{ 
	// set value first then set interpolating flag to ignore further updates
	mTargetWeight = target_value; 
	setWeight(target_value, set_by_user);
	mIsAnimating = TRUE;
	if (mNext)
	{
		mNext->setAnimationTarget(target_value, set_by_user);
	}
}

void LLTexParamColor::animate(F32 delta, BOOL set_by_user)
{
	if (mNext)
	{
		mNext->animate(delta, set_by_user);
	}
}


//-----------------------------------------------------------------------------
// LLTexStaticImageList
//-----------------------------------------------------------------------------

// static
LLTexStaticImageList gTexStaticImageList;
LLStringTable LLTexStaticImageList::sImageNames(16384);

LLTexStaticImageList::LLTexStaticImageList()
	:
	mGLBytes( 0 ),
	mTGABytes( 0 )
{}

LLTexStaticImageList::~LLTexStaticImageList()
{
	deleteCachedImages();
}

void LLTexStaticImageList::dumpByteCount()
{
	llinfos << "Avatar Static Textures " <<
		"KB GL:" << (mGLBytes / 1024) <<
		"KB TGA:" << (mTGABytes / 1024) << "KB" << llendl;
}

void LLTexStaticImageList::deleteCachedImages()
{
	if( mGLBytes || mTGABytes )
	{
		llinfos << "Clearing Static Textures " <<
			"KB GL:" << (mGLBytes / 1024) <<
			"KB TGA:" << (mTGABytes / 1024) << "KB" << llendl;

		//mStaticImageLists uses LLPointers, clear() will cause deletion
		
		mStaticImageListTGA.clear();
		mStaticImageListGL.clear();
		
		mGLBytes = 0;
		mTGABytes = 0;
	}
}

// Note: in general, for a given image image we'll call either getImageTga() or getImageGL().
// We call getImageTga() if the image is used as an alpha gradient.
// Otherwise, we call getImageGL()

// Returns an LLImageTGA that contains the encoded data from a tga file named file_name.
// Caches the result to speed identical subsequent requests.
LLImageTGA* LLTexStaticImageList::getImageTGA(const std::string& file_name)
{
	const char *namekey = sImageNames.addString(file_name);
	image_tga_map_t::iterator iter = mStaticImageListTGA.find(namekey);
	if( iter != mStaticImageListTGA.end() )
	{
		return iter->second;
	}
	else
	{
		std::string path;
		path = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER,file_name);
		LLPointer<LLImageTGA> image_tga = new LLImageTGA( path );
		if( image_tga->getDataSize() > 0 )
		{
			mStaticImageListTGA[ namekey ] = image_tga;
			mTGABytes += image_tga->getDataSize();
			return image_tga;
		}
		else
		{
			return NULL;
		}
	}
}



// Returns a GL Image (without a backing ImageRaw) that contains the decoded data from a tga file named file_name.
// Caches the result to speed identical subsequent requests.
LLImageGL* LLTexStaticImageList::getImageGL(const std::string& file_name, BOOL is_mask )
{
	LLPointer<LLImageGL> image_gl;
	const char *namekey = sImageNames.addString(file_name);

	image_gl_map_t::iterator iter = mStaticImageListGL.find(namekey);
	if( iter != mStaticImageListGL.end() )
	{
		image_gl = iter->second;
	}
	else
	{
		image_gl = new LLImageGL( FALSE );
		LLPointer<LLImageRaw> image_raw = new LLImageRaw;
		if( loadImageRaw( file_name, image_raw ) )
		{
			if( (image_raw->getComponents() == 1) && is_mask )
			{
				// Note: these are static, unchanging images so it's ok to assume
				// that once an image is a mask it's always a mask.
				image_gl->setExplicitFormat( GL_ALPHA8, GL_ALPHA );
			}
			image_gl->createGLTexture(0, image_raw);

			gGL.getTexUnit(0)->bind(image_gl);
			image_gl->setClamp(TRUE, TRUE);

			mStaticImageListGL [ namekey ] = image_gl;
			mGLBytes += (S32)image_gl->getWidth() * image_gl->getHeight() * image_gl->getComponents();
		}
		else
		{
			image_gl = NULL;
		}
	}

	return image_gl;
}

// Reads a .tga file, decodes it, and puts the decoded data in image_raw.
// Returns TRUE if successful.
BOOL LLTexStaticImageList::loadImageRaw( const std::string& file_name, LLImageRaw* image_raw )
{
	BOOL success = FALSE;
	std::string path;
	path = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER,file_name);
	LLPointer<LLImageTGA> image_tga = new LLImageTGA( path );
	if( image_tga->getDataSize() > 0 )
	{
		// Copy data from tga to raw.
		success = image_tga->decode( image_raw );
	}

	return success;
}

//-----------------------------------------------------------------------------
// LLMaskedMorph()
//-----------------------------------------------------------------------------
LLMaskedMorph::LLMaskedMorph( LLPolyMorphTarget *morph_target, BOOL invert ) : mMorphTarget(morph_target), mInvert(invert)
{
	morph_target->addPendingMorphMask();
}

