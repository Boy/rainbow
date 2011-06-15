/** 
 * @file llnotify.cpp
 * @brief Non-blocking notification that doesn't take keyboard focus.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llnotify.h"

#include "llchat.h"
#include "llfocusmgr.h"
#include "llrender.h"

#include "llbutton.h"
#include "llfocusmgr.h"
#include "llglheaders.h"
#include "lliconctrl.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lluiconstants.h"
#include "llui.h"
#include "llxmlnode.h"
#include "llalertdialog.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewerimagelist.h"
#include "llfloaterchat.h"	// for add_chat_history()
#include "lloverlaybar.h" // for gOverlayBar
#include "lluictrlfactory.h"

//MK
#include "llviewerregion.h"
#include "llagent.h"
//mk

// Globals
LLNotifyBoxView* gNotifyBoxView = NULL;

const F32 ANIMATION_TIME = 0.333f;

// statics
S32 LLNotifyBox::sNotifyBoxCount = 0;
const LLFontGL* LLNotifyBox::sFont = NULL;
const LLFontGL* LLNotifyBox::sFontSmall = NULL;
std::map<std::string, LLNotifyBox*> LLNotifyBox::sOpenUniqueNotifyBoxes;
LLPointer<LLNotifyBoxTemplate> LLNotifyBox::sDefaultTemplate;


LLNotifyBox::template_map_t LLNotifyBox::sNotifyTemplates;

LLNotifyBox::LLNotifyBehavior::LLNotifyBehavior(notify_callback_t callback, void* data) :
	mCallback(callback),
	mData(data)
{
}

//---------------------------------------------------------------------------
// LLNotifyBox
//---------------------------------------------------------------------------

//static
LLNotifyBox* LLNotifyBox::showXml( const std::string& xml_desc, notify_callback_t callback, void *user_data)
{
	return showXml(xml_desc, LLStringUtil::format_map_t(), callback, user_data);
}


//static
LLNotifyBox* LLNotifyBox::showXml( const std::string& xml_desc, const LLStringUtil::format_map_t& args, BOOL is_caution,
						   notify_callback_t callback, void *user_data)
{
	// for script permission prompts
	LLPointer<LLNotifyBoxTemplate> xml_template = getTemplate(xml_desc);
	LLNotifyBox* notify = findExistingNotify(xml_template, args);
	if (notify)
	{
		delete notify->mBehavior;
		notify->mBehavior = new LLNotifyBehavior(callback, user_data);
	}
	else
	{
		notify = new LLNotifyBox(xml_template, args, callback, user_data, is_caution);
		gNotifyBoxView->addChildAtEnd(notify);
		notify->moveToBack();
	}
	return notify;
}

//static
LLNotifyBox* LLNotifyBox::showXml( const std::string& xml_desc, const LLStringUtil::format_map_t& args,
						   notify_callback_t callback, void *user_data)
{
	LLPointer<LLNotifyBoxTemplate> xml_template = getTemplate(xml_desc);
	LLNotifyBox* notify = findExistingNotify(xml_template, args);
	if (notify)
	{
		delete notify->mBehavior;
		notify->mBehavior = new LLNotifyBehavior(callback, user_data);
	}
	else
	{
		notify = new LLNotifyBox(xml_template, args, callback, user_data);
		gNotifyBoxView->addChildAtEnd(notify);
		notify->moveToBack();
	}
	return notify;
}

//static
LLNotifyBox* LLNotifyBox::showXml( const std::string& xml_desc, const LLStringUtil::format_map_t& args,
						   notify_callback_t callback, void *user_data,
						   const option_list_t& options,
						   BOOL layout_script_dialog)
{
	LLPointer<LLNotifyBoxTemplate> xml_template = getTemplate(xml_desc);
	LLNotifyBox* notify = findExistingNotify(xml_template, args);
	if (notify)
	{
		delete notify->mBehavior;
		notify->mBehavior = new LLNotifyBehavior(callback, user_data);
	}
	else
	{
		notify = new LLNotifyBox(xml_template, args, callback, user_data, FALSE, options, layout_script_dialog);
		gNotifyBoxView->addChild(notify);
	}
	return notify;
}

//static
LLNotifyBox* LLNotifyBox::findExistingNotify(LLPointer<LLNotifyBoxTemplate> notify_template, const LLStringUtil::format_map_t &args)
{
	if(notify_template->mUnique)
	{
		std::string message = notify_template->mMessage;
		format(message, args);
		unique_map_t::iterator found_it = sOpenUniqueNotifyBoxes.find(notify_template->mLabel + message);
		if (found_it != sOpenUniqueNotifyBoxes.end())
		{
			return found_it->second;
		}
	}
	return NULL;
}

//static
void LLNotifyBox::cleanup()
{
	sDefaultTemplate = NULL;
}

//---------------------------------------------------------------------------

LLNotifyBox::LLNotifyBox(LLPointer<LLNotifyBoxTemplate> xml_template, const LLStringUtil::format_map_t& args,
						 notify_callback_t callback, void* user_data, BOOL is_caution,
						 const option_list_t& extra_options,
						 BOOL layout_script_dialog)
	: LLPanel(xml_template->mLabel, LLRect(), BORDER_NO),
	  LLEventTimer(xml_template->mDuration),
	  mIsTip(FALSE),
	  mAnimating(TRUE),
	  mUnique(xml_template->mUnique),
	  mNextBtn(NULL),
	  mBehavior(new LLNotifyBehavior(callback, user_data)),
	  mNumOptions(0),
	  mDefaultOption(0)
{
	// clicking on a button does not steal current focus
	setIsChrome(TRUE);

	// class init
	if (!sFont)
	{
		sFont = LLFontGL::sSansSerif;
		sFontSmall = LLFontGL::sSansSerifSmall;
	}

	// setup paramaters
	
	mMessage = xml_template->mMessage;
	format(mMessage, args);

	// use name + formatted text as unique key
	if (mUnique)
	{
		sOpenUniqueNotifyBoxes[xml_template->mLabel + mMessage] = this;
	}

	option_list_t options = xml_template->mOptions;
	options.insert(options.end(), extra_options.begin(), extra_options.end());

//MK
	if (gRRenabled && gAgent.mRRInterface.mContainsShowloc)
	{
		// hide every occurrence of the Parcel name if the location restriction is active
		mMessage = gAgent.mRRInterface.stringReplace (mMessage, gAgent.mRRInterface.getParcelName(), "(Parcel hidden)");
		// hide every occurrence of the Region name if the location restriction is active
		mMessage = gAgent.mRRInterface.stringReplace (mMessage, gAgent.getRegion()->getName(), "(Region hidden)");
	}
	
	if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
	{
		mMessage = gAgent.mRRInterface.getCensoredMessage(mMessage);
	}
//mk

	// initialize

	mIsTip = xml_template->mIsTip;
	setFocusRoot(!mIsTip);

	// caution flag can be set explicitly by specifying it in the
	// call to the c'tor, or it can be set implicitly if the
	// notify xml template specifies that it is a caution
	//
	// tip-style notification handle 'caution' differently -
	// they display the tip in a different color
	mIsCaution = (xml_template->mIsCaution || is_caution);

	// Don't animate if behind other windows
	if( gNotifyBoxView->getChildCount() > 0 )
		mAnimating = FALSE;
	else
		mAnimating = TRUE;

	mNumOptions = options.size();
	mDefaultOption = xml_template->mDefaultOption;
		  
	LLRect rect = mIsTip ? getNotifyTipRect(mMessage)
		   		  		 : getNotifyRect(mNumOptions, layout_script_dialog, mIsCaution);
	setRect(rect);
	setFollows(mIsTip ? (FOLLOWS_BOTTOM|FOLLOWS_RIGHT) : (FOLLOWS_TOP|FOLLOWS_RIGHT));
	setBackgroundVisible(FALSE);
	setBackgroundOpaque(TRUE);

	LLIconCtrl* icon;
	LLTextEditor* text;

	const S32 TOP = getRect().getHeight() - (mIsTip ? (S32)sFont->getLineHeight() : 32);
	const S32 BOTTOM = (S32)sFont->getLineHeight();
	S32 x = HPAD + HPAD;
	S32 y = TOP;

	if (mIsTip)
	{
		// use the tip notification icon
		icon = new LLIconCtrl(std::string("icon"), LLRect(x, y, x+32, TOP-32), std::string("notify_tip_icon.tga"));
	}
	else if (mIsCaution)
	{
		// use the caution notification icon
		icon = new LLIconCtrl(std::string("icon"), LLRect(x, y, x+32, TOP-32), std::string("notify_caution_icon.tga"));
	}
	else
	{
		// use the default notification icon
		icon = new LLIconCtrl(std::string("icon"), LLRect(x, y, x+32, TOP-32), std::string("notify_box_icon.tga"));
	}

	icon->setMouseOpaque(FALSE);
	addChild(icon);

	x += HPAD + HPAD + 32;

	// add a caution textbox at the top of a caution notification
	LLTextBox* caution_box = NULL;
	if (mIsCaution && !mIsTip)
	{
		S32 caution_height = ((S32)sFont->getLineHeight() * 2) + VPAD;
		caution_box = new LLTextBox(
			std::string("caution_box"), 
			LLRect(x, y, getRect().getWidth() - 2, caution_height), 
			LLStringUtil::null, 
			sFont, 
			FALSE);

		caution_box->setFontStyle(LLFontGL::BOLD);
		caution_box->setColor(gColors.getColor("NotifyCautionWarnColor"));
		caution_box->setBackgroundColor(gColors.getColor("NotifyCautionBoxColor"));
		caution_box->setBorderVisible(FALSE);
		caution_box->setWrappedText(LLNotifyBox::getTemplateMessage("ScriptQuestionCautionWarn"));
		
		addChild(caution_box);

		// adjust the vertical position of the next control so that 
		// it appears below the caution textbox
		y = y - caution_height;
	}

	const S32 BOTTOM_PAD = VPAD * 3;
	const S32 BTN_TOP = BOTTOM_PAD + (((mNumOptions-1+2)/3)) * (BTN_HEIGHT+VPAD);

	// Tokenization on \n is handled by LLTextBox

	const S32 MAX_LENGTH = 512 + 20 + 
		DB_FIRST_NAME_BUF_SIZE + 
		DB_LAST_NAME_BUF_SIZE +
		DB_INV_ITEM_NAME_BUF_SIZE;  // For script dialogs: add space for title.

	text = new LLTextEditor(std::string("box"),
							LLRect(x, y, getRect().getWidth()-2, mIsTip ? BOTTOM : BTN_TOP+16),
							MAX_LENGTH,
							mMessage,
							sFont,
							FALSE);
	text->setWordWrap(TRUE);
	text->setTabStop(FALSE);
	text->setMouseOpaque(FALSE);
	text->setBorderVisible(FALSE);
	text->setTakesNonScrollClicks(FALSE);
	text->setHideScrollbarForShortDocs(TRUE);
	text->setReadOnlyBgColor ( LLColor4::transparent ); // the background color of the box is manually 
	                                                    // rendered under the text box, therefore we want 
														// the actual text box to be transparent
	text->setReadOnlyFgColor ( gColors.getColor("NotifyTextColor") );
	text->setEnabled(FALSE); // makes it read-only
	text->setTabStop(FALSE); // can't tab to it (may be a problem for scrolling via keyboard)
	addChild(text);

	if (mIsTip)
	{
		if (!gSavedSettings.getBOOL("HideNotificationsInChat")) {
			// TODO: Make a separate archive for these.
			LLChat chat(mMessage);
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			LLFloaterChat::getInstance(LLSD())->addChatHistory(chat);
		}
	}
	else
	{
		LLButton* btn;
		btn = new LLButton(std::string("next"),
						   LLRect(getRect().getWidth()-26, BOTTOM_PAD + 20, getRect().getWidth()-2, BOTTOM_PAD),
						   std::string("notify_next.png"),
						   std::string("notify_next.png"),
						   LLStringUtil::null,
						   onClickNext,
						   this,
						   sFont);
		btn->setScaleImage(TRUE);
		btn->setToolTip(std::string("Next")); // *TODO: Translate
		addChild(btn);
		mNextBtn = btn;

		// make caution notification buttons slightly narrower
		// so that 3 of them can fit without overlapping the "next" button
		S32 btn_width = mIsCaution? 84 : 90;
		LLRect btn_rect;

		for (S32 i = 0; i < mNumOptions; i++)
		{
			S32 index = i;
			S32 btn_height= BTN_HEIGHT;
			const LLFontGL* font = sFont;
			S32 ignore_pad = 0;

			if (layout_script_dialog)
			{
				// Add two "blank" option spaces, before the "Ignore" button
				index = i + 2;
				if (i == 0)
				{
					// Ignore button is smaller, less wide
					btn_height = BTN_HEIGHT_SMALL;
					font = sFontSmall;
					ignore_pad = 10;
				}
			}

			btn_rect.setOriginAndSize(x + (index % 3) * (btn_width+HPAD+HPAD) + ignore_pad,
									  BOTTOM_PAD + (index / 3) * (BTN_HEIGHT+VPAD),
									  btn_width - 2*ignore_pad,
									  btn_height);

			InstanceAndS32* userdata = new InstanceAndS32;
			userdata->mSelf = this;
			userdata->mButton = i;

			mBtnCallbackData.push_back(userdata);

			btn = new LLButton(options[i], btn_rect, LLStringUtil::null, onClickButton, userdata);
			btn->setFont(font);

			if (mIsCaution)
			{
				btn->setImageColor(LLUI::sColorsGroup->getColor("ButtonCautionImageColor"));
				btn->setDisabledImageColor(LLUI::sColorsGroup->getColor("ButtonCautionImageColor"));
			}

			addChild(btn, -1);

			if (i == mDefaultOption)
			{
				setDefaultBtn(btn);
			}
		}
		
		sNotifyBoxCount++;

		// If this is the only notify box, don't show the next button
		if (sNotifyBoxCount == 1
			&& mNextBtn)
		{
			mNextBtn->setVisible(FALSE);
		}
	}
}

// virtual
LLNotifyBox::~LLNotifyBox()
{
	delete mBehavior;
	mBehavior = NULL;

	std::for_each(mBtnCallbackData.begin(), mBtnCallbackData.end(), DeletePointer());

	if (mUnique)
	{
		sOpenUniqueNotifyBoxes.erase(getName() + mMessage);
	}
}

// virtual
BOOL LLNotifyBox::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (mIsTip)
	{
		if (mBehavior->mCallback)
		{
			mBehavior->mCallback(0, mBehavior->mData);
			mBehavior->mCallback = NULL; // Notification callbacks only expect to be called once ever
		}
		close();
		return TRUE;
	}

	setFocus(TRUE);

	return LLPanel::handleMouseUp(x, y, mask);
}

// virtual
BOOL LLNotifyBox::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (!mIsTip)
	{
		moveToBack(true);
		return TRUE;
	}

	return LLPanel::handleRightMouseDown(x, y, mask);
}


// virtual
void LLNotifyBox::draw()
{
	// If we are teleporting, stop the timer and restart it when the teleporting completes
	if (gTeleportDisplay)
	{
		mEventTimer.stop();
	}
	else if (!mEventTimer.getStarted())
	{
		mEventTimer.start();
	}
		
	F32 display_time = mAnimateTimer.getElapsedTimeF32();

	if (mAnimating && display_time < ANIMATION_TIME)
	{
		glMatrixMode(GL_MODELVIEW);
		LLUI::pushMatrix();

		S32 height = getRect().getHeight();
		F32 fraction = display_time / ANIMATION_TIME;
		F32 voffset = (1.f - fraction) * height;
		if (mIsTip) voffset *= -1.f;
		LLUI::translate(0.f, voffset, 0.f);
		
		drawBackground();

		LLPanel::draw();

		LLUI::popMatrix();
	}
	else
	{
		if(mAnimating)
		{
			mAnimating = FALSE;
			if(!mIsTip) 
			{
				// hide everyone behind me once I'm done animating
				gNotifyBoxView->showOnly(this);
			}
		}
		drawBackground();
		LLPanel::draw();
	}
}

void LLNotifyBox::drawBackground() const
{
	LLUIImagePtr imagep = LLUI::getUIImage("rounded_square.tga");
	if (imagep)
	{
		gGL.getTexUnit(0)->bind(imagep->getImage());
		// set proper background color depending on whether notify box is a caution or not
		LLColor4 color = mIsCaution? gColors.getColor("NotifyCautionBoxColor") : gColors.getColor("NotifyBoxColor");
		if(gFocusMgr.childHasKeyboardFocus( this ))
		{
			const S32 focus_width = 2;
			color = gColors.getColor("FloaterFocusBorderColor");
			gGL.color4fv(color.mV);
			gl_segmented_rect_2d_tex(-focus_width, getRect().getHeight() + focus_width, 
									getRect().getWidth() + focus_width, -focus_width,
									imagep->getTextureWidth(), imagep->getTextureHeight(),
									16, mIsTip ? ROUNDED_RECT_TOP : ROUNDED_RECT_BOTTOM);
			color = gColors.getColor("ColorDropShadow");
			gGL.color4fv(color.mV);
			gl_segmented_rect_2d_tex(0, getRect().getHeight(), getRect().getWidth(), 0, imagep->getTextureWidth(), imagep->getTextureHeight(), 16, mIsTip ? ROUNDED_RECT_TOP : ROUNDED_RECT_BOTTOM);

			if( mIsCaution )
				color = gColors.getColor("NotifyCautionBoxColor");
			else
				color = gColors.getColor("NotifyBoxColor");

			gGL.color4fv(color.mV);
			gl_segmented_rect_2d_tex(1, getRect().getHeight()-1, getRect().getWidth()-1, 1, imagep->getTextureWidth(), imagep->getTextureHeight(), 16, mIsTip ? ROUNDED_RECT_TOP : ROUNDED_RECT_BOTTOM);
		}
		else
		{
			gGL.color4fv(color.mV);
			gl_segmented_rect_2d_tex(0, getRect().getHeight(), getRect().getWidth(), 0, imagep->getTextureWidth(), imagep->getTextureHeight(), 16, mIsTip ? ROUNDED_RECT_TOP : ROUNDED_RECT_BOTTOM);
		}
	}
}


void LLNotifyBox::close()
{
	BOOL isTipTmp = mIsTip;

	if (!mIsTip)
	{
		sNotifyBoxCount--;
	}

	die();
	if(!isTipTmp)
	{
		LLNotifyBox * front = gNotifyBoxView->getFirstNontipBox();
		if(front)
		{
			gNotifyBoxView->showOnly(front);
			// we're assuming that close is only called by user action (for non-tips),
			// so we then give focus to the next close button
			if (front->getDefaultButton())
			{
				front->getDefaultButton()->setFocus(TRUE);
			}
			gFocusMgr.triggerFocusFlash(); // TODO it's ugly to call this here
		}
	}
}

void LLNotifyBox::format(std::string& msg, const LLStringUtil::format_map_t& args)
{
	// XUI:translate!
	LLStringUtil::format_map_t targs = args;
	targs["[SECOND_LIFE]"] = "Rainbow Viewer";
	LLStringUtil::format(msg, targs);
}


/*virtual*/
BOOL LLNotifyBox::tick()
{
	if (mIsTip)
	{
		close();
	}
	return FALSE;
}

