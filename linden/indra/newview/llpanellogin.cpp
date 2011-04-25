/** 
 * @file llpanellogin.cpp
 * @brief Login dialog and logo display
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

#include "llpanellogin.h"
#include "llpanelgeneral.h"

#include "indra_constants.h"		// for key and mask constants
#include "llfontgl.h"
#include "llmd5.h"
#include "llsecondlifeurls.h"
#include "llversionviewer.h"
#include "v4color.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcommandhandler.h"
#include "llcombobox.h"
#include "llcurl.h"
#include "llviewercontrol.h"
#include "llfloaterabout.h"
#include "llfloatertest.h"
#include "llfloaterpreference.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llstartup.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluiconstants.h"
#include "llurlhistory.h" // OGPX : regionuri text box has a history of region uris (if FN/LN are loaded at startup)
#include "llurlsimstring.h"
#include "llviewerbuild.h"
#include "llviewerimagelist.h"
#include "llviewermenu.h"			// for handle_preferences()
#include "llviewernetwork.h"
#include "llviewerwindow.h"			// to link into child list
#include "llnotify.h"
#include "llurlsimstring.h"
#include "lluictrlfactory.h"
#include "llhttpclient.h"
#include "llweb.h"
#include "llwebbrowserctrl.h"

#include "llfloaterhtml.h"

#include "llfloaterhtmlhelp.h"
#include "llfloatertos.h"

#include "llglheaders.h"

#define USE_VIEWER_AUTH 0

const S32 BLACK_BORDER_HEIGHT = 160;
const S32 MAX_PASSWORD = 16;

LLPanelLogin *LLPanelLogin::sInstance = NULL;
BOOL LLPanelLogin::sCapslockDidNotification = FALSE;


class LLLoginRefreshHandler : public LLCommandHandler
{
public:
	// don't allow from external browsers
	LLLoginRefreshHandler() : LLCommandHandler("login_refresh", true) { }
	bool handle(const LLSD& tokens, const LLSD& query_map, LLWebBrowserCtrl* web)
	{	
		if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
		{
			LLPanelLogin::loadLoginPage();
		}	
		return true;
	}
};

LLLoginRefreshHandler gLoginRefreshHandler;



// helper class that trys to download a URL from a web site and calls a method 
// on parent class indicating if the web server is working or not
class LLIamHereLogin : public LLHTTPClient::Responder
{
	private:
		LLIamHereLogin( LLPanelLogin* parent ) :
		   mParent( parent )
		{}

		LLPanelLogin* mParent;

	public:
		static boost::intrusive_ptr< LLIamHereLogin > build( LLPanelLogin* parent )
		{
			return boost::intrusive_ptr< LLIamHereLogin >( new LLIamHereLogin( parent ) );
		};

		virtual void  setParent( LLPanelLogin* parentIn )
		{
			mParent = parentIn;
		};

		// We don't actually expect LLSD back, so need to override completedRaw
		virtual void completedRaw(U32 status, const std::string& reason,
								  const LLChannelDescriptors& channels,
								  const LLIOPipe::buffer_ptr_t& buffer)
		{
			completed(status, reason, LLSD()); // will call result() or error()
		}
	
		virtual void result( const LLSD& content )
		{
			if ( mParent )
				mParent->setSiteIsAlive( true );
		};

		virtual void error( U32 status, const std::string& reason )
		{
			if ( mParent )
				mParent->setSiteIsAlive( false );
		};
};

// this is global and not a class member to keep crud out of the header file
namespace {
	boost::intrusive_ptr< LLIamHereLogin > gResponsePtr = 0;
};

void set_start_location(LLUICtrl* ctrl, void* data)
{
    LLURLSimString::setString(ctrl->getValue().asString());
}

//---------------------------------------------------------------------------
// Public methods
//---------------------------------------------------------------------------
LLPanelLogin::LLPanelLogin(const LLRect &rect,
						 BOOL show_server,
						 void (*callback)(S32 option, void* user_data),
						 void *cb_data)
:	LLPanel(std::string("panel_login"), LLRect(0,600,800,0), FALSE),		// not bordered
	mLogoImage(),
	mCallback(callback),
	mCallbackData(cb_data),
	mHtmlAvailable( TRUE ),
	mShowServerCombo(show_server)
{
	setFocusRoot(TRUE);

	setBackgroundVisible(FALSE);
	setBackgroundOpaque(TRUE);

	// instance management
	if (LLPanelLogin::sInstance)
	{
		llwarns << "Duplicate instance of login view deleted" << llendl;
		delete LLPanelLogin::sInstance;

		// Don't leave bad pointer in gFocusMgr
		gFocusMgr.setDefaultKeyboardFocus(NULL);
	}

	LLPanelLogin::sInstance = this;

	// add to front so we are the bottom-most child
	gViewerWindow->getRootView()->addChildAtEnd(this);

	// Logo
	mLogoImage = LLUI::getUIImage("startup_logo.j2c");

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_login.xml");
	
#if USE_VIEWER_AUTH
	//leave room for the login menu bar
	setRect(LLRect(0, rect.getHeight()-18, rect.getWidth(), 0)); 
#endif
	reshape(rect.getWidth(), rect.getHeight());

#if !USE_VIEWER_AUTH
	LLComboBox* first_name_combo = sInstance->getChild<LLComboBox>("first_name_combo");
	first_name_combo->setCommitCallback(onSelectLoginEntry);
	first_name_combo->setFocusLostCallback(onLoginComboLostFocus);
	first_name_combo->setPrevalidate(LLLineEditor::prevalidatePrintableNoSpace);
	first_name_combo->setSuppressTentative(true);

	LLLineEditor* last_name_edit = sInstance->getChild<LLLineEditor>("last_name_edit");
	last_name_edit->setPrevalidate(LLLineEditor::prevalidatePrintableNoSpace);
	last_name_edit->setCommitCallback(onLastNameEditLostFocus);

	childSetCommitCallback("remember_name_check", onNameCheckChanged);
	childSetCommitCallback("password_edit", mungePassword);
	childSetKeystrokeCallback("password_edit", onPassKey, this);
	childSetUserData("password_edit", this);

	// change z sort of clickable text to be behind buttons
	sendChildToBack(getChildView("channel_text"));
	sendChildToBack(getChildView("forgot_password_text"));

	LLLineEditor* edit = getChild<LLLineEditor>("password_edit");
	if (edit) edit->setDrawAsterixes(TRUE);

	//OGPX : This keeps the uris in a history file 
	//OGPX TODO: should this be inside an OGP only check?
	LLComboBox* regioncombo = getChild<LLComboBox>("regionuri_edit"); 
	regioncombo->setAllowTextEntry(TRUE, 256, FALSE);
	std::string  current_regionuri = gSavedSettings.getString("CmdLineRegionURI");

	// iterate on uri list adding to combobox (couldn't figure out how to add them all in one call)
	// ... and also append the command line value we might have gotten to the URLHistory
	LLSD regionuri_history = LLURLHistory::getURLHistory("regionuri");
	LLSD::array_iterator iter_history = regionuri_history.beginArray();
	LLSD::array_iterator iter_end = regionuri_history.endArray();
	for (; iter_history != iter_end; ++iter_history)
	{
		regioncombo->addSimpleElement((*iter_history).asString());
	}

	if ( LLURLHistory::appendToURLCollection("regionuri",current_regionuri)) 
	{
		// since we are in login, another read of urlhistory file is going to happen 
		// so we need to persist the new value we just added (or maybe we should do it in startup.cpp?)

		// since URL history only populated on create of sInstance, add to combo list directly
		regioncombo->addSimpleElement(current_regionuri);
	}
	
	// select which is displayed if we have a current URL.
	regioncombo->setSelectedByValue(LLSD(current_regionuri),TRUE);

	//llinfos << " url history: " << LLSDOStreamer<LLSDXMLFormatter>(LLURLHistory::getURLHistory("regionuri")) << llendl;

	LLComboBox* combo = getChild<LLComboBox>("start_location_combo");
	combo->setAllowTextEntry(TRUE, 128, FALSE);

	// The XML file loads the combo with the following labels:
	// 0 - "My Home"
	// 1 - "My Last Location"
	// 2 - "<Type region name>"

	BOOL login_last = gSavedSettings.getBOOL("LoginLastLocation");
	std::string sim_string = LLURLSimString::sInstance.mSimString;
	if (!sim_string.empty())
	{
		// Replace "<Type region name>" with this region name
		combo->remove(2);
		combo->add( sim_string );
		combo->setTextEntry(sim_string);
		combo->setCurrentByIndex( 2 );
	}
	else if (login_last)
	{
		combo->setCurrentByIndex( 1 );
	}
	else
	{
		combo->setCurrentByIndex( 0 );
	}

	combo->setCommitCallback( &set_start_location );

	LLComboBox* server_choice_combo = sInstance->getChild<LLComboBox>("server_combo");
	server_choice_combo->setCommitCallback(onSelectServer);
	server_choice_combo->setFocusLostCallback(onServerComboLostFocus);

	childSetAction("connect_btn", onClickConnect, this);

	setDefaultBtn("connect_btn");

	// childSetAction("quit_btn", onClickQuit, this);

	std::string channel = gSavedSettings.getString("VersionChannelName");
	std::string version = llformat("%d.%d.%d (%d)",
		LL_VERSION_MAJOR,
		LL_VERSION_MINOR,
		LL_VERSION_PATCH,
		LL_VIEWER_BUILD );
	LLTextBox* channel_text = getChild<LLTextBox>("channel_text");
	channel_text->setTextArg("[CHANNEL]", channel);
	channel_text->setTextArg("[VERSION]", version);
	channel_text->setClickedCallback(onClickVersion);
	channel_text->setCallbackUserData(this);
	
	LLTextBox* forgot_password_text = getChild<LLTextBox>("forgot_password_text");
	forgot_password_text->setClickedCallback(onClickForgotPassword);

	LLTextBox* create_new_account_text = getChild<LLTextBox>("create_new_account_text");
	create_new_account_text->setClickedCallback(onClickNewAccount);
#endif    
	
	// get the web browser control
	LLWebBrowserCtrl* web_browser = getChild<LLWebBrowserCtrl>("login_html");
	web_browser->addObserver(this);

	// Need to handle login secondlife:///app/ URLs
	web_browser->setTrusted( true );

	// don't make it a tab stop until SL-27594 is fixed
	web_browser->setTabStop(FALSE);
	web_browser->navigateToLocalPage( "loading", "loading.html" );

	// make links open in external browser
	web_browser->setOpenInExternalBrowser( true );

	// force the size to be correct (XML doesn't seem to be sufficient to do this) (with some padding so the other login screen doesn't show through)
	LLRect htmlRect = getRect();
#if USE_VIEWER_AUTH
	htmlRect.setCenterAndSize( getRect().getCenterX() - 2, getRect().getCenterY(), getRect().getWidth() + 6, getRect().getHeight());
#else
	htmlRect.setCenterAndSize( getRect().getCenterX() - 2, getRect().getCenterY() + 40, getRect().getWidth() + 6, getRect().getHeight() - 78 );
#endif
	web_browser->setRect( htmlRect );
	web_browser->reshape( htmlRect.getWidth(), htmlRect.getHeight(), TRUE );
	reshape( getRect().getWidth(), getRect().getHeight(), 1 );

	// kick off a request to grab the url manually
	gResponsePtr = LLIamHereLogin::build( this );
	std::string login_page = gSavedSettings.getString("LoginPage");
	if (login_page.empty())
	{
		login_page = getString( "real_url" );
	}
	LLHTTPClient::head( login_page, gResponsePtr );

#if !USE_VIEWER_AUTH
	// Initialize visibility (and don't force visibility - use prefs)
	refreshLocation( false );
#endif

}

void LLPanelLogin::setSiteIsAlive( bool alive )
{
	LLWebBrowserCtrl* web_browser = getChild<LLWebBrowserCtrl>("login_html");
	// if the contents of the site was retrieved
	if ( alive )
	{
		if ( web_browser )
		{
			loadLoginPage();
			
			// mark as available
			mHtmlAvailable = TRUE;
		}
	}
	else
	// the site is not available (missing page, server down, other badness)
	{
#if !USE_VIEWER_AUTH
		if ( web_browser )
		{
			// hide browser control (revealing default one)
			web_browser->setVisible( FALSE );

			// mark as unavailable
			mHtmlAvailable = FALSE;
		}
#else

		if ( web_browser )
		{	
			web_browser->navigateToLocalPage( "loading-error" , "index.html" );

			// mark as available
			mHtmlAvailable = TRUE;
		}
#endif
	}
}

void LLPanelLogin::mungePassword(LLUICtrl* caller, void* user_data)
{
	LLPanelLogin* self = (LLPanelLogin*)user_data;
	LLLineEditor* editor = (LLLineEditor*)caller;
	std::string password = editor->getText();

	// Re-md5 if we've changed at all
	if (password != self->mIncomingPassword)
	{
		LLMD5 pass((unsigned char *)password.c_str());
		char munged_password[MD5HEX_STR_SIZE];
		pass.hex_digest(munged_password);
		self->mMungedPassword = munged_password;
	}
}

LLPanelLogin::~LLPanelLogin()
{
	LLPanelLogin::sInstance = NULL;

	// tell the responder we're not here anymore
	if ( gResponsePtr )
		gResponsePtr->setParent( 0 );

	//// We know we're done with the image, so be rid of it.
	//gImageList.deleteImage( mLogoImage );
	
	if ( gFocusMgr.getDefaultKeyboardFocus() == this )
	{
		gFocusMgr.setDefaultKeyboardFocus(NULL);
	}
}

void LLPanelLogin::setLoginHistory(LLSavedLogins const& login_history)
{
	sInstance->mLoginHistoryData = login_history;

	LLComboBox* login_combo = sInstance->getChild<LLComboBox>("first_name_combo");
	llassert(login_combo);
	login_combo->clear();

	LLSavedLoginsList const& saved_login_entries(login_history.getEntries());
	for (LLSavedLoginsList::const_reverse_iterator i = saved_login_entries.rbegin();
	     i != saved_login_entries.rend(); ++i)
	{
		LLSD e = i->asLLSD();
		if (e.isMap()) login_combo->add(i->getDisplayString(), e);
	}
}

// virtual
void LLPanelLogin::draw()
{
	glPushMatrix();
	{
		F32 image_aspect = 1.333333f;
		F32 view_aspect = (F32)getRect().getWidth() / (F32)getRect().getHeight();
		// stretch image to maintain aspect ratio
		if (image_aspect > view_aspect)
		{
			glTranslatef(-0.5f * (image_aspect / view_aspect - 1.f) * getRect().getWidth(), 0.f, 0.f);
			glScalef(image_aspect / view_aspect, 1.f, 1.f);
		}

		S32 width = getRect().getWidth();
		S32 height = getRect().getHeight();

		if ( mHtmlAvailable )
		{
#if !USE_VIEWER_AUTH
			// draw a background box in black
			gl_rect_2d( 0, height - 264, width, 264, LLColor4( 0.0f, 0.0f, 0.0f, 1.f ) );
			// draw the bottom part of the background image - just the blue background to the native client UI
			mLogoImage->draw(0, -264, width + 8, mLogoImage->getHeight());
#endif
		}
		else
		{
			// the HTML login page is not available so default to the original screen
			S32 offscreen_part = height / 3;
			mLogoImage->draw(0, -offscreen_part, width, height+offscreen_part);
		};
	}
	glPopMatrix();

	LLPanel::draw();
}

// virtual
BOOL LLPanelLogin::handleKeyHere(KEY key, MASK mask)
{
	if (( KEY_RETURN == key ) && (MASK_ALT == mask))
	{
		gViewerWindow->toggleFullscreen(FALSE);
		return TRUE;
	}

	if (('P' == key) && (MASK_CONTROL == mask))
	{
		LLFloaterPreference::show(NULL);
		return TRUE;
	}

	if (('T' == key) && (MASK_CONTROL == mask))
	{
		new LLFloaterSimple("floater_test.xml");
		return TRUE;
	}
	
	if ( KEY_F1 == key )
	{
		llinfos << "Spawning HTML help window" << llendl;
		gViewerHtmlHelp.show();
		return TRUE;
	}

# if !LL_RELEASE_FOR_DOWNLOAD
	if ( KEY_F2 == key )
	{
		llinfos << "Spawning floater TOS window" << llendl;
		LLFloaterTOS* tos_dialog = LLFloaterTOS::show(LLFloaterTOS::TOS_TOS,"");
		tos_dialog->startModal();
		return TRUE;
	}
#endif

	if (KEY_RETURN == key && MASK_NONE == mask)
	{
		// let the panel handle UICtrl processing: calls onClickConnect()
		return LLPanel::handleKeyHere(key, mask);
	}

	return LLPanel::handleKeyHere(key, mask);
}

// virtual 
void LLPanelLogin::setFocus(BOOL b)
{
	if(b != hasFocus())
	{
		if(b)
		{
			LLPanelLogin::giveFocus();
		}
		else
		{
			LLPanel::setFocus(b);
		}
	}
}

// static
void LLPanelLogin::giveFocus()
{
#if USE_VIEWER_AUTH
	if (sInstance)
	{
		sInstance->setFocus(TRUE);
	}
#else
	if( sInstance )
	{
		// Grab focus and move cursor to first blank input field
		std::string first = sInstance->childGetText("first_name_combo");
		std::string pass = sInstance->childGetText("password_edit");

		BOOL have_first = !first.empty();
		BOOL have_pass = !pass.empty();

		if (!have_first)
		{
			// User doesn't have a name, so start there.
			LLComboBox* combo = sInstance->getChild<LLComboBox>("first_name_combo");
			combo->setFocusText(TRUE);
		}
		else if (!have_pass)
		{
			LLLineEditor* edit = NULL;
			// User saved his name but not his password.  Move
			// focus to password field.
			edit = sInstance->getChild<LLLineEditor>("password_edit");
			edit->setFocus(TRUE);
			edit->selectAll();
		}
		else
		{
			// else, we have both name and password.
			// We get here waiting for the login to happen.
			LLButton* connect_btn = sInstance->getChild<LLButton>("connect_btn");
			connect_btn->setFocus(TRUE);
		}
	}
#endif
}

// static
void LLPanelLogin::show(const LLRect &rect,
						BOOL show_server,
						void (*callback)(S32 option, void* user_data),
						void* callback_data)
{
	new LLPanelLogin(rect, show_server, callback, callback_data);

	if( !gFocusMgr.getKeyboardFocus() )
	{
		// Grab focus and move cursor to first enabled control
		sInstance->setFocus(TRUE);
	}

	// Make sure that focus always goes here (and use the latest sInstance that was just created)
	gFocusMgr.setDefaultKeyboardFocus(sInstance);
}

// static
void LLPanelLogin::setFields(const std::string& firstname,
			     const std::string& lastname,
			     const std::string& password,
			     const LLSavedLogins& login_history)
{
	if (!sInstance)
	{
		llwarns << "Attempted fillFields with no login view shown" << llendl;
		return;
	}

	LLComboBox* login_combo = sInstance->getChild<LLComboBox>("first_name_combo");
	sInstance->childSetText("last_name_edit", lastname);

	llassert_always(firstname.find(' ') == std::string::npos);
	login_combo->setLabel(firstname);


	// OGPX : Are we guaranteed that LLAppViewer::instance exists already?
	if (gSavedSettings.getBOOL("OpenGridProtocol"))
	{
		LLComboBox* regioncombo = sInstance->getChild<LLComboBox>("regionuri_edit");
		
		// select which is displayed if we have a current URL.
		regioncombo->setSelectedByValue(LLSD(gSavedSettings.getString("CmdLineRegionURI")),TRUE);
	}

	// Max "actual" password length is 16 characters.
	// Hex digests are always 32 characters.
	if (password.length() == 32)
	{
		// This is a MD5 hex digest of a password.
		// We don't actually use the password input field, 
		// fill it with MAX_PASSWORD characters so we get a 
		// nice row of asterixes.
		const std::string filler("123456789!123456");
		sInstance->childSetText("password_edit", filler);
		sInstance->mIncomingPassword = filler;
		sInstance->mMungedPassword = password;
	}
	else
	{
		// this is a normal text password
		sInstance->childSetText("password_edit", password);
		sInstance->mIncomingPassword = password;
		LLMD5 pass((unsigned char *)password.c_str());
		char munged_password[MD5HEX_STR_SIZE];
		pass.hex_digest(munged_password);
		sInstance->mMungedPassword = munged_password;
	}
}

// static
void LLPanelLogin::setFields(const LLSavedLoginEntry& entry, bool takeFocus)
{
	if (!sInstance)
	{
		llwarns << "Attempted setFields with no login view shown" << llendl;
		return;
	}

	LLCheckBoxCtrl* remember_pass_check = sInstance->getChild<LLCheckBoxCtrl>("remember_check");
	LLComboBox* login_combo = sInstance->getChild<LLComboBox>("first_name_combo");
	login_combo->setLabel(entry.getFirstName());
	login_combo->resetDirty();
	login_combo->resetTextDirty();

	LLLineEditor* last_name = sInstance->getChild<LLLineEditor>("last_name_edit");
	last_name->setText(entry.getLastName());
	last_name->resetDirty();

	if (entry.getPassword().empty())
	{
		sInstance->childSetText("password_edit", std::string(""));
		remember_pass_check->setValue(LLSD(false));
	}
	else
	{
		const std::string filler("123456789!123456");
		sInstance->childSetText("password_edit", filler);
		sInstance->mIncomingPassword = filler;
		sInstance->mMungedPassword = entry.getPassword();
		remember_pass_check->setValue(LLSD(true));
	}

	LLComboBox* server_combo = sInstance->getChild<LLComboBox>("server_combo");
	if (server_combo->getSimple() != entry.getGridName())	// Avoid loops.
	{
		server_combo->setSimple(entry.getGridName());	// Same string as used in login_show().
	}

	LLViewerLogin* vl = LLViewerLogin::getInstance();

	if (entry.getGrid() == GRID_INFO_OTHER)
	{
		vl->setGridURI(entry.getGridURI().asString());
		vl->setHelperURI(entry.getHelperURI().asString());
		vl->setLoginPageURI(entry.getLoginPageURI().asString());
	}

	EGridInfo entry_grid = entry.getGrid();

	if (entry_grid == GRID_INFO_OTHER || entry_grid != vl->getGridChoice())
	{
		vl->setGridChoice(entry_grid);

		// grid changed so show new splash screen (possibly)
		loadLoginPage();
	}

	if (takeFocus)
	{
		giveFocus();
	}
}

// static
void LLPanelLogin::addServer(const std::string& server, S32 domain_name)
{
	if (!sInstance)
	{
		llwarns << "Attempted addServer with no login view shown" << llendl;
		return;
	}

	LLComboBox* combo = sInstance->getChild<LLComboBox>("server_combo");
	combo->add(server, LLSD(domain_name) );
	combo->setCurrentByIndex(0);
}

// static
void LLPanelLogin::getFields(std::string *firstname,
			     std::string *lastname,
			     std::string *password)
{
	if (!sInstance)
	{
		llwarns << "Attempted getFields with no login view shown" << llendl;
		return;
	}

	*firstname = sInstance->childGetText("first_name_combo");
	LLStringUtil::trim(*firstname);

	*lastname = sInstance->childGetText("last_name_edit");
	LLStringUtil::trim(*lastname);
    // OGPX : Nice up the uri string and save it.
	if (gSavedSettings.getBOOL("OpenGridProtocol"))
	{
		std::string regionuri = sInstance->childGetValue("regionuri_edit").asString();
		LLStringUtil::trim(regionuri);
		if (regionuri.find("://",0) == std::string::npos)
		{
			// if there wasn't a URI designation, assume http
			regionuri = "http://"+regionuri;
			llinfos << "Region URI was prepended, now " << regionuri << llendl;
		}
		gSavedSettings.setString("CmdLineRegionURI",regionuri);
		// add new uri to url history (don't need to add to combo box since it is recreated each login)
		LLURLHistory::appendToURLCollection("regionuri", regionuri);
	}

	*password = sInstance->mMungedPassword;
}

// static
BOOL LLPanelLogin::isGridComboDirty()
{
	BOOL user_picked = FALSE;
	if (!sInstance)
	{
		llwarns << "Attempted getServer with no login view shown" << llendl;
	}
	else
	{
		LLComboBox* combo = sInstance->getChild<LLComboBox>("server_combo");
		user_picked = combo->isDirty();
	}
	return user_picked;
}

// static
void LLPanelLogin::getLocation(std::string &location)
{
	if (!sInstance)
	{
		llwarns << "Attempted getLocation with no login view shown" << llendl;
		return;
	}
	
	LLComboBox* combo = sInstance->getChild<LLComboBox>("start_location_combo");
	location = combo->getValue().asString();
}

// static
void LLPanelLogin::refreshLocation( bool force_visible )
{
	if (!sInstance) return;

#if USE_VIEWER_AUTH
	loadLoginPage();
#else
	LLComboBox* combo = sInstance->getChild<LLComboBox>("start_location_combo");

	if (LLURLSimString::parse())
	{
		combo->setCurrentByIndex( 3 );		// BUG?  Maybe 2?
		combo->setTextEntry(LLURLSimString::sInstance.mSimString);
	}
	else
	{
		BOOL login_last = gSavedSettings.getBOOL("LoginLastLocation");
		combo->setCurrentByIndex( login_last ? 1 : 0 );
	}

	BOOL show_start = TRUE;

	if ( ! force_visible )
		show_start = gSavedSettings.getBOOL("ShowStartLocation");
		
	//MK
	if (gSavedSettings.getBOOL("RestrainedLove"))
		show_start = FALSE;
	//mk

	// OGPX : if --ogp on the command line (or --set OpenGridProtocol TRUE), then
	// the start location is hidden, and regionuri shows in its place. 
	// "Home", and "Last" have no meaning in OGPX, so it's OK to not have the start_location combo
	// box unavailable on the menu panel. 
	if (gSavedSettings.getBOOL("OpenGridProtocol"))
	{
		sInstance->childSetVisible("start_location_combo", FALSE); // hide legacy box
		sInstance->childSetVisible("start_location_text", TRUE);   // when OGPX always show location
		sInstance->childSetVisible("regionuri_edit",TRUE);         // show regionuri box if OGPX

	}
	else
	{
		sInstance->childSetVisible("start_location_combo", show_start); // maintain ShowStartLocation if legacy
		sInstance->childSetVisible("start_location_text", show_start);
		sInstance->childSetVisible("regionuri_edit",FALSE); // Do Not show regionuri box if legacy
	}

	sInstance->childSetVisible("server_combo", gSavedSettings.getBOOL("ForceShowGrid"));

#endif
}

// static
void LLPanelLogin::close()
{
	if (sInstance)
	{
		gViewerWindow->getRootView()->removeChild( LLPanelLogin::sInstance );
		
		gFocusMgr.setDefaultKeyboardFocus(NULL);

		delete sInstance;
		sInstance = NULL;
	}
}

// static
void LLPanelLogin::setAlwaysRefresh(bool refresh)
{
	if (LLStartUp::getStartupState() >= STATE_LOGIN_CLEANUP) return;

	LLWebBrowserCtrl* web_browser = sInstance->getChild<LLWebBrowserCtrl>("login_html");

	if (web_browser)
	{
		web_browser->setAlwaysRefresh(refresh);
	}
}



void LLPanelLogin::loadLoginPage()
{
	if (!sInstance) return;
	
	std::ostringstream oStr;

	LLViewerLogin* vl = LLViewerLogin::getInstance();
	std::string login_page = vl->getLoginPageURI();
	if (login_page.empty())
	{
		login_page = sInstance->getString( "real_url" );
		vl->setLoginPageURI(login_page);
	}
	oStr << login_page;
	
	// Use the right delimeter depending on how LLURI parses the URL
	LLURI login_page_uri = LLURI(login_page);
	std::string first_query_delimiter = "&";
	if (login_page_uri.queryMap().size() == 0)
	{
		first_query_delimiter = "?";
	}

	// Language
	std::string language = LLUI::getLanguage();
	oStr << first_query_delimiter<<"lang=" << language;
	
	// First Login?
	if (gSavedSettings.getBOOL("FirstLoginThisInstall"))
	{
		oStr << "&firstlogin=TRUE";
	}

	// Channel and Version
	std::string version = llformat("%d.%d.%d (%d)",
						LL_VERSION_MAJOR, LL_VERSION_MINOR, LL_VERSION_PATCH, LL_VIEWER_BUILD);

	char* curl_channel = curl_escape(gSavedSettings.getString("VersionChannelName").c_str(), 0);
	char* curl_version = curl_escape(version.c_str(), 0);

	oStr << "&channel=" << curl_channel;
	oStr << "&version=" << curl_version;

	curl_free(curl_channel);
	curl_free(curl_version);

	// Grid
	std::string label = vl->getGridLabel();
	LLStringUtil::toLower(label);
	if (label.find("secondlife") != std::string::npos || label.find("second life") != std::string::npos)
	{
		if (label.find("beta") != std::string::npos)
		{
			label = "aditi";
		}
		else
		{
			label = "agni";
		}
	}
	char* curl_grid = curl_escape(label.c_str(), 0);
	oStr << "&grid=" << curl_grid;
	curl_free(curl_grid);

	gViewerWindow->setMenuBackgroundColor(false, !vl->isInProductionGrid());
	vl->setMenuColor();
	gLoginMenuBarView->setBackgroundColor(gMenuBarView->getBackgroundColor());


#if USE_VIEWER_AUTH
	LLURLSimString::sInstance.parse();

	std::string location;
	std::string region;
	std::string password;
	
	if (LLURLSimString::parse())
	{
		std::ostringstream oRegionStr;
		location = "specify";
		oRegionStr << LLURLSimString::sInstance.mSimName << "/" << LLURLSimString::sInstance.mX << "/"
			 << LLURLSimString::sInstance.mY << "/"
			 << LLURLSimString::sInstance.mZ;
		region = oRegionStr.str();
	}
	else
	{
		if (gSavedSettings.getBOOL("LoginLastLocation"))
		{
			location = "last";
		}
		else
		{
			location = "home";
		}
	}
	
	std::string firstname, lastname;

    if(gSavedSettings.getLLSD("UserLoginInfo").size() == 3)
    {
        LLSD cmd_line_login = gSavedSettings.getLLSD("UserLoginInfo");
		firstname = cmd_line_login[0].asString();
		lastname = cmd_line_login[1].asString();
        password = cmd_line_login[2].asString();
    }
    	
	if (firstname.empty())
	{
		firstname = gSavedSettings.getString("FirstName");
	}
	
	if (lastname.empty())
	{
		lastname = gSavedSettings.getString("LastName");
	}
	
	char* curl_region = curl_escape(region.c_str(), 0);

	oStr <<"firstname=" << firstname <<
		"&lastname=" << lastname << "&location=" << location <<	"&region=" << curl_region;
	
	curl_free(curl_region);

	if (!password.empty())
	{
		oStr << "&password=" << password;
	}
	else if (!(password = load_password_from_disk()).empty())
	{
		oStr << "&password=$1$" << password;
	}
	if (gAutoLogin)
	{
		oStr << "&auto_login=TRUE";
	}
	if (gSavedSettings.getBOOL("ShowStartLocation") && !gSavedSettings.getBOOL( "RestrainedLove"))
	{
		oStr << "&show_start_location=TRUE";
	}	
	if (gSavedSettings.getBOOL("RememberPassword"))
	{
		oStr << "&remember_password=TRUE";
	}	
#ifndef	LL_RELEASE_FOR_DOWNLOAD
	oStr << "&show_grid=TRUE";
#else
	if (gSavedSettings.getBOOL("ForceShowGrid"))
		oStr << "&show_grid=TRUE";
#endif
#endif
	
	LLWebBrowserCtrl* web_browser = sInstance->getChild<LLWebBrowserCtrl>("login_html");
	
	// navigate to the "real" page 
	web_browser->navigateTo(oStr.str());
}

void LLPanelLogin::onNavigateComplete( const EventType& eventIn )
{
	LLWebBrowserCtrl* web_browser = sInstance->getChild<LLWebBrowserCtrl>("login_html");
	if (web_browser)
	{
		// *HACK HACK HACK HACK!
		/* Stuff a Tab key into the browser now so that the first field will
		** get the focus!  The embedded javascript on the page that properly
		** sets the initial focus in a real web browser is not working inside
		** the viewer, so this is an UGLY HACK WORKAROUND for now.
		*/
		// Commented out as it's not reliable
		//web_browser->handleKey(KEY_TAB, MASK_NONE, false);
	}
}

