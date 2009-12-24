/**
 * @file llfloaterurlentry.cpp
 * @brief LLFloaterURLEntry class implementation
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llfloaterurlentry.h"

#include "llpanellandmedia.h"

// project includes
#include "llcombobox.h"
#include "llurlhistory.h"
#include "lluictrlfactory.h"
#include "llwindow.h"
#include "llviewerwindow.h"

static LLFloaterURLEntry* sInstance = NULL;

// Move this to its own file.
// helper class that tries to download a URL from a web site and calls a method
// on the Panel Land Media and to discover the MIME type
class LLMediaTypeResponder : public LLHTTPClient::Responder
{
public:
	LLMediaTypeResponder( const LLHandle<LLFloater> parent ) :
	  mParent( parent )
	  {}

	  LLHandle<LLFloater> mParent;


	  virtual void completedHeader(U32 status, const std::string& reason, const LLSD& content)
	  {
		  std::string media_type = content["content-type"].asString();
		  std::string::size_type idx1 = media_type.find_first_of(";");
		  std::string mime_type = media_type.substr(0, idx1);
		  completeAny(status, mime_type);
	  }

	  virtual void error( U32 status, const std::string& reason )
	  {
		  completeAny(status, "none/none");
	  }

	  void completeAny(U32 status, const std::string& mime_type)
	  {
		  // Set empty type to none/none.  Empty string is reserved for legacy parcels
		  // which have no mime type set.
		  std::string resolved_mime_type = ! mime_type.empty() ? mime_type : "none/none";
		  LLFloaterURLEntry* floater_url_entry = (LLFloaterURLEntry*)mParent.get();
		  if ( floater_url_entry )
			  floater_url_entry->headerFetchComplete( status, resolved_mime_type );
	  }
};

//-----------------------------------------------------------------------------
// LLFloaterURLEntry()
//-----------------------------------------------------------------------------
LLFloaterURLEntry::LLFloaterURLEntry(LLHandle<LLPanel> parent)
	:
	LLFloater(),
	mPanelLandMediaHandle(parent)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_url_entry.xml");

	mMediaURLEdit = getChild<LLComboBox>("media_entry");

	// Cancel button
	childSetAction("cancel_btn", onBtnCancel, this);

	// Cancel button
	childSetAction("clear_btn", onBtnClear, this);

	// clear media list button
	LLSD parcel_history = LLURLHistory::getURLHistory("parcel");
	bool enable_clear_button = parcel_history.size() > 0 ? true : false;
	childSetEnabled( "clear_btn", enable_clear_button );

	// OK button
	childSetAction("ok_btn", onBtnOK, this);

	setDefaultBtn("ok_btn");
	buildURLHistory();

	sInstance = this;
}

//-----------------------------------------------------------------------------
// ~LLFloaterURLEntry()
//-----------------------------------------------------------------------------
LLFloaterURLEntry::~LLFloaterURLEntry()
{
	sInstance = NULL;
}

void LLFloaterURLEntry::buildURLHistory()
{
	LLCtrlListInterface* url_list = childGetListInterface("media_entry");
	if (url_list)
	{
		url_list->operateOnAll(LLCtrlListInterface::OP_DELETE);
	}

	// Get all of the entries in the "parcel" collection
	LLSD parcel_history = LLURLHistory::getURLHistory("parcel");

	LLSD::array_iterator iter_history =
		parcel_history.beginArray();
	LLSD::array_iterator end_history =
		parcel_history.endArray();
	for(; iter_history != end_history; ++iter_history)
	{
		url_list->addSimpleElement((*iter_history).asString());
	}
}

void LLFloaterURLEntry::headerFetchComplete(U32 status, const std::string& mime_type)
{
	LLPanelLandMedia* panel_media = (LLPanelLandMedia*)mPanelLandMediaHandle.get();
	if (panel_media)
	{
		// status is ignored for now -- error = "none/none"
		panel_media->setMediaType(mime_type);
		panel_media->setMediaURL(mMediaURLEdit->getValue().asString());
	}
	// Decrement the cursor
	getWindow()->decBusyCount();
	childSetVisible("loading_label", false);
	close();
}

// static
LLHandle<LLFloater> LLFloaterURLEntry::show(LLHandle<LLPanel> parent)
{
	if (sInstance)
	{
		sInstance->open();
	}
	else
	{
		sInstance = new LLFloaterURLEntry(parent);
	}
	sInstance->updateFromLandMediaPanel();
	return sInstance->getHandle();
}

void LLFloaterURLEntry::updateFromLandMediaPanel()
{
	LLPanelLandMedia* panel_media = (LLPanelLandMedia*)mPanelLandMediaHandle.get();
	if (panel_media)
	{
		std::string media_url = panel_media->getMediaURL();
		addURLToCombobox(media_url);
	}
}

bool LLFloaterURLEntry::addURLToCombobox(const std::string& media_url)
{
	if(! mMediaURLEdit->setSimple( media_url ) && ! media_url.empty())
	{
		mMediaURLEdit->add( media_url );
		mMediaURLEdit->setSimple( media_url );
		return true;
	}

	// URL was not added for whatever reason (either it was empty or already existed)
	return false;
}

// static
//-----------------------------------------------------------------------------
// onBtnOK()
//-----------------------------------------------------------------------------
void LLFloaterURLEntry::onBtnOK( void* userdata )
{
	LLFloaterURLEntry *self =(LLFloaterURLEntry *)userdata;

	std::string media_url	= self->mMediaURLEdit->getValue().asString();
	self->mMediaURLEdit->remove(media_url);
	LLURLHistory::removeURL("parcel", media_url);
	if(self->addURLToCombobox(media_url))
	{
		// Add this url to the parcel collection
		LLURLHistory::addURL("parcel", media_url);
	}

	// leading whitespace causes problems with the MIME-type detection so strip it
	LLStringUtil::trim( media_url );

	// First check the URL scheme
	LLURI url(media_url);
	std::string scheme = url.scheme();

	// We assume that an empty scheme is an http url, as this is how we will treat it.
	if(scheme == "")
	{
		scheme = "http";
	}

	// Discover the MIME type only for "http" scheme.
	if(scheme == "http")
	{
		LLHTTPClient::getHeaderOnly( media_url,
			new LLMediaTypeResponder(self->getHandle()));
	}
	else
	{
		self->headerFetchComplete(0, scheme);
	}

	// Grey the buttons until we get the header response
	self->childSetEnabled("ok_btn", false);
	self->childSetEnabled("cancel_btn", false);
	self->childSetEnabled("media_entry", false);

	// show progress bar here?
	getWindow()->incBusyCount();
	self->childSetVisible("loading_label", true);
}

// static
//-----------------------------------------------------------------------------
// onBtnCancel()
//-----------------------------------------------------------------------------
void LLFloaterURLEntry::onBtnCancel( void* userdata )
{
	LLFloaterURLEntry *self =(LLFloaterURLEntry *)userdata;
	self->close();
}

// static
//-----------------------------------------------------------------------------
// onBtnClear()
//-----------------------------------------------------------------------------
void LLFloaterURLEntry::onBtnClear( void* userdata )
{
	gViewerWindow->alertXml( "ConfirmClearMediaUrlList", callback_clear_url_list, userdata );
}

void LLFloaterURLEntry::callback_clear_url_list(S32 option, void* userdata)
{
	if ( option == 0 ) // YES
	{
		LLFloaterURLEntry *self =(LLFloaterURLEntry *)userdata;

		if ( self )
		{
			// clear saved list
			LLCtrlListInterface* url_list = self->childGetListInterface("media_entry");
			if ( url_list )
			{
				url_list->operateOnAll( LLCtrlListInterface::OP_DELETE );
			}

			// clear current contents of combo box
			self->mMediaURLEdit->clear();

			// clear stored version of list
			LLURLHistory::clear("parcel");

			// cleared the list so disable Clear button
			self->childSetEnabled( "clear_btn", false );
		}
	}
}