void LLNotifyBox::setVisible(BOOL visible)
{
	// properly set the status of the next button
	if(visible && !mIsTip)
	{
		mNextBtn->setVisible(sNotifyBoxCount > 1);
		mNextBtn->setEnabled(sNotifyBoxCount > 1);
	}
	LLPanel::setVisible(visible);
}

void LLNotifyBox::moveToBack(bool getfocus)
{
	// Move this dialog to the back.
	gNotifyBoxView->sendChildToBack(this);
	if(!mIsTip && mNextBtn)
	{
		mNextBtn->setVisible(FALSE);

		// And enable the next button on the frontmost one, if there is one
		if (gNotifyBoxView->getChildCount() > 0)
		{
			LLNotifyBox* front = gNotifyBoxView->getFirstNontipBox();
			if (front)
			{
				gNotifyBoxView->showOnly(front);
				if (getfocus)
				{
					// if are called from a user interaction
					// we give focus to the next next button
					if (front->mNextBtn != NULL)
					{
						front->mNextBtn->setFocus(TRUE);
					}
					gFocusMgr.triggerFocusFlash(); // TODO: it's ugly to call this here
				}
			}
		}
	}
}


// static
LLRect LLNotifyBox::getNotifyRect(S32 num_options, BOOL layout_script_dialog, BOOL is_caution)
{
	S32 notify_height = gSavedSettings.getS32("NotifyBoxHeight");
	if (is_caution)
	{
		// make caution-style dialog taller to accomodate extra text,
		// as well as causing the accept/decline buttons to be drawn
		// in a different position, to help prevent "quick-click-through"
		// of many permissions prompts
		notify_height = gSavedSettings.getS32("PermissionsCautionNotifyBoxHeight");
	}
	const S32 NOTIFY_WIDTH = gSavedSettings.getS32("NotifyBoxWidth");

	const S32 TOP = gNotifyBoxView->getRect().getHeight();
	const S32 RIGHT = gNotifyBoxView->getRect().getWidth();
	const S32 LEFT = RIGHT - NOTIFY_WIDTH;

	if (num_options < 1)
	{
		num_options = 1;
	}

	// Add two "blank" option spaces.
	if (layout_script_dialog)
	{
		num_options += 2;
	}

	S32 additional_lines = (num_options-1) / 3;

	notify_height += additional_lines * (BTN_HEIGHT + VPAD);

	return LLRect(LEFT, TOP, RIGHT, TOP-notify_height);
}