bool LLPanelLogin::getRememberLogin()
{
	bool remember = false;

	if (sInstance)
	{
		LLCheckBoxCtrl* remember_login = sInstance->getChild<LLCheckBoxCtrl>("remember_name_check");
		if (remember_login)
		{
			remember = remember_login->getValue().asBoolean();
		}
	}
	else
	{
		llwarns << "Attempted to query rememberLogin with no login view shown" << llendl;
	}

	return remember;
}

// static
void LLPanelLogin::selectFirstElement(void)
{
	sInstance->getChild<LLComboBox>("server_combo")->setCurrentByIndex(0);
	LLPanelLogin::onSelectServer(NULL, NULL);
}

//---------------------------------------------------------------------------
// Protected methods
//---------------------------------------------------------------------------

// static
void LLPanelLogin::onClickConnect(void *)
{
	if (sInstance && sInstance->mCallback)
	{
		// tell the responder we're not here anymore
		if ( gResponsePtr )
			gResponsePtr->setParent( 0 );

		// JC - Make sure the fields all get committed.
		sInstance->setFocus(FALSE);

		std::string first = sInstance->childGetText("first_name_combo");
		std::string last  = sInstance->childGetText("last_name_edit");
		if (!first.empty() && !last.empty())
		{
			// has both first and last name typed
			sInstance->mCallback(0, sInstance->mCallbackData);
		}
		else
		{
			gViewerWindow->alertXml("MustHaveAccountToLogIn",
									LLPanelLogin::newAccountAlertCallback);
		}
	}
}


