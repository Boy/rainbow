/** 
 * @file llbutton.cpp
 * @brief LLButton base class
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

#include "linden_common.h"

#include "llbutton.h"

// Linden library includes
#include "v4color.h"
#include "llstring.h"

// Project includes
#include "llhtmlhelp.h"
#include "llkeyboard.h"
#include "llui.h"
#include "lluiconstants.h"
#include "llresmgr.h"
#include "llcriticaldamp.h"
#include "llfocusmgr.h"
#include "llwindow.h"
#include "llrender.h"

static LLRegisterWidget<LLButton> r("button");

// globals loaded from settings.xml
S32	LLBUTTON_ORIG_H_PAD	= 6; // Pre-zoomable UI
S32	LLBUTTON_H_PAD	= 0;
S32	LLBUTTON_V_PAD	= 0;
S32 BTN_HEIGHT_SMALL= 0;
S32 BTN_HEIGHT		= 0;

S32 BTN_GRID		= 12;
S32 BORDER_SIZE = 1;

LLButton::LLButton(	const std::string& name, const LLRect& rect, const std::string& control_name, void (*click_callback)(void*), void *callback_data)
:	LLUICtrl(name, rect, TRUE, NULL, NULL),
	mClickedCallback( click_callback ),
	mMouseDownCallback( NULL ),
	mMouseUpCallback( NULL ),
	mHeldDownCallback( NULL ),
	mGLFont( NULL ),
	mMouseDownFrame( 0 ),
	mHeldDownDelay( 0.5f ),			// seconds until held-down callback is called
	mHeldDownFrameDelay( 0 ),
	mImageUnselected( NULL ),
	mImageSelected( NULL ),
	mImageHoverSelected( NULL ),
	mImageHoverUnselected( NULL ),
	mImageDisabled( NULL ),
	mImageDisabledSelected( NULL ),
	mToggleState( FALSE ),
	mIsToggle( FALSE ),
	mScaleImage( TRUE ),
	mDropShadowedText( TRUE ),
	mBorderEnabled( FALSE ),
	mFlashing( FALSE ),
	mHAlign( LLFontGL::HCENTER ),
	mLeftHPad( LLBUTTON_H_PAD ),
	mRightHPad( LLBUTTON_H_PAD ),
	mHoverGlowStrength(0.15f),
	mCurGlowStrength(0.f),
	mNeedsHighlight(FALSE),
	mCommitOnReturn(TRUE),
	mImagep( NULL )
{
	mUnselectedLabel = name;
	mSelectedLabel = name;

	setImageUnselected(std::string("button_enabled_32x128.tga"));
	setImageSelected(std::string("button_enabled_selected_32x128.tga"));
	setImageDisabled(std::string("button_disabled_32x128.tga"));
	setImageDisabledSelected(std::string("button_disabled_32x128.tga"));

	mImageColor = LLUI::sColorsGroup->getColor( "ButtonImageColor" );
	mDisabledImageColor = LLUI::sColorsGroup->getColor( "ButtonImageColor" );

	init(click_callback, callback_data, NULL, control_name);
}


LLButton::LLButton(const std::string& name, const LLRect& rect, 
				   const std::string &unselected_image_name, 
				   const std::string &selected_image_name, 
				   const std::string& control_name,
				   void (*click_callback)(void*),
				   void *callback_data,
				   const LLFontGL *font,
				   const std::string& unselected_label, 
				   const std::string& selected_label )
:	LLUICtrl(name, rect, TRUE, NULL, NULL),
	mClickedCallback( click_callback ),
	mMouseDownCallback( NULL ),
	mMouseUpCallback( NULL ),
	mHeldDownCallback( NULL ),
	mGLFont( NULL ),
	mMouseDownFrame( 0 ),
	mHeldDownDelay( 0.5f ),			// seconds until held-down callback is called
	mHeldDownFrameDelay( 0 ),
	mImageUnselected( NULL ),
	mImageSelected( NULL ),
	mImageHoverSelected( NULL ),
	mImageHoverUnselected( NULL ),
	mImageDisabled( NULL ),
	mImageDisabledSelected( NULL ),
	mToggleState( FALSE ),
	mIsToggle( FALSE ),
	mScaleImage( TRUE ),
	mDropShadowedText( TRUE ),
	mBorderEnabled( FALSE ),
	mFlashing( FALSE ),
	mHAlign( LLFontGL::HCENTER ),
	mLeftHPad( LLBUTTON_H_PAD ), 
	mRightHPad( LLBUTTON_H_PAD ),
	mHoverGlowStrength(0.25f),
	mCurGlowStrength(0.f),
	mNeedsHighlight(FALSE),
	mCommitOnReturn(TRUE),
	mImagep( NULL )
{
	mUnselectedLabel = unselected_label;
	mSelectedLabel = selected_label;

	// by default, disabled color is same as enabled
	mImageColor = LLUI::sColorsGroup->getColor( "ButtonImageColor" );
	mDisabledImageColor = LLUI::sColorsGroup->getColor( "ButtonImageColor" );

	if( unselected_image_name != "" )
	{
		// user-specified image - don't use fixed borders unless requested
		setImageUnselected(unselected_image_name);
		setImageDisabled(unselected_image_name);
		
		mDisabledImageColor.mV[VALPHA] = 0.5f;
		mScaleImage = FALSE;
	}
	else
	{
		setImageUnselected(std::string("button_enabled_32x128.tga"));
		setImageDisabled(std::string("button_disabled_32x128.tga"));
	}

	if( selected_image_name != "" )
	{
		// user-specified image - don't use fixed borders unless requested
		setImageSelected(selected_image_name);
		setImageDisabledSelected(selected_image_name);

		mDisabledImageColor.mV[VALPHA] = 0.5f;
		mScaleImage = FALSE;
	}
	else
	{
		setImageSelected(std::string("button_enabled_selected_32x128.tga"));
		setImageDisabledSelected(std::string("button_disabled_32x128.tga"));
	}

	init(click_callback, callback_data, font, control_name);
}

void LLButton::init(void (*click_callback)(void*), void *callback_data, const LLFontGL* font, const std::string& control_name)
{
	mGLFont = ( font ? font : LLFontGL::sSansSerif);

	// Hack to make sure there is space for at least one character
	if (getRect().getWidth() - (mRightHPad + mLeftHPad) < mGLFont->getWidth(std::string(" ")))
	{
		// Use old defaults
		mLeftHPad = LLBUTTON_ORIG_H_PAD;
		mRightHPad = LLBUTTON_ORIG_H_PAD;
	}
	
	mCallbackUserData = callback_data;
	mMouseDownTimer.stop();

	setControlName(control_name, NULL);

	mUnselectedLabelColor = (			LLUI::sColorsGroup->getColor( "ButtonLabelColor" ) );
	mSelectedLabelColor = (			LLUI::sColorsGroup->getColor( "ButtonLabelSelectedColor" ) );
	mDisabledLabelColor = (			LLUI::sColorsGroup->getColor( "ButtonLabelDisabledColor" ) );
	mDisabledSelectedLabelColor = (	LLUI::sColorsGroup->getColor( "ButtonLabelSelectedDisabledColor" ) );
	mHighlightColor = (				LLUI::sColorsGroup->getColor( "ButtonUnselectedFgColor" ) );
	mUnselectedBgColor = (				LLUI::sColorsGroup->getColor( "ButtonUnselectedBgColor" ) );
	mSelectedBgColor = (				LLUI::sColorsGroup->getColor( "ButtonSelectedBgColor" ) );
	mFlashBgColor = (				LLUI::sColorsGroup->getColor( "ButtonFlashBgColor" ) );

	mImageOverlayAlignment = LLFontGL::HCENTER;
	mImageOverlayColor = LLColor4::white;
}

LLButton::~LLButton()
{
 	if( hasMouseCapture() )
	{
		gFocusMgr.setMouseCapture( NULL );
	}
}

// HACK: Committing a button is the same as instantly clicking it.
// virtual
void LLButton::onCommit()
{
	// WARNING: Sometimes clicking a button destroys the floater or
	// panel containing it.  Therefore we need to call mClickedCallback
	// LAST, otherwise this becomes deleted memory.
	LLUICtrl::onCommit();

	if (mMouseDownCallback)
	{
		(*mMouseDownCallback)(mCallbackUserData);
	}
	
	if (mMouseUpCallback)
	{
		(*mMouseUpCallback)(mCallbackUserData);
	}

	if (getSoundFlags() & MOUSE_DOWN)
	{
		make_ui_sound("UISndClick");
	}

	if (getSoundFlags() & MOUSE_UP)
	{
		make_ui_sound("UISndClickRelease");
	}

	if (mIsToggle)
	{
		toggleState();
	}

	// do this last, as it can result in destroying this button
	if (mClickedCallback)
	{
		(*mClickedCallback)( mCallbackUserData );
	}
}



BOOL LLButton::handleUnicodeCharHere(llwchar uni_char)
{
	BOOL handled = FALSE;
	if(' ' == uni_char 
		&& !gKeyboard->getKeyRepeated(' '))
	{
		if (mIsToggle)
		{
			toggleState();
		}

		if (mClickedCallback)
		{
			(*mClickedCallback)( mCallbackUserData );
		}
		handled = TRUE;		
	}
	return handled;	
}

BOOL LLButton::handleKeyHere(KEY key, MASK mask )
{
	BOOL handled = FALSE;
	if( mCommitOnReturn && KEY_RETURN == key && mask == MASK_NONE && !gKeyboard->getKeyRepeated(key))
	{
		if (mIsToggle)
		{
			toggleState();
		}

		handled = TRUE;

		if (mClickedCallback)
		{
			(*mClickedCallback)( mCallbackUserData );
		}
	}
	return handled;
}


BOOL LLButton::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Route future Mouse messages here preemptively.  (Release on mouse up.)
	gFocusMgr.setMouseCapture( this );

	if (hasTabStop() && !getIsChrome())
	{
		setFocus(TRUE);
	}

	if (mMouseDownCallback)
	{
		(*mMouseDownCallback)(mCallbackUserData);
	}

	mMouseDownTimer.start();
	mMouseDownFrame = (S32) LLFrameTimer::getFrameCount();
	
	if (getSoundFlags() & MOUSE_DOWN)
	{
		make_ui_sound("UISndClick");
	}

	return TRUE;
}


BOOL LLButton::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// We only handle the click if the click both started and ended within us
	if( hasMouseCapture() )
	{
		// Always release the mouse
		gFocusMgr.setMouseCapture( NULL );

		// Regardless of where mouseup occurs, handle callback
		if (mMouseUpCallback)
		{
			(*mMouseUpCallback)(mCallbackUserData);
		}

		mMouseDownTimer.stop();
		mMouseDownTimer.reset();

		// DO THIS AT THE VERY END to allow the button to be destroyed as a result of being clicked.
		// If mouseup in the widget, it's been clicked
		if (pointInView(x, y))
		{
			if (getSoundFlags() & MOUSE_UP)
			{
				make_ui_sound("UISndClickRelease");
			}

			if (mIsToggle)
			{
				toggleState();
			}

			if (mClickedCallback)
			{
				(*mClickedCallback)( mCallbackUserData );
			}			
		}
	}

	return TRUE;
}


BOOL LLButton::handleHover(S32 x, S32 y, MASK mask)
{
	LLMouseHandler* other_captor = gFocusMgr.getMouseCapture();
	mNeedsHighlight = other_captor == NULL || 
				other_captor == this ||
				// this following bit is to support modal dialogs
				(other_captor->isView() && hasAncestor((LLView*)other_captor));

	if (mMouseDownTimer.getStarted() && NULL != mHeldDownCallback)
	{
		F32 elapsed = getHeldDownTime();
		if( mHeldDownDelay <= elapsed && mHeldDownFrameDelay <= (S32)LLFrameTimer::getFrameCount() - mMouseDownFrame)
		{
			mHeldDownCallback( mCallbackUserData );		
		}
	}

	// We only handle the click if the click both started and ended within us
	getWindow()->setCursor(UI_CURSOR_ARROW);
	lldebugst(LLERR_USER_INPUT) << "hover handled by " << getName() << llendl;

	return TRUE;
}


// virtual
void LLButton::draw()
{
	BOOL flash = FALSE;
	if( mFlashing )
	{
		F32 elapsed = mFlashingTimer.getElapsedTimeF32();
		S32 flash_count = S32(elapsed * LLUI::sConfigGroup->getF32("ButtonFlashRate") * 2.f);
		// flash on or off?
		flash = (flash_count % 2 == 0) || flash_count > S32((F32)LLUI::sConfigGroup->getS32("ButtonFlashCount") * 2.f);
	}

	BOOL pressed_by_keyboard = FALSE;
	if (hasFocus())
	{
		pressed_by_keyboard = gKeyboard->getKeyDown(' ') || (mCommitOnReturn && gKeyboard->getKeyDown(KEY_RETURN));
	}

	// Unselected image assignments
	S32 local_mouse_x;
	S32 local_mouse_y;
	LLCoordWindow cursor_pos_window;
	getWindow()->getCursorPosition(&cursor_pos_window);
	LLCoordGL cursor_pos_gl;
	getWindow()->convertCoords(cursor_pos_window, &cursor_pos_gl);
	cursor_pos_gl.mX = llround((F32)cursor_pos_gl.mX / LLUI::sGLScaleFactor.mV[VX]);
	cursor_pos_gl.mY = llround((F32)cursor_pos_gl.mY / LLUI::sGLScaleFactor.mV[VY]);
	screenPointToLocal(cursor_pos_gl.mX, cursor_pos_gl.mY, &local_mouse_x, &local_mouse_y);

	BOOL pressed = pressed_by_keyboard 
					|| (hasMouseCapture() && pointInView(local_mouse_x, local_mouse_y)) 
					|| mToggleState;
	
	BOOL use_glow_effect = FALSE;
	LLColor4 glow_color = LLColor4::white;
	LLRender::eBlendType glow_type = LLRender::BT_ADD_WITH_ALPHA;
	if ( mNeedsHighlight )
	{
		if (pressed)
		{
			if (mImageHoverSelected)
			{
				mImagep = mImageHoverSelected;
			}
			else
			{
				mImagep = mImageSelected;
				use_glow_effect = TRUE;
			}
		}
		else
		{
			if (mImageHoverUnselected)
			{
				mImagep = mImageHoverUnselected;
			}
			else
			{
				mImagep = mImageUnselected;
				use_glow_effect = TRUE;
			}
		}
	}
	else if ( pressed )
	{
		mImagep = mImageSelected;
	}
	else
	{
		mImagep = mImageUnselected;
	}

	if (mFlashing)
	{
		use_glow_effect = TRUE;
		glow_type = LLRender::BT_ALPHA; // blend the glow
		if (mNeedsHighlight) // highlighted AND flashing
			glow_color = (glow_color*0.5f + mFlashBgColor*0.5f) % 2.0f; // average between flash and highlight colour, with sum of the opacity
		else
			glow_color = mFlashBgColor;
	}

	// Override if more data is available
	// HACK: Use gray checked state to mean either:
	//   enabled and tentative
	// or
	//   disabled but checked
	if (!mImageDisabledSelected.isNull() 
		&& 
			( (getEnabled() && getTentative()) 
			|| (!getEnabled() && pressed ) ) )
	{
		mImagep = mImageDisabledSelected;
	}
	else if (!mImageDisabled.isNull() 
		&& !getEnabled() 
		&& !pressed)
	{
		mImagep = mImageDisabled;
	}

	if (mNeedsHighlight && !mImagep)
	{
		use_glow_effect = TRUE;
	}

	// Figure out appropriate color for the text
	LLColor4 label_color;

	// label changes when button state changes, not when pressed
	if ( getEnabled() )
	{
		if ( mToggleState )
		{
			label_color = mSelectedLabelColor;
		}
		else
		{
			label_color = mUnselectedLabelColor;
		}
	}
	else
	{
		if ( mToggleState )
		{
			label_color = mDisabledSelectedLabelColor;
		}
		else
		{
			label_color = mDisabledLabelColor;
		}
	}

	// Unselected label assignments
	LLWString label;

	if( mToggleState )
	{
		if( getEnabled() || mDisabledSelectedLabel.empty() )
		{
			label = mSelectedLabel;
		}
		else
		{
			label = mDisabledSelectedLabel;
		}
	}
	else
	{
		if( getEnabled() || mDisabledLabel.empty() )
		{
			label = mUnselectedLabel;
		}
		else
		{
			label = mDisabledLabel;
		}
	}

	// overlay with keyboard focus border
	if (hasFocus())
	{
		F32 lerp_amt = gFocusMgr.getFocusFlashAmt();
		drawBorder(gFocusMgr.getFocusColor(), llround(lerp(1.f, 3.f, lerp_amt)));
	}
	
	if (use_glow_effect)
	{
		mCurGlowStrength = lerp(mCurGlowStrength,
					mFlashing ? (flash? 1.0 : 0.0)
					: mHoverGlowStrength,
					LLCriticalDamp::getInterpolant(0.05f));
	}
	else
	{
		mCurGlowStrength = lerp(mCurGlowStrength, 0.f, LLCriticalDamp::getInterpolant(0.05f));
	}

	// Draw button image, if available.
	// Otherwise draw basic rectangular button.
	if (mImagep.notNull())
	{
		if ( mScaleImage)
		{
			mImagep->draw(getLocalRect(), getEnabled() ? mImageColor : mDisabledImageColor  );
			if (mCurGlowStrength > 0.01f)
			{
				gGL.setSceneBlendType(glow_type);
				mImagep->drawSolid(0, 0, getRect().getWidth(), getRect().getHeight(), glow_color % mCurGlowStrength);
				gGL.setSceneBlendType(LLRender::BT_ALPHA);
			}
		}
		else
		{
			mImagep->draw(0, 0, getEnabled() ? mImageColor : mDisabledImageColor );
			if (mCurGlowStrength > 0.01f)
			{
				gGL.setSceneBlendType(glow_type);
				mImagep->drawSolid(0, 0, glow_color % mCurGlowStrength);
				gGL.setSceneBlendType(LLRender::BT_ALPHA);
			}
		}
	}
	else
	{
		// no image
		llwarns << "No image for button " << getName() << llendl;
		// draw it in pink so we can find it
		gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, LLColor4::pink1, FALSE);
	}

	// let overlay image and text play well together
	S32 text_left = mLeftHPad;
	S32 text_right = getRect().getWidth() - mRightHPad;
	S32 text_width = getRect().getWidth() - mLeftHPad - mRightHPad;

	// draw overlay image
	if (mImageOverlay.notNull())
	{
		// get max width and height (discard level 0)
		S32 overlay_width = mImageOverlay->getWidth();
		S32 overlay_height = mImageOverlay->getHeight();

		F32 scale_factor = llmin((F32)getRect().getWidth() / (F32)overlay_width, (F32)getRect().getHeight() / (F32)overlay_height, 1.f);
		overlay_width = llround((F32)overlay_width * scale_factor);
		overlay_height = llround((F32)overlay_height * scale_factor);

		S32 center_x = getLocalRect().getCenterX();
		S32 center_y = getLocalRect().getCenterY();

		//FUGLY HACK FOR "DEPRESSED" BUTTONS
		if (pressed)
		{
			center_y--;
			center_x++;
		}

		// fade out overlay images on disabled buttons
		LLColor4 overlay_color = mImageOverlayColor;
		if (!getEnabled())
		{
			overlay_color.mV[VALPHA] = 0.5f;
		}

		switch(mImageOverlayAlignment)
		{
		case LLFontGL::LEFT:
			text_left += overlay_width + 1;
			text_width -= overlay_width + 1;
			mImageOverlay->draw(
				mLeftHPad, 
				center_y - (overlay_height / 2), 
				overlay_width, 
				overlay_height, 
				overlay_color);
			break;
		case LLFontGL::HCENTER:
			mImageOverlay->draw(
				center_x - (overlay_width / 2), 
				center_y - (overlay_height / 2), 
				overlay_width, 
				overlay_height, 
				overlay_color);
			break;
		case LLFontGL::RIGHT:
			text_right -= overlay_width + 1;				
			text_width -= overlay_width + 1;
			mImageOverlay->draw(
				getRect().getWidth() - mRightHPad - overlay_width, 
				center_y - (overlay_height / 2), 
				overlay_width, 
				overlay_height, 
				overlay_color);
			break;
		default:
			// draw nothing
			break;
		}
	}

	// Draw label
	if( !label.empty() )
	{
		LLWStringUtil::trim(label);

		S32 x;
		switch( mHAlign )
		{
		case LLFontGL::RIGHT:
			x = text_right;
			break;
		case LLFontGL::HCENTER:
			x = getRect().getWidth() / 2;
			break;
		case LLFontGL::LEFT:
		default:
			x = text_left;
			break;
		}

		S32 y_offset = 2 + (getRect().getHeight() - 20)/2;
	
		if (pressed)
		{
			y_offset--;
			x++;
		}

		mGLFont->render(label, 0, (F32)x, (F32)(LLBUTTON_V_PAD + y_offset), 
			label_color,
			mHAlign, LLFontGL::BOTTOM,
			mDropShadowedText ? LLFontGL::DROP_SHADOW_SOFT : LLFontGL::NORMAL,
			U32_MAX, text_width,
			NULL, FALSE, FALSE);
	}

	if (sDebugRects	
		|| (LLView::sEditingUI && this == LLView::sEditingUIView))
	{
		drawDebugRect();
	}

	// reset hover status for next frame
	mNeedsHighlight = FALSE;
}

void LLButton::drawBorder(const LLColor4& color, S32 size)
{
	if (mScaleImage)
	{
		mImagep->drawBorder(getLocalRect(), color, size);
	}
	else
	{
		mImagep->drawBorder(0, 0, color, size);
	}
}

void LLButton::setClickedCallback(void (*cb)(void*), void* userdata)
{
	mClickedCallback = cb;
	if (userdata)
	{
		mCallbackUserData = userdata;
	}
}


void LLButton::setToggleState(BOOL b)
{
	if( b != mToggleState )
	{
		setControlValue(b); // will fire LLControlVariable callbacks (if any)
		mToggleState = b; // may or may not be redundant
	}
}

void LLButton::setFlashing( BOOL b )	
{ 
	if (b != mFlashing)
	{
		mFlashing = b; 
		mFlashingTimer.reset();
	}
}


BOOL LLButton::toggleState()			
{ 
	setToggleState( !mToggleState ); 
	return mToggleState; 
}

void LLButton::setValue(const LLSD& value )
{
	mToggleState = value.asBoolean();
}

LLSD LLButton::getValue() const
{
	return mToggleState == TRUE;
}

void LLButton::setLabel( const LLStringExplicit& label )
{
	setLabelUnselected(label);
	setLabelSelected(label);
}

//virtual
BOOL LLButton::setLabelArg( const std::string& key, const LLStringExplicit& text )
{
	mUnselectedLabel.setArg(key, text);
	mSelectedLabel.setArg(key, text);
	return TRUE;
}

void LLButton::setLabelUnselected( const LLStringExplicit& label )
{
	mUnselectedLabel = label;
}

void LLButton::setLabelSelected( const LLStringExplicit& label )
{
	mSelectedLabel = label;
}

void LLButton::setDisabledLabel( const LLStringExplicit& label )
{
	mDisabledLabel = label;
}

void LLButton::setDisabledSelectedLabel( const LLStringExplicit& label )
{
	mDisabledSelectedLabel = label;
}

void LLButton::setImageUnselected(LLPointer<LLUIImage> image)
{
	mImageUnselected = image;
}

void LLButton::setImages( const std::string &image_name, const std::string &selected_name )
{
	setImageUnselected(image_name);
	setImageSelected(selected_name);

}

void LLButton::setImageSelected(LLPointer<LLUIImage> image)
{
	mImageSelected = image;
}

void LLButton::setImageColor(const LLColor4& c)		
{ 
	mImageColor = c; 
}

void LLButton::setColor(const LLColor4& color)
{
	setImageColor(color);
}


void LLButton::setImageDisabled(LLPointer<LLUIImage> image)
{
	mImageDisabled = image;
	mDisabledImageColor = mImageColor;
	mDisabledImageColor.mV[VALPHA] *= 0.5f;
}

void LLButton::setImageDisabledSelected(LLPointer<LLUIImage> image)
{
	mImageDisabledSelected = image;
	mDisabledImageColor = mImageColor;
	mDisabledImageColor.mV[VALPHA] *= 0.5f;
}

void LLButton::setDisabledImages( const std::string &image_name, const std::string &selected_name, const LLColor4& c )
{
	setImageDisabled(image_name);
	setImageDisabledSelected(selected_name);
	mDisabledImageColor = c;
}

void LLButton::setImageHoverSelected(LLPointer<LLUIImage> image)
{
	mImageHoverSelected = image;
}

void LLButton::setDisabledImages( const std::string &image_name, const std::string &selected_name)
{
	LLColor4 clr = mImageColor;
	clr.mV[VALPHA] *= .5f;
	setDisabledImages( image_name, selected_name, clr );
}

void LLButton::setImageHoverUnselected(LLPointer<LLUIImage> image)
{
	mImageHoverUnselected = image;
}

void LLButton::setHoverImages( const std::string& image_name, const std::string& selected_name )
{
	setImageHoverUnselected(image_name);
	setImageHoverSelected(selected_name);
}

void LLButton::setImageOverlay(const std::string& image_name, LLFontGL::HAlign alignment, const LLColor4& color)
{
	if (image_name.empty())
	{
		mImageOverlay = NULL;
	}
	else
	{
		mImageOverlay = LLUI::getUIImage(image_name);
		mImageOverlayAlignment = alignment;
		mImageOverlayColor = color;
	}
}


void LLButton::onMouseCaptureLost()
{
	mMouseDownTimer.stop();
	mMouseDownTimer.reset();
}

//-------------------------------------------------------------------------
// Utilities
//-------------------------------------------------------------------------
S32 round_up(S32 grid, S32 value)
{
	S32 mod = value % grid;

	if (mod > 0)
	{
		// not even multiple
		return value + (grid - mod);
	}
	else
	{
		return value;
	}
}

void			LLButton::setImageUnselected(const std::string &image_name)
{	
	setImageUnselected(LLUI::getUIImage(image_name));
	mImageUnselectedName = image_name;
}

void			LLButton::setImageSelected(const std::string &image_name)
{	
	setImageSelected(LLUI::getUIImage(image_name));
	mImageSelectedName = image_name;
}

void			LLButton::setImageHoverSelected(const std::string &image_name)
{
	setImageHoverSelected(LLUI::getUIImage(image_name));
	mImageHoverSelectedName = image_name;
}

void			LLButton::setImageHoverUnselected(const std::string &image_name)
{
	setImageHoverUnselected(LLUI::getUIImage(image_name));
	mImageHoverUnselectedName = image_name;
}

void			LLButton::setImageDisabled(const std::string &image_name)
{
	setImageDisabled(LLUI::getUIImage(image_name));
	mImageDisabledName = image_name;
}

void			LLButton::setImageDisabledSelected(const std::string &image_name)
{
	setImageDisabledSelected(LLUI::getUIImage(image_name));
	mImageDisabledSelectedName = image_name;
}

void LLButton::addImageAttributeToXML(LLXMLNodePtr node, 
									  const std::string& image_name,
									  const LLUUID&	image_id,
									  const std::string& xml_tag_name) const
{
	if( !image_name.empty() )
	{
		node->createChild(xml_tag_name.c_str(), TRUE)->setStringValue(image_name);
	}
	else if( image_id != LLUUID::null )
	{
		node->createChild((xml_tag_name + "_id").c_str(), TRUE)->setUUIDValue(image_id);
	}
}

// virtual
LLXMLNodePtr LLButton::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->createChild("label", TRUE)->setStringValue(getLabelUnselected());
	node->createChild("label_selected", TRUE)->setStringValue(getLabelSelected());
	node->createChild("font", TRUE)->setStringValue(LLFontGL::nameFromFont(mGLFont));
	node->createChild("halign", TRUE)->setStringValue(LLFontGL::nameFromHAlign(mHAlign));

	addImageAttributeToXML(node,mImageUnselectedName,mImageUnselectedID,std::string("image_unselected"));
	addImageAttributeToXML(node,mImageSelectedName,mImageSelectedID,std::string("image_selected"));
	addImageAttributeToXML(node,mImageHoverSelectedName,mImageHoverSelectedID,std::string("image_hover_selected"));
	addImageAttributeToXML(node,mImageHoverUnselectedName,mImageHoverUnselectedID,std::string("image_hover_unselected"));
	addImageAttributeToXML(node,mImageDisabledName,mImageDisabledID,std::string("image_disabled"));
	addImageAttributeToXML(node,mImageDisabledSelectedName,mImageDisabledSelectedID,std::string("image_disabled_selected"));

	node->createChild("scale_image", TRUE)->setBoolValue(mScaleImage);

	return node;
}

void clicked_help(void* data)
{
	LLButton* self = (LLButton*)data;
	if (!self) return;
	
	if (!LLUI::sHtmlHelp)
	{
		return;
	}
	
	LLUI::sHtmlHelp->show(self->getHelpURL());
}

// static
LLView* LLButton::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string name("button");
	node->getAttributeString("name", name);

	std::string label = name;
	node->getAttributeString("label", label);

	std::string label_selected = label;
	node->getAttributeString("label_selected", label_selected);

	LLFontGL* font = selectFont(node);

	std::string	image_unselected;
	if (node->hasAttribute("image_unselected")) node->getAttributeString("image_unselected",image_unselected);
	
	std::string	image_selected;
	if (node->hasAttribute("image_selected")) node->getAttributeString("image_selected",image_selected);
	
	std::string	image_hover_selected;
	if (node->hasAttribute("image_hover_selected")) node->getAttributeString("image_hover_selected",image_hover_selected);
	
	std::string	image_hover_unselected;
	if (node->hasAttribute("image_hover_unselected")) node->getAttributeString("image_hover_unselected",image_hover_unselected);
	
	std::string	image_disabled_selected;
	if (node->hasAttribute("image_disabled_selected")) node->getAttributeString("image_disabled_selected",image_disabled_selected);
	
	std::string	image_disabled;
	if (node->hasAttribute("image_disabled")) node->getAttributeString("image_disabled",image_disabled);

	std::string	image_overlay;
	node->getAttributeString("image_overlay", image_overlay);

	LLFontGL::HAlign image_overlay_alignment = LLFontGL::HCENTER;
	std::string image_overlay_alignment_string;
	if (node->hasAttribute("image_overlay_alignment"))
	{
		node->getAttributeString("image_overlay_alignment", image_overlay_alignment_string);
		image_overlay_alignment = LLFontGL::hAlignFromName(image_overlay_alignment_string);
	}


	LLButton *button = new LLButton(name, 
			LLRect(),
			image_unselected,
			image_selected,
			LLStringUtil::null, 
			NULL, 
			parent,
			font,
			label,
			label_selected);

	node->getAttributeS32("pad_right", button->mRightHPad);
	node->getAttributeS32("pad_left", button->mLeftHPad);

	BOOL is_toggle = button->getIsToggle();
	node->getAttributeBOOL("toggle", is_toggle);
	button->setIsToggle(is_toggle);

	if(image_hover_selected != LLStringUtil::null) button->setImageHoverSelected(image_hover_selected);
	
	if(image_hover_unselected != LLStringUtil::null) button->setImageHoverUnselected(image_hover_unselected);
	
	if(image_disabled_selected != LLStringUtil::null) button->setImageDisabledSelected(image_disabled_selected );
	
	if(image_disabled != LLStringUtil::null) button->setImageDisabled(image_disabled);
	
	if(image_overlay != LLStringUtil::null) button->setImageOverlay(image_overlay, image_overlay_alignment);

	if (node->hasAttribute("halign"))
	{
		LLFontGL::HAlign halign = selectFontHAlign(node);
		button->setHAlign(halign);
	}

	if (node->hasAttribute("scale_image"))
	{
		BOOL	needsScale = FALSE;
		node->getAttributeBOOL("scale_image",needsScale);
		button->setScaleImage( needsScale );
	}

	if(label.empty())
	{
		button->setLabelUnselected(node->getTextContents());
	}
	if (label_selected.empty())
	{
		button->setLabelSelected(node->getTextContents());
	}
		
	if (node->hasAttribute("help_url")) 
	{
		std::string	help_url;
		node->getAttributeString("help_url",help_url);
		button->setHelpURLCallback(help_url);
	}

	button->initFromXML(node, parent);
	
	return button;
}

void LLButton::setHelpURLCallback(const std::string &help_url)
{
	mHelpURL = help_url;
	setClickedCallback(clicked_help,this);
}
