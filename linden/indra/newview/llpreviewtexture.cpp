/** 
 * @file llpreviewtexture.cpp
 * @brief LLPreviewTexture class implementation
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

#include "llpreviewtexture.h"

#include "llagent.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llfilepicker.h"
#include "llimagetga.h"
#include "llinventoryview.h"
#include "llinventory.h"
#include "llresmgr.h"
#include "lltextbox.h"
#include "lltextureview.h"
#include "llui.h"
#include "llviewerimage.h"
#include "llviewerimagelist.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "lllineeditor.h"

const S32 PREVIEW_TEXTURE_MIN_WIDTH = 300;
const S32 PREVIEW_TEXTURE_MIN_HEIGHT = 120;

const F32 PREVIEW_TEXTURE_MAX_ASPECT = 200.f;
const F32 PREVIEW_TEXTURE_MIN_ASPECT = 0.005f;

const S32 CLIENT_RECT_VPAD = 4;

const F32 SECONDS_TO_SHOW_FILE_SAVED_MSG = 8.f;

LLPreviewTexture::LLPreviewTexture(const std::string& name,
								   const LLRect& rect,
								   const std::string& title,
								   const LLUUID& item_uuid,
								   const LLUUID& object_id,
								   BOOL show_keep_discard)
:	LLPreview(name, rect, title, item_uuid, object_id, TRUE, PREVIEW_TEXTURE_MIN_WIDTH, PREVIEW_TEXTURE_MIN_HEIGHT ),
	mLoadingFullImage( FALSE ),
	mShowKeepDiscard(show_keep_discard),
	mCopyToInv(FALSE),
	mIsCopyable(FALSE),
	mLastHeight(0),
	mLastWidth(0),
	mAspectRatio(0.f)
{
	const LLInventoryItem *item = getItem();
	if(item)
	{
		mImageID = item->getAssetUUID();
		const LLPermissions& perm = item->getPermissions();
		U32 mask = PERM_NONE;
		if(perm.getOwner() == gAgent.getID())
		{
			mask = perm.getMaskBase();
		}
		else if(gAgent.isInGroup(perm.getGroup()))
		{
			mask = perm.getMaskGroup();
		}
		else
		{
			mask = perm.getMaskEveryone();
		}
		if((mask & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED)
		{
			mIsCopyable = TRUE;
		}
	}

	init();

	setTitle(title);

	if (!getHost())
	{
		LLRect curRect = getRect();
		translate(rect.mLeft - curRect.mLeft, rect.mTop - curRect.mTop);
	}
}


// Note: uses asset_id as a dummy item id.
LLPreviewTexture::LLPreviewTexture(
	const std::string& name,
	const LLRect& rect,
	const std::string& title,
	const LLUUID& asset_id,
	BOOL copy_to_inv)
	:
	LLPreview(
		name,
		rect,
		title,
		asset_id,
		LLUUID::null,
		TRUE,
		PREVIEW_TEXTURE_MIN_WIDTH,
		PREVIEW_TEXTURE_MIN_HEIGHT ),
	mImageID(asset_id),
	mLoadingFullImage( FALSE ),
	mShowKeepDiscard(FALSE),
	mCopyToInv(copy_to_inv),
	mIsCopyable(TRUE),
	mLastHeight(0),
	mLastWidth(0),
	mAspectRatio(0.f)
{

	init();

	setTitle(title);

	LLRect curRect = getRect();
	translate(curRect.mLeft - rect.mLeft, curRect.mTop - rect.mTop);
	
}


LLPreviewTexture::~LLPreviewTexture()
{
	if( mLoadingFullImage )
	{
		getWindow()->decBusyCount();
	}

	mImage = NULL;
}


void LLPreviewTexture::init()
{
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_preview_texture.xml");
	
	childSetVisible("desc", !mCopyToInv);	// Hide description field for embedded textures
	childSetVisible("desc txt", !mCopyToInv);
	childSetVisible("Copy To Inventory", mCopyToInv);
	childSetVisible("Keep", mShowKeepDiscard);
	childSetVisible("Discard", mShowKeepDiscard);
	
	if (mCopyToInv) 
	{
		childSetAction("Copy To Inventory",LLPreview::onBtnCopyToInv,this);
	}

	else if (mShowKeepDiscard)
	{
		childSetAction("Keep",onKeepBtn,this);
		childSetAction("Discard",onDiscardBtn,this);
	}

	else 
	{
		// If the buttons are hidden move stuff down to use the space.
		
		LLRect keep_rect, old_rect, new_rect;
		S32 diff;
		
		childGetRect("Keep", keep_rect);
		childGetRect("combo_aspect_ratio", old_rect);
		
		diff = old_rect.mBottom - keep_rect.mBottom;
		
		new_rect.setOriginAndSize(old_rect.mLeft, old_rect.mBottom - diff,
								  old_rect.getWidth(), old_rect.getHeight());
		childSetRect("combo_aspect_ratio", new_rect);

		childGetRect("aspect_ratio", old_rect);
		new_rect.setOriginAndSize(old_rect.mLeft, old_rect.mBottom - diff,
								  old_rect.getWidth(), old_rect.getHeight());
		childSetRect("aspect_ratio", new_rect);

		childGetRect("dimensions", old_rect);
		new_rect.setOriginAndSize(old_rect.mLeft, old_rect.mBottom - diff,
								  old_rect.getWidth(), old_rect.getHeight());
		childSetRect("dimensions", new_rect);
	}


	if (!mCopyToInv) 
	{
		const LLInventoryItem* item = getItem();
		
		if (item)
		{
			childSetCommitCallback("desc", LLPreview::onText, this);
			childSetText("desc", item->getDescription());
			childSetPrevalidate("desc", &LLLineEditor::prevalidatePrintableNotPipe);
		}
	}

	childSetCommitCallback("combo_aspect_ratio", onAspectRatioCommit, this);
	LLComboBox* combo = getChild<LLComboBox>("combo_aspect_ratio");
	combo->setCurrentByIndex(0);
}

void LLPreviewTexture::draw()
{
	updateDimensions();

	LLPreview::draw();

	if (!isMinimized())
	{
		LLGLSUIDefault gls_ui;
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		
		const LLRect& border = mClientRect;
		LLRect interior = mClientRect;
		interior.stretch( -PREVIEW_BORDER_WIDTH );

		// ...border
		gl_rect_2d( border, LLColor4(0.f, 0.f, 0.f, 1.f));
		gl_rect_2d_checkerboard( interior );

		if ( mImage.notNull() )
		{
			// Draw the texture
			glColor3f( 1.f, 1.f, 1.f );
			gl_draw_scaled_image(interior.mLeft,
								interior.mBottom,
								interior.getWidth(),
								interior.getHeight(),
								mImage);

			// Pump the texture priority
			F32 pixel_area = mLoadingFullImage ? (F32)MAX_IMAGE_AREA  : (F32)(interior.getWidth() * interior.getHeight() );
			mImage->addTextureStats( pixel_area );

			// Don't bother decoding more than we can display, unless
			// we're loading the full image.
			if (!mLoadingFullImage)
			{
				S32 int_width = interior.getWidth();
				S32 int_height = interior.getHeight();
				mImage->setKnownDrawSize(int_width, int_height);
			}
			else
			{
				// Don't use this feature
				mImage->setKnownDrawSize(0, 0);
			}

			if( mLoadingFullImage )
			{
				// *TODO: Translate
				LLFontGL::sSansSerif->renderUTF8(std::string("Receiving:"), 0,
					interior.mLeft + 4, 
					interior.mBottom + 4,
					LLColor4::white, LLFontGL::LEFT, LLFontGL::BOTTOM,
					LLFontGL::DROP_SHADOW);
				
				F32 data_progress = mImage->mDownloadProgress;
				
				// Draw the progress bar.
				const S32 BAR_HEIGHT = 12;
				const S32 BAR_LEFT_PAD = 80;
				S32 left = interior.mLeft + 4 + BAR_LEFT_PAD;
				S32 bar_width = getRect().getWidth() - left - RESIZE_HANDLE_WIDTH - 2;
				S32 top = interior.mBottom + 4 + BAR_HEIGHT;
				S32 right = left + bar_width;
				S32 bottom = top - BAR_HEIGHT;

				LLColor4 background_color(0.f, 0.f, 0.f, 0.75f);
				LLColor4 decoded_color(0.f, 1.f, 0.f, 1.0f);
				LLColor4 downloaded_color(0.f, 0.5f, 0.f, 1.0f);

				gl_rect_2d(left, top, right, bottom, background_color);

				if (data_progress > 0.0f)
				{
					// Downloaded bytes
					right = left + llfloor(data_progress * (F32)bar_width);
					if (right > left)
					{
						gl_rect_2d(left, top, right, bottom, downloaded_color);
					}
				}
			}
			else
			if( !mSavedFileTimer.hasExpired() )
			{
				// *TODO: Translate
				LLFontGL::sSansSerif->renderUTF8(std::string("File Saved"), 0,
					interior.mLeft + 4,
					interior.mBottom + 4,
					LLColor4::white, LLFontGL::LEFT, LLFontGL::BOTTOM,
					LLFontGL::DROP_SHADOW);
			}
		}
	} 
}


// virtual
BOOL LLPreviewTexture::canSaveAs() const
{
	return mIsCopyable && !mLoadingFullImage && mImage.notNull() && !mImage->isMissingAsset();
}


// virtual
void LLPreviewTexture::saveAs()
{
	if( mLoadingFullImage ) return;

	LLFilePicker& file_picker = LLFilePicker::instance();
	const LLViewerInventoryItem* item = getItem() ;
	if( !file_picker.getSaveFile( LLFilePicker::FFSAVE_TGA, item ? LLDir::getScrubbedFileName(item->getName()) : LLStringUtil::null) )
	{
		// User canceled or we failed to acquire save file.
		return;
	}
	// remember the user-approved/edited file name.
	mSaveFileName = file_picker.getFirstFile();
	mLoadingFullImage = TRUE;
	getWindow()->incBusyCount();
	mImage->setLoadedCallback( LLPreviewTexture::onFileLoadedForSave, 
								0, TRUE, FALSE, new LLUUID( mItemUUID ) );
}


// static
void LLPreviewTexture::onFileLoadedForSave(BOOL success, 
											LLViewerImage *src_vi,
											LLImageRaw* src, 
											LLImageRaw* aux_src, 
											S32 discard_level,
											BOOL final,
											void* userdata)
{
	LLUUID* item_uuid = (LLUUID*) userdata;
	LLPreviewTexture* self = NULL;
	preview_map_t::iterator found_it = LLPreview::sInstances.find(*item_uuid);
	if(found_it != LLPreview::sInstances.end())
	{
		self = (LLPreviewTexture*) found_it->second;
	}

	if( final || !success )
	{
		delete item_uuid;

		if( self )
		{
			self->getWindow()->decBusyCount();
			self->mLoadingFullImage = FALSE;
		}
	}

	if( self && final && success )
	{
		LLPointer<LLImageTGA> image_tga = new LLImageTGA;
		if( !image_tga->encode( src ) )
		{
			LLStringUtil::format_map_t args;
			args["[FILE]"] = self->mSaveFileName;
			gViewerWindow->alertXml("CannotEncodeFile", args);
		}
		else if( !image_tga->save( self->mSaveFileName ) )
		{
			LLStringUtil::format_map_t args;
			args["[FILE]"] = self->mSaveFileName;
			gViewerWindow->alertXml("CannotWriteFile", args);
		}
		else
		{
			self->mSavedFileTimer.reset();
			self->mSavedFileTimer.setTimerExpirySec( SECONDS_TO_SHOW_FILE_SAVED_MSG );
		}

		self->mSaveFileName.clear();
	}

	if( self && !success )
	{
		gViewerWindow->alertXml("CannotDownloadFile");
	}
}


// It takes a while until we get height and width information.
// When we receive it, reshape the window accordingly.
void LLPreviewTexture::updateDimensions()
{
	if (!mImage) return;

	S32 image_height = llmax(1, mImage->getHeight(0));
	S32 image_width = llmax(1, mImage->getWidth(0));
	// Attempt to make the image 1:1 on screen.
	// If that fails, cut width by half.
	S32 client_width = image_width;
	S32 client_height = image_height;
	S32 horiz_pad = 2 * (LLPANEL_BORDER_WIDTH + PREVIEW_PAD) + PREVIEW_RESIZE_HANDLE_SIZE;
	S32 vert_pad = PREVIEW_HEADER_SIZE + 2 * CLIENT_RECT_VPAD + LLPANEL_BORDER_WIDTH;	
	S32 max_client_width = gViewerWindow->getWindowWidth() - horiz_pad;
	S32 max_client_height = gViewerWindow->getWindowHeight() - vert_pad;

	if (mAspectRatio > 0.f) client_height = llceil((F32)client_width / mAspectRatio);

	while ((client_width > max_client_width) ||
	       (client_height > max_client_height ) )
	{
		client_width /= 2;
		client_height /= 2;
	}

	S32 view_width = client_width + horiz_pad;
	S32 view_height = client_height + vert_pad;
	
	// set text on dimensions display (should be moved out of here and into a callback of some sort)
	childSetTextArg("dimensions", "[WIDTH]", llformat("%d", mImage->mFullWidth));
	childSetTextArg("dimensions", "[HEIGHT]", llformat("%d", mImage->mFullHeight));
	
	// add space for dimensions and aspect ratio
	S32 info_height = 0;
	LLRect aspect_rect;
	childGetRect("combo_aspect_ratio", aspect_rect);
	S32 aspect_height = aspect_rect.getHeight();
	info_height += aspect_height + CLIENT_RECT_VPAD;
	view_height += info_height;
	
	S32 button_height = 0;
	if (mShowKeepDiscard || mCopyToInv) {  //mCopyToInvBtn

		// add space for buttons
		view_height += 	BTN_HEIGHT + CLIENT_RECT_VPAD;
		button_height = BTN_HEIGHT + PREVIEW_PAD;
	}

	view_width = llmax(view_width, getMinWidth());
	view_height = llmax(view_height, getMinHeight());
	
	if (client_height != mLastHeight || client_width != mLastWidth)
	{
		mLastWidth = client_width;
		mLastHeight = client_height;

		S32 old_top = getRect().mTop;
		S32 old_left = getRect().mLeft;
		if (getHost())
		{
			getHost()->growToFit(view_width, view_height);
		}
		else
		{
			reshape( view_width, view_height );
			S32 new_bottom = old_top - getRect().getHeight();
			setOrigin( old_left, new_bottom );
			// Try to keep whole view onscreen, don't allow partial offscreen.
			gFloaterView->adjustToFitScreen(this, FALSE);
		}
	}

	
	if (!mUserResized)
	{
		// clamp texture size to fit within actual size of floater after attempting resize
		client_width = llmin(client_width, getRect().getWidth() - horiz_pad);
		client_height = llmin(client_height, getRect().getHeight() - PREVIEW_HEADER_SIZE 
						- (2 * CLIENT_RECT_VPAD) - LLPANEL_BORDER_WIDTH - info_height);

		
	}
	else
	{
		client_width = getRect().getWidth() - horiz_pad;
		if (mAspectRatio > 0.f)
		{
			client_height = llround(client_width / mAspectRatio);
		}
		else
		{
			client_height = getRect().getHeight() - vert_pad;
		}
	}

	S32 max_height = getRect().getHeight() - PREVIEW_BORDER - button_height
		- CLIENT_RECT_VPAD - info_height - CLIENT_RECT_VPAD - PREVIEW_HEADER_SIZE;

	if (mAspectRatio > 0.f)
	{
		max_height = llmax(max_height, 1);

		if (client_height > max_height)
		{
			client_height = max_height;
			client_width = llround(client_height * mAspectRatio);
		}
	}
	else
	{
		S32 max_width = getRect().getWidth() - horiz_pad;

		client_height = llclamp(client_height, 1, max_height);
		client_width = llclamp(client_width, 1, max_width);
	}
	
	LLRect window_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
	window_rect.mTop -= (PREVIEW_HEADER_SIZE + CLIENT_RECT_VPAD);
	window_rect.mBottom += PREVIEW_BORDER + button_height + CLIENT_RECT_VPAD + info_height + CLIENT_RECT_VPAD;

	mClientRect.setLeftTopAndSize(window_rect.getCenterX() - (client_width / 2), window_rect.mTop, client_width, client_height);
	
	// Hide the aspect ratio label if the window is too narrow
	// Assumes the label should be to the right of the dimensions
	LLRect dim_rect, aspect_label_rect;
	childGetRect("aspect_ratio", aspect_label_rect);
	childGetRect("dimensions", dim_rect);
	childSetVisible("aspect_ratio", dim_rect.mRight < aspect_label_rect.mLeft);
}

void LLPreviewTexture::setAspectRatio(const F32 ratio)
{
	if (0.f >= ratio)
	{
		mAspectRatio = 0.f;
	}
	else
	{
		mAspectRatio = llclamp(ratio, PREVIEW_TEXTURE_MIN_ASPECT, PREVIEW_TEXTURE_MAX_ASPECT);
	}
}

void LLPreviewTexture::onAspectRatioCommit(LLUICtrl* ctrl, void* userdata)
{
	LLPreviewTexture* self = (LLPreviewTexture*) userdata;
	
	std::string ratio(ctrl->getValue().asString());
	std::string::size_type separator(ratio.find_first_of(":/\\"));
	
	if (std::string::npos == separator) {
		// If there's no separator assume we want an unconstrained ratio
		self->setAspectRatio(0.f);
		return;
	}
	
	F32 width, height;
	std::istringstream numerator(ratio.substr(0, separator));
	std::istringstream denominator(ratio.substr(separator + 1));
	numerator >> width;
	denominator >> height;
	
	if (0.f < height && 0.f < width)
	{
		self->setAspectRatio(width / height);
	}
	else
	{
		self->setAspectRatio(0.f);
	}
}

void LLPreviewTexture::loadAsset()
{
	mImage = gImageList.getImage(mImageID, MIPMAP_TRUE, FALSE);
	mImage->setBoostLevel(LLViewerImage::BOOST_PREVIEW);
	mAssetStatus = PREVIEW_ASSET_LOADING;
}

LLPreview::EAssetStatus LLPreviewTexture::getAssetStatus()
{
	if (mImage.notNull() && (mImage->mFullWidth * mImage->mFullHeight > 0))
	{
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
	return mAssetStatus;
}