// static
void LLPanelLogin::newAccountAlertCallback(S32 option, void*)
{
	sInstance->setFocus(TRUE);
	return false;
}


// static
void LLPanelLogin::onClickNewAccount(void*)
{
	LLWeb::loadURLExternal( CREATE_ACCOUNT_URL );
}


// *NOTE: This function is dead as of 2008 August.  I left it here in case
// we suddenly decide to put the Quit button back. JC
// static
void LLPanelLogin::onClickQuit(void*)
{
	if (sInstance && sInstance->mCallback)
	{
		// tell the responder we're not here anymore
		if ( gResponsePtr )
			gResponsePtr->setParent( 0 );

		sInstance->mCallback(1, sInstance->mCallbackData);
	}
}


// static
void LLPanelLogin::onClickVersion(void*)
{
	LLFloaterAbout::show(NULL);
}

//static
void LLPanelLogin::onClickForgotPassword(void*)
{
	if (sInstance )
	{
		LLWeb::loadURLExternal(sInstance->getString( "forgot_password_url" ));
	}
}

// static
void LLPanelLogin::onPassKey(LLLineEditor* caller, void* user_data)
{
	if (gKeyboard->getKeyDown(KEY_CAPSLOCK) && sCapslockDidNotification == FALSE)
	{
		LLNotifyBox::showXml("CapsKeyOn");
		sCapslockDidNotification = TRUE;
	}
}