// static
LLRect LLNotifyBox::getNotifyTipRect(const std::string &utf8message)
{
	S32 line_count = 1;
	LLWString message = utf8str_to_wstring(utf8message);
	S32 message_len = message.length();

	const S32 NOTIFY_WIDTH = gSavedSettings.getS32("NotifyBoxWidth");
	// Make room for the icon area.
	const S32 text_area_width = NOTIFY_WIDTH - HPAD * 4 - 32;

	const llwchar* wchars = message.c_str();
	const llwchar* start = wchars;
	const llwchar* end;
	S32 total_drawn = 0;
	BOOL done = FALSE;

	do
	{
		line_count++;

		for (end=start; *end != 0 && *end != '\n'; end++)
			;

		if( *end == 0 )
		{
			end = wchars + message_len;
			done = TRUE;
		}
		
		S32 remaining = end - start;
		while( remaining )
		{
			S32 drawn = sFont->maxDrawableChars( start, (F32)text_area_width, remaining, TRUE );

			if( 0 == drawn )
			{
				drawn = 1;  // Draw at least one character, even if it doesn't all fit. (avoids an infinite loop)
			}

			total_drawn += drawn; 
			start += drawn;
			remaining -= drawn;
			
			if( total_drawn < message_len )
			{
				if( (wchars[ total_drawn ] != '\n') )
				{
					// wrap because line was too long
					line_count++;
				}
			}
			else
			{
				done = TRUE;
			}
		}

		total_drawn++;	// for '\n'
		end++;
		start = end;
	} while( !done );

	const S32 MIN_NOTIFY_HEIGHT = 72;
	const S32 MAX_NOTIFY_HEIGHT = 600;
	S32 notify_height = llceil((F32) (line_count+1) * sFont->getLineHeight());
	if(gOverlayBar)
	{
		notify_height += gOverlayBar->getBoundingRect().mTop;
	}
	else
	{
		// *FIX: this is derived from the padding caused by the
		// rounded rects, shouldn't be a const here.
		notify_height += 10;  
	}
	notify_height += VPAD;
	notify_height = llclamp(notify_height, MIN_NOTIFY_HEIGHT, MAX_NOTIFY_HEIGHT);

	const S32 RIGHT = gNotifyBoxView->getRect().getWidth();
	const S32 LEFT = RIGHT - NOTIFY_WIDTH;

	// Make sure it goes slightly offscreen
	return LLRect(LEFT, notify_height-1, RIGHT, -1);
}


