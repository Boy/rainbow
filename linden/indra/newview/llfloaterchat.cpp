/** 
 * @file llfloaterchat.cpp
 * @brief LLFloaterChat class implementation
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

/**
 * Actually the "Chat History" floater.
 * Should be llfloaterchathistory, not llfloaterchat.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterchat.h"
#include "llfloateractivespeakers.h"
#include "llfloaterscriptdebug.h"

#include "llcachename.h"
#include "llchat.h"
#include "llfontgl.h"
#include "llrect.h"
#include "llerror.h"
#include "llstring.h"
#include "message.h"

// project include
#include "llagent.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llconsole.h"
#include "llfloaterchatterbox.h"
#include "llfloatermute.h"
#include "llkeyboard.h"
//#include "lllineeditor.h"
#include "llmutelist.h"
//#include "llresizehandle.h"
#include "llchatbar.h"
#include "llstatusbar.h"
#include "llviewertexteditor.h"
#include "llviewergesture.h"			// for triggering gestures
#include "llviewermessage.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llvoavatar.h"
#include "lluictrlfactory.h"
#include "llchatbar.h"
#include "lllogchat.h"
#include "lltexteditor.h"
#include "llfloaterhtml.h"
#include "llweb.h"
#include "llstylemap.h"

// Used for LCD display
extern void AddNewIMToLCD(const std::string &newLine);
extern void AddNewChatToLCD(const std::string &newLine);
//
// Constants
//
const F32 INSTANT_MSG_SIZE = 8.0f;
const F32 CHAT_MSG_SIZE = 8.0f;
const LLColor4 MUTED_MSG_COLOR(0.5f, 0.5f, 0.5f, 1.f);
const S32 MAX_CHATTER_COUNT = 16;

//
// Global statics
//
LLColor4 get_text_color(const LLChat& chat);

LLColor4 get_extended_text_color(const LLChat& chat, LLColor4 defaultColor)
{
	if (gSavedSettings.getBOOL("HighlightOwnNameInChat"))
	{
		std::string new_line = std::string(chat.mText);
		int name_pos = new_line.find(chat.mFromName);
		if (name_pos == 0)
		{
			new_line = new_line.substr(chat.mFromName.length());
			if (new_line.find(": ") == 0)
				new_line = new_line.substr(2);
			else
				new_line = new_line.substr(1);
		}

		if (LLFloaterChat::isOwnNameInText(new_line))
		{
			return gSavedSettings.getColor4("OwnNameChatColor");
		}
	}

	return defaultColor;
}

//
// Member Functions
//
LLFloaterChat::LLFloaterChat(const LLSD& seed)
:	LLFloater(std::string("chat floater"), std::string("FloaterChatRect"), LLStringUtil::null, 
			  RESIZE_YES, 440, 100, DRAG_ON_TOP, MINIMIZE_NO, CLOSE_YES),
	mPanel(NULL)
{
	mFactoryMap["chat_panel"] = LLCallbackMap(createChatPanel, NULL);
	mFactoryMap["active_speakers_panel"] = LLCallbackMap(createSpeakersPanel, NULL);
	// do not automatically open singleton floaters (as result of getInstance())
	BOOL no_open = FALSE;
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_chat_history.xml",&getFactoryMap(),no_open);

	childSetCommitCallback("show mutes",onClickToggleShowMute,this); //show mutes
	childSetVisible("Chat History Editor with mute",FALSE);
	childSetAction("toggle_active_speakers_btn", onClickToggleActiveSpeakers, this);
	setDefaultBtn("Chat");
}

LLFloaterChat::~LLFloaterChat()
{
	// Children all cleaned up by default view destructor.
}

void LLFloaterChat::setVisible(BOOL visible)
{
	LLFloater::setVisible( visible );

	gSavedSettings.setBOOL("ShowChatHistory", visible);
}

void LLFloaterChat::draw()
{
	// enable say and shout only when text available
		
	childSetValue("toggle_active_speakers_btn", childIsVisible("active_speakers_panel"));

	LLChatBar* chat_barp = getChild<LLChatBar>("chat_panel", TRUE);
	if (chat_barp)
	{
		chat_barp->refresh();
	}

	mPanel->refreshSpeakers();
	LLFloater::draw();
}

BOOL LLFloaterChat::postBuild()
{
	mPanel = (LLPanelActiveSpeakers*)getChild<LLPanel>("active_speakers_panel");

	LLChatBar* chat_barp = getChild<LLChatBar>("chat_panel", TRUE);
	if (chat_barp)
	{
		chat_barp->setGestureCombo(getChild<LLComboBox>( "Gesture"));
	}
	return TRUE;
}

// public virtual
void LLFloaterChat::onClose(bool app_quitting)
{
	if (!app_quitting)
	{
		gSavedSettings.setBOOL("ShowChatHistory", FALSE);
	}
	setVisible(FALSE);
}

void LLFloaterChat::onVisibilityChange(BOOL new_visibility)
{
	// Hide the chat overlay when our history is visible.
	updateConsoleVisibility();

	// stop chat history tab from flashing when it appears
	if (new_visibility)
	{
		LLFloaterChatterBox::getInstance()->setFloaterFlashing(this, FALSE);
	}

	LLFloater::onVisibilityChange(new_visibility);
}

void LLFloaterChat::setMinimized(BOOL minimized)
{
	LLFloater::setMinimized(minimized);
	updateConsoleVisibility();
}


void LLFloaterChat::updateConsoleVisibility()
{
	// determine whether we should show console due to not being visible
	gConsole->setVisible( !isInVisibleChain()								// are we not in part of UI being drawn?
							|| isMinimized()								// are we minimized?
							|| (getHost() && getHost()->isMinimized() ));	// are we hosted in a minimized floater?
}

void add_timestamped_line(LLViewerTextEditor* edit, const LLChat &chat, const LLColor4& color)
{
	std::string line = chat.mText;
	bool prepend_newline = true;
	if (gSavedSettings.getBOOL("ChatShowTimestamps"))
	{
		edit->appendTime(prepend_newline);
		prepend_newline = false;
	}

	// If the msg is not from an agent (not yourself though),
	// extract out the sender name and replace it with the hotlinked name.
	if (chat.mSourceType == CHAT_SOURCE_AGENT &&
		chat.mFromID != LLUUID::null &&
		(line.length() > chat.mFromName.length() && line.find(chat.mFromName,0) == 0))
	{
//MK
		if (!gRRenabled || !gAgent.mRRInterface.mContainsShownames)
		{
//mk
			size_t pos;
			if (chat.mFromName.empty() || chat.mFromName.find_first_not_of(' ') == std::string::npos)
			{
				// Name is empty... Set the link on the first word instead (skipping leading spaces)...
				pos = line.find_first_not_of(' ');
				if (pos == std::string::npos)
				{
					// Only spaces !... Normally not possible, but...
					pos = 1;
				}
				else
				{
					pos = line.find(' ', pos);
					if (pos == std::string::npos)
					{
						// Only one word in the line...
						pos = line.length();
						line += " ";
					}
				}
			}
			else
			{
				pos = chat.mFromName.length() + 1;
			}
			std::string start_line = line.substr(0, pos);
			line = line.substr(pos);
			const LLStyleSP &sourceStyle = LLStyleMap::instance().lookup(chat.mFromID);
			edit->appendStyledText(start_line, false, prepend_newline, &sourceStyle);
			prepend_newline = false;
//MK
		}
//mk
	}
	edit->appendColoredText(line, false, prepend_newline, color);
}

void log_chat_text(const LLChat& chat)
{
		std::string histstr;
		if (gSavedPerAccountSettings.getBOOL("LogChatTimestamp"))
			histstr = LLLogChat::timestamp(gSavedPerAccountSettings.getBOOL("LogTimestampDate")) + chat.mText;
		else
			histstr = chat.mText;

		LLLogChat::saveHistory(std::string("chat"),histstr);
}
// static
void LLFloaterChat::addChatHistory(const LLChat& chat, bool log_to_file)
{	
	if ( gSavedPerAccountSettings.getBOOL("LogChat") && log_to_file) 
	{
		log_chat_text(chat);
	}
	
	LLColor4 color = get_text_color(chat);
	
	if (!log_to_file) color = LLColor4::grey;	//Recap from log file.

	if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
	{
		LLFloaterScriptDebug::addScriptLine(chat.mText,
											chat.mFromName, 
											color, 
											chat.mFromID);
		if (!gSavedSettings.getBOOL("ScriptErrorsAsChat"))
		{
			return;
		}
	}
	
	// could flash the chat button in the status bar here. JC
	LLFloaterChat* chat_floater = LLFloaterChat::getInstance(LLSD());
	LLViewerTextEditor*	history_editor = chat_floater->getChild<LLViewerTextEditor>("Chat History Editor");
	LLViewerTextEditor*	history_editor_with_mute = chat_floater->getChild<LLViewerTextEditor>("Chat History Editor with mute");

	history_editor->setParseHTML(TRUE);
	history_editor_with_mute->setParseHTML(TRUE);
	
	if (!chat.mMuted)
	{
		add_timestamped_line(history_editor, chat, color);
		add_timestamped_line(history_editor_with_mute, chat, color);
	}
	else
	{
		// desaturate muted chat
		LLColor4 muted_color = lerp(color, LLColor4::grey, 0.5f);
		add_timestamped_line(history_editor_with_mute, chat, color);
	}
	
	// add objects as transient speakers that can be muted
	if (chat.mSourceType == CHAT_SOURCE_OBJECT)
	{
		chat_floater->mPanel->setSpeaker(chat.mFromID, chat.mFromName, LLSpeaker::STATUS_NOT_IN_CHANNEL, LLSpeaker::SPEAKER_OBJECT);
	}

	// start tab flashing on incoming text from other users (ignoring system text, etc)
	if (!chat_floater->isInVisibleChain() && chat.mSourceType == CHAT_SOURCE_AGENT)
	{
		LLFloaterChatterBox::getInstance()->setFloaterFlashing(chat_floater, TRUE);
	}
}

// static
void LLFloaterChat::setHistoryCursorAndScrollToEnd()
{
	LLViewerTextEditor*	history_editor = LLFloaterChat::getInstance(LLSD())->getChild<LLViewerTextEditor>("Chat History Editor");
	LLViewerTextEditor*	history_editor_with_mute = LLFloaterChat::getInstance(LLSD())->getChild<LLViewerTextEditor>("Chat History Editor with mute");
	
	if (history_editor) 
	{
		history_editor->setCursorAndScrollToEnd();
	}
	if (history_editor_with_mute)
	{
		 history_editor_with_mute->setCursorAndScrollToEnd();
	}
}


//static 
void LLFloaterChat::onClickMute(void *data)
{
	LLFloaterChat* self = (LLFloaterChat*)data;

	LLComboBox*	chatter_combo = self->getChild<LLComboBox>("chatter combobox");

	const std::string& name = chatter_combo->getSimple();
	LLUUID id = chatter_combo->getCurrentID();

	if (name.empty()) return;

	LLMute mute(id);
	mute.setFromDisplayName(name);
	LLMuteList::getInstance()->add(mute);
	
	LLFloaterMute::showInstance();
}

//static
void LLFloaterChat::onClickToggleShowMute(LLUICtrl* caller, void *data)
{
	LLFloaterChat* floater = (LLFloaterChat*)data;


	//LLCheckBoxCtrl*	
	BOOL show_mute = floater->getChild<LLCheckBoxCtrl>("show mutes")->get();
	LLViewerTextEditor*	history_editor = floater->getChild<LLViewerTextEditor>("Chat History Editor");
	LLViewerTextEditor*	history_editor_with_mute = floater->getChild<LLViewerTextEditor>("Chat History Editor with mute");

	if (!history_editor || !history_editor_with_mute)
		return;

	//BOOL show_mute = floater->mShowMuteCheckBox->get();
	if (show_mute)
	{
		history_editor->setVisible(FALSE);
		history_editor_with_mute->setVisible(TRUE);
		history_editor_with_mute->setCursorAndScrollToEnd();
	}
	else
	{
		history_editor->setVisible(TRUE);
		history_editor_with_mute->setVisible(FALSE);
		history_editor->setCursorAndScrollToEnd();
	}
}

// Put a line of chat in all the right places
void LLFloaterChat::addChat(const LLChat& chat, 
			  BOOL from_instant_message, 
			  BOOL local_agent)
{
	LLColor4 text_color = get_text_color(chat);

	BOOL invisible_script_debug_chat = 
			chat.mChatType == CHAT_TYPE_DEBUG_MSG
			&& !gSavedSettings.getBOOL("ScriptErrorsAsChat");

#if LL_LCD_COMPILE
	// add into LCD displays
	if (!invisible_script_debug_chat)
	{
		if (!from_instant_message)
		{
			AddNewChatToLCD(chat.mText);
		}
		else
		{
			AddNewIMToLCD(chat.mText);
		}
	}
#endif
	if (!invisible_script_debug_chat 
		&& !chat.mMuted 
		&& gConsole 
		&& !local_agent)
	{
		F32 size = CHAT_MSG_SIZE;
		if (chat.mSourceType == CHAT_SOURCE_SYSTEM)
		{
			text_color = gSavedSettings.getColor("SystemChatColor");
		}
		else if(from_instant_message)
		{
			text_color = gSavedSettings.getColor("IMChatColor");
			size = INSTANT_MSG_SIZE;
		}
		gConsole->addLine(chat.mText, size, text_color);
	}

	if(from_instant_message && gSavedPerAccountSettings.getBOOL("LogChatIM"))
		log_chat_text(chat);
	
	if(from_instant_message && gSavedSettings.getBOOL("IMInChatHistory"))
		addChatHistory(chat,false);
	
	if(!from_instant_message)
		addChatHistory(chat);
}

LLColor4 get_text_color(const LLChat& chat)
{
	LLColor4 text_color;

	if(chat.mMuted)
	{
		text_color.setVec(0.8f, 0.8f, 0.8f, 1.f);
	}
	else
	{
		switch(chat.mSourceType)
		{
		case CHAT_SOURCE_SYSTEM:
			text_color = gSavedSettings.getColor4("SystemChatColor");
			break;
		case CHAT_SOURCE_AGENT:
		    if (chat.mFromID.isNull())
			{
				text_color = gSavedSettings.getColor4("SystemChatColor");
			}
			else
			{
				if(gAgent.getID() == chat.mFromID)
				{
					text_color = gSavedSettings.getColor4("UserChatColor");
				}
				else
				{
					text_color = get_extended_text_color(chat, gSavedSettings.getColor4("AgentChatColor"));
				}
			}
			break;
		case CHAT_SOURCE_OBJECT:
			if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
			{
				text_color = gSavedSettings.getColor4("ScriptErrorColor");
			}
			else if ( chat.mChatType == CHAT_TYPE_OWNER )
			{
				text_color = gSavedSettings.getColor4("llOwnerSayChatColor");
			}
			else
			{
				text_color = gSavedSettings.getColor4("ObjectChatColor");
			}
			break;
		default:
			text_color.setToWhite();
		}

		if (!chat.mPosAgent.isExactlyZero())
		{
			LLVector3 pos_agent = gAgent.getPositionAgent();
			F32 distance = dist_vec(pos_agent, chat.mPosAgent);
			if (distance > gAgent.getNearChatRadius())
			{
				// diminish far-off chat
				text_color.mV[VALPHA] = 0.8f;
			}
		}
	}

	return text_color;
}

//static
void LLFloaterChat::loadHistory()
{
	LLLogChat::loadHistory(std::string("chat"), &chatFromLogFile, (void *)LLFloaterChat::getInstance(LLSD())); 
}

//static
void LLFloaterChat::chatFromLogFile(LLLogChat::ELogLineType type , std::string line, void* userdata)
{
	switch (type)
	{
	case LLLogChat::LOG_EMPTY:
	case LLLogChat::LOG_END:
		// *TODO: nice message from XML file here
		break;
	case LLLogChat::LOG_LINE:
		{
			LLChat chat;					
			chat.mText = line;
			addChatHistory(chat,  FALSE);
		}
		break;
	default:
		// nothing
		break;
	}
}

//static
void* LLFloaterChat::createSpeakersPanel(void* data)
{
	return new LLPanelActiveSpeakers(LLLocalSpeakerMgr::getInstance(), TRUE);
}

//static
void* LLFloaterChat::createChatPanel(void* data)
{
	LLChatBar* chatp = new LLChatBar();
	return chatp;
}

// static
void LLFloaterChat::onClickToggleActiveSpeakers(void* userdata)
{
	LLFloaterChat* self = (LLFloaterChat*)userdata;
//MK
	if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
	{
		if (!self->childIsVisible("active_speakers_panel")) return;
	}
//mk
	self->childSetVisible("active_speakers_panel", !self->childIsVisible("active_speakers_panel"));
}

//static 
bool LLFloaterChat::visible(LLFloater* instance, const LLSD& key)
{
	return VisibilityPolicy<LLFloater>::visible(instance, key);
}

//static 
void LLFloaterChat::show(LLFloater* instance, const LLSD& key)
{
	VisibilityPolicy<LLFloater>::show(instance, key);
}

//static 
void LLFloaterChat::hide(LLFloater* instance, const LLSD& key)
{
	if(instance->getHost())
	{
		LLFloaterChatterBox::hideInstance();
	}
	else
	{
		VisibilityPolicy<LLFloater>::hide(instance, key);
	}
}

static std::set<std::string> highlight_words;

bool make_words_list()
{
	bool changed = false;
	size_t index;
	std::string name, part;
	static std::string nicknames = "";
	if (gSavedPerAccountSettings.getString("HighlightNicknames") != nicknames)
	{
		nicknames = gSavedPerAccountSettings.getString("HighlightNicknames");
		changed = true;
	}

#ifdef LL_DISPLAY_NAMES
	LLAvatarName avatar_name;
	static std::string display_name = "";
	static bool highlight_display_name = false;
	bool do_highlight = gSavedPerAccountSettings.getBOOL("HighlightDisplayName") &&
						LLAvatarNameCache::useDisplayNames() &&
						LLAvatarNameCache::get(gAgent.getID(), &avatar_name);

	if (do_highlight != highlight_display_name)
	{
		highlight_display_name = do_highlight;
		changed = true;
		if (!highlight_display_name)
		{
			display_name = "";
		}
	}

	if (highlight_display_name)
	{
		if (avatar_name.mIsDisplayNameDefault)
		{
			name = "";
		}	
		else
		{
			name = avatar_name.mDisplayName;
			LLStringUtil::toLower(name);
		}

		if (name != display_name)
		{
			display_name = name;
			changed = true;
		}
	}
#endif

	if (changed)
	{
		// Rebuild the whole list
		highlight_words.clear();

		// First, fetch the avatar name (note: we don't use
		// gSavedSettings.getString("[First/Last]Name") here,
		// because those are not set when using --autologin).
		LLVOAvatar* avatarp = gAgent.getAvatarObject();
		LLNameValue *firstname = avatarp->getNVPair("FirstName");
		LLNameValue *lastname = avatarp->getNVPair("LastName");
		name.assign(firstname->getString());
		LLStringUtil::toLower(name);
		highlight_words.insert(name);
		name.assign(lastname->getString());
		if (name != "Resident" && highlight_words.count(name) == 0)
		{
			LLStringUtil::toLower(name);
			highlight_words.insert(name);
		}

#ifdef LL_DISPLAY_NAMES
		if (!display_name.empty())
		{
			name = display_name;
			while ((index = name.find(' ')) != std::string::npos)
			{
				part = name.substr(0, index);
				name = name.substr(index + 1);
				if (part.length() > 3)
				{
					highlight_words.insert(part);
				}
			}
			if (name.length() > 3 && highlight_words.count(name) == 0)
			{
				highlight_words.insert(name);
			}
		}
#endif

		if (!nicknames.empty())
		{
			name = nicknames;
			LLStringUtil::toLower(name);
			LLStringUtil::replaceChar(name, ' ', ','); // Accept space and comma separated list
			while ((index = name.find(',')) != std::string::npos)
			{
				part = name.substr(0, index);
				name = name.substr(index + 1);
				if (part.length() > 2)
				{
					highlight_words.insert(part);
				}
			}
			if (name.length() > 2 && highlight_words.count(name) == 0)
			{
				highlight_words.insert(name);
			}
		}
	}

	return changed;
}

//static 
bool LLFloaterChat::isOwnNameInText(const std::string &text_line)
{
	if (!gAgent.getAvatarObject())
	{
		return false;
	}
	const std::string separators(" .,;:'!?*-()[]\"");
	bool flag;
	char before, after;
	size_t index = 0, larger_index, length = 0;
	std::set<std::string>::iterator it;
	std::string name;
	std::string text = " " + text_line + " ";
	LLStringUtil::toLower(text);

	if (make_words_list())
	{
		name = "Highlights words list changed to: ";
		flag = false;
		for (it = highlight_words.begin(); it != highlight_words.end(); it++)
		{
			if (flag) {
				name += ", ";
			}
			name += *it;
			flag = true;
		}
		LL_INFOS("Chat/IM posts highlighting") << name << LL_ENDL;
	}

	do
	{
		flag = false;
		larger_index = 0;
		for (it = highlight_words.begin(); it != highlight_words.end(); it++)
		{
			name = *it;
			index = text.find(name);
			if (index != std::string::npos)
			{
				flag = true;
				before = text.at(index - 1);
				after = text.at(index + name.length());
				if (separators.find(before) != std::string::npos && separators.find(after) != std::string::npos)
				{
					return true;
				}
				if (index >= larger_index)
				{
					larger_index = index;
					length = name.length();
				}
			}
		}
		if (flag)
		{
			text = " " + text.substr(index + length);
		}
	}
	while (flag);

	return false;
}