// static
void LLPanelLogin::onSelectServer(LLUICtrl*, void*)
{
	// *NOTE: The parameters for this method are ignored.

	// This function is only called by one thread, so we can use a static here.
	static bool looping;
	if (looping) return;
	looping = true;

	// LLPanelLogin::onServerComboLostFocus(LLFocusableElement* fe, void*)
	// calls this method.

	// The user twiddled with the grid choice ui.
	// apply the selection to the grid setting.
	std::string grid_name;
	EGridInfo grid_index;

	LLComboBox* combo = sInstance->getChild<LLComboBox>("server_combo");
	LLSD combo_val = combo->getValue();

	if (LLSD::TypeInteger == combo_val.type())
	{
		grid_index = (EGridInfo)combo->getValue().asInteger();
		grid_name = combo->getSimple();
	}
	else
	{
		// no valid selection, return other
		grid_index = GRID_INFO_OTHER;
		grid_name = combo_val.asString();
	}

	// This new selection will override preset uris
	// from the command line.
	LLViewerLogin* vl = LLViewerLogin::getInstance();

	if (grid_index != GRID_INFO_OTHER)
	{
		vl->setGridChoice(grid_index);
	}
	else
	{
		vl->setGridChoice(grid_name);
	}

	// Get the newly selected and properly formatted grid name.
	grid_name = vl->getGridLabel();

	// Find a saved login entry that uses this grid, if any.
	bool found = false;
	LLSavedLoginsList const& entries = sInstance->mLoginHistoryData.getEntries();
	for (LLSavedLoginsList::const_reverse_iterator i = entries.rbegin(); i != entries.rend(); ++i)
	{
		if (!i->asLLSD().isMap())
		{
			continue;
		}
		if (i->getGridName() == grid_name)
		{
		  	if (!vl->nameEditted())
			{
				// Change the other fields to match this grid.
				LLPanelLogin::setFields(*i, false);
			}
			else	// Probably creating a new account.
			{
				// Likely the current password is for a different grid.
				clearPassword();
			}
			found = true;
			break;
		}
	}
	if (!found)
	{
		clearPassword();

		// If the grid_name starts with 'http[s]://' then
		// we have to assume it's a new loginuri, set
		// on the commandline.
		if (grid_name.substr(0, 4) == "http")
		{
			// Use it as login uri.
			vl->setGridURI(grid_name);
			// And set the login page if it was given.
			std::string loginPage = gSavedSettings.getString("LoginPage");
			std::string helperURI = gSavedSettings.getString("CmdLineHelperURI");
			if (!loginPage.empty()) vl->setLoginPageURI(loginPage);
			if (!helperURI.empty()) vl->setHelperURI(helperURI);
		}
	}

	// grid changed so show new splash screen (possibly)
	loadLoginPage();

	looping = false;
}