// static
void LLNotifyBox::onClickButton(void* data)
{
	InstanceAndS32* self_and_button = (InstanceAndS32*)data;
	LLNotifyBox* self = self_and_button->mSelf;
	S32 button = self_and_button->mButton;

	// for caution notifications, check if the last button in the prompt was clicked
	// unless it is the only button, in which case it will just be an "OK" button
	if ((self->mIsCaution) && (button > 0) && (button == (self->mNumOptions - 1)))
	{
		// show an alert dialog containing more explanation about the debit permission
		LLAlertDialog::showXml("DebitPermissionDetails");

		// keep this notification open
		return;
	}

	if (self->mBehavior->mCallback)
	{
		self->mBehavior->mCallback(button, self->mBehavior->mData);
		self->mBehavior->mCallback = NULL; // Notification callbacks only expect to be called once ever
	}

	self->close();
}


// static
void LLNotifyBox::onClickNext(void* data)
{
	LLNotifyBox* self = static_cast<LLNotifyBox*>(data);
	self->moveToBack(true);
}

// static
LLPointer<LLNotifyBoxTemplate> LLNotifyBox::getTemplate(const std::string& xml_desc)
{
	// get template
	
	if (!sDefaultTemplate)
	{
		// default template is non-unique, of course
		sDefaultTemplate = new LLNotifyBoxTemplate(FALSE, gSavedSettings.getF32("NotifyTipDuration"));
		sDefaultTemplate->addOption("OK", FALSE);
	}
	
	LLPointer<LLNotifyBoxTemplate> xml_template;
	template_map_t::iterator iter = sNotifyTemplates.find(xml_desc);
	if (iter != sNotifyTemplates.end())
	{
		xml_template = iter->second;
	}
	else
	{
		std::string tmsg = "[Notification template not found:\n " + xml_desc + " ]";
		sDefaultTemplate->setMessage(tmsg);
		xml_template = sDefaultTemplate;
	}

	return xml_template;
}