void LLPanelLogin::onServerComboLostFocus(LLFocusableElement* fe, void*)
{
	if( !sInstance )
	{
		return;
	}
	
	LLComboBox* combo = sInstance->getChild<LLComboBox>("server_combo");
	if(fe == combo)
	{
		onSelectServer(combo, NULL);	
	}
}

// static
void LLPanelLogin::onLastNameEditLostFocus(LLUICtrl* ctrl, void* data)
{
	if (sInstance)
	{
		LLLineEditor* edit = sInstance->getChild<LLLineEditor>("last_name_edit");
		if(ctrl == edit)
		{
			if (edit->isDirty())
			{
				clearPassword();
				LLViewerLogin::getInstance()->setNameEditted(true);
			}
		}
	}
}

// static
void LLPanelLogin::onSelectLoginEntry(LLUICtrl* ctrl, void* data)
{
	if (sInstance)
	{
		LLComboBox* combo = sInstance->getChild<LLComboBox>("first_name_combo");
		if (ctrl == combo)
		{
			LLSD selected_entry = combo->getSelectedValue();
			if (!selected_entry.isUndefined())
			{
				LLSavedLoginEntry entry(selected_entry);
				setFields(entry);
			}
			// This stops the automatic matching of the first name to a selected grid.
			LLViewerLogin::getInstance()->setNameEditted(true);
		}
	}
}

// static
void LLPanelLogin::onLoginComboLostFocus(LLFocusableElement* fe, void*)
{
	if (sInstance)
	{
		LLComboBox* combo = sInstance->getChild<LLComboBox>("first_name_combo");
		if(fe == combo)
		{
			if (combo->isTextDirty())
			{
				clearPassword();
			}
			onSelectLoginEntry(combo, NULL);
		}
	}
}

// static
void LLPanelLogin::onNameCheckChanged(LLUICtrl* ctrl, void* data)
{
	if (sInstance)
	{
		LLCheckBoxCtrl* remember_login_check = sInstance->getChild<LLCheckBoxCtrl>("remember_name_check");
		LLCheckBoxCtrl* remember_pass_check = sInstance->getChild<LLCheckBoxCtrl>("remember_check");
		if (remember_login_check && remember_pass_check)
		{
			if (remember_login_check->getValue().asBoolean())
			{
				remember_pass_check->setEnabled(true);
			}
			else
			{
				remember_pass_check->setValue(LLSD(false));
				remember_pass_check->setEnabled(false);
			}
		}
	}
}

// static
void LLPanelLogin::clearPassword()
{
	std::string blank;
	sInstance->childSetText("password_edit", blank);
	sInstance->mIncomingPassword = blank;
	sInstance->mMungedPassword = blank;
}