//-----------------------------------------------------------------------------

//static
const std::string LLNotifyBox::getTemplateMessage(const std::string& xml_desc, const LLStringUtil::format_map_t& args)
{
	template_map_t::iterator iter = sNotifyTemplates.find(xml_desc);
	if (iter != sNotifyTemplates.end())
	{
		std::string message = iter->second->mMessage;
		format(message, args);
		return message;
	}
	else
	{
		return xml_desc;
	}
}

//static
const std::string LLNotifyBox::getTemplateMessage(const std::string& xml_desc)
{
	template_map_t::iterator iter = sNotifyTemplates.find(xml_desc);
	if (iter != sNotifyTemplates.end())
	{
		return iter->second->mMessage;
	}
	else
	{
		return xml_desc;
	}
}

// method to check whether a given notify template show as a caution or not
BOOL LLNotifyBox::getTemplateIsCaution(const std::string& xml_desc)
{
	BOOL is_caution = FALSE;

	template_map_t::iterator iter = sNotifyTemplates.find(xml_desc);
	if (iter != sNotifyTemplates.end())
	{
		is_caution = iter->second->mIsCaution;
	}

	return is_caution;
}

//static
bool LLNotifyBox::parseNotify(const std::string& xml_filename)
{
	LLXMLNodePtr root;

	BOOL success  = LLUICtrlFactory::getLayeredXMLNode(xml_filename, root);

	if (!success || root.isNull() || !root->hasName( "notifications" ))
	{
		llerrs << "Problem reading UI Notify file: " << xml_filename << llendl;
		return false;
	}

	for (LLXMLNode* notify = root->getFirstChild();
		 notify != NULL; notify = notify->getNextSibling())
	{
		if (!notify->hasName("notify"))
		{
			continue;
		}

		BOOL unique = FALSE;
		notify->getAttributeBOOL("unique", unique);

		F32 duration = gSavedSettings.getF32("NotifyTipDuration");
		notify->getAttributeF32("duration", duration);
		
		LLPointer<LLNotifyBoxTemplate> xml_template = new LLNotifyBoxTemplate(unique, duration);

		// label=
		std::string notify_name;
		if (notify->getAttributeString("name", notify_name))
		{
			xml_template->mLabel = notify_name;
		}
		else
		{
			llwarns << "Unable to parse notify with no name" << llendl;
			continue;
		}
		// modal=
		BOOL tip;
		if (notify->getAttributeBOOL("tip", tip))
		{
			xml_template->mIsTip = tip;
		}

		// parse a bool attribute named "caution" to determine
		// whether this notification gets cautionary special handling
		BOOL caution = FALSE;
		if (notify->getAttributeBOOL("caution", caution))
		{
			if (xml_template)
			{
				xml_template->mIsCaution = caution;
			}
		}
		
				
		S32 btn_idx = 0;
		for (LLXMLNode* child = notify->getFirstChild();
			 child != NULL; child = child->getNextSibling())
		{
			// <message>
			if (child->hasName("message"))
			{
				xml_template->mMessage = child->getTextContents();
			}

			// <option>
			if (child->hasName("option"))
			{
				std::string label = child->getValue();
				BOOL is_default = FALSE;
				child->getAttributeBOOL("default", is_default);
				std::string ignore_text;
				if (!child->getAttributeString("ignore", ignore_text))
				{
					ignore_text = label;
				}
				xml_template->addOption(label, is_default);
				btn_idx++;
			}
		}

		//*TODO:translate
		if (xml_template->mOptions.empty())
		{
			xml_template->addOption("OK", FALSE);
		}
		sNotifyTemplates[xml_template->mLabel] = xml_template;
	}
	return true;
}

LLNotifyBoxView::LLNotifyBoxView(const std::string& name, const LLRect& rect, BOOL mouse_opaque, U32 follows)
	: LLUICtrl(name,rect,mouse_opaque,NULL,NULL,follows) 
{
}

LLNotifyBox * LLNotifyBoxView::getFirstNontipBox() const
{
	// *TODO: Don't make assumptions like this!
	// assumes every child is a notify box
	for(child_list_const_iter_t iter = getChildList()->begin();
			iter != getChildList()->end();
			iter++)
	{
		// hack! *TODO: Integrate llnotify and llgroupnotify
		if(isGroupNotifyBox(*iter))
		{
			continue;
		}
		
		LLNotifyBox* box = (LLNotifyBox*)(*iter);
		if(!box->isTip() && !box->isDead())
		{
			return box;
		}
	}
	return NULL;
}

void LLNotifyBoxView::showOnly(LLView * view)
{
	if(view) 
	{
		// assumes that the argument is actually a child
		LLNotifyBox * shown = dynamic_cast<LLNotifyBox*>(view);
		if(!shown)
		{
			return ;
		}

		// make every other notification invisible
		for(child_list_const_iter_t iter = getChildList()->begin();
			iter != getChildList()->end();
			iter++)
		{
			if(isGroupNotifyBox(*iter))
			{
				continue;
			}

			LLNotifyBox * box = (LLNotifyBox*)(*iter);
			if(box != view && box->getVisible() && !box->isTip())
			{
				box->setVisible(FALSE);
			}
		}
		shown->setVisible(TRUE);
		sendChildToFront(shown);
	}
}

void LLNotifyBoxView::purgeMessagesMatching(const Matcher& matcher)
{
	// Make a *copy* of the child list to iterate over 
	// since we'll be removing items from the real list as we go.
	LLView::child_list_t notification_queue(*getChildList());
	for(LLView::child_list_iter_t iter = notification_queue.begin();
		iter != notification_queue.end();
		iter++)
	{
		if(isGroupNotifyBox(*iter))
		{
			continue;
		}

		LLNotifyBox* notification = (LLNotifyBox*)*iter;
		if(matcher.matches(notification->getNotifyCallback(), notification->getUserData()))
		{
			removeChild(notification);
		}
	}
}

bool LLNotifyBoxView::isGroupNotifyBox(const LLView* view) const
{
	if (view->getName() == "groupnotify")
	{
		return TRUE ;
	}

	return FALSE ;
}

