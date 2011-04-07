/** 
 * @file llviewernetwork.cpp
 * @author James Cook, Richard Nelson
 * @brief Networking constants and globals for viewer.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llviewernetwork.h"
#include "llviewercontrol.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llviewermenu.h"

unsigned char gMACAddress[MAC_ADDRESS_BYTES];		/* Flawfinder: ignore */

const EGridInfo DEFAULT_GRID_CHOICE = 1;
EGridInfo GRID_INFO_OTHER;

LLViewerLogin::LLViewerLogin() :
	mGridChoice(DEFAULT_GRID_CHOICE)
{
	LLSD array = mGridList.emptyArray();
	LLSD entry = mGridList.emptyMap();
	entry.insert("label", "None");
	entry.insert("name", "");
	entry.insert("login_uri", "");
	entry.insert("helper_uri", "");
	entry.insert("login_page", "");
	array.append(entry);
	// Add SecondLife servers (main and beta grid):
	entry = mGridList.emptyMap();
	entry.insert("label", "SecondLife");
	entry.insert("name", "agni.lindenlab.com");
	entry.insert("login_uri", "https://login.agni.lindenlab.com/cgi-bin/login.cgi");
	entry.insert("helper_uri", "https://secondlife.com/helpers/");
	entry.insert("login_page", "http://secondlife.com/app/login/");
	array.append(entry);
	entry = mGridList.emptyMap();
	entry.insert("label", "SecondLife Beta");
	entry.insert("name", "aditi.lindenlab.com");
	entry.insert("login_uri", "https://login.aditi.lindenlab.com/cgi-bin/login.cgi");
	entry.insert("helper_uri", "http://aditi-secondlife.webdev.lindenlab.com/helpers/");
	entry.insert("login_page", "http://secondlife.com/app/login/");
	array.append(entry);

	mGridList.insert("grids", array);

	// load the alternate grids if available
	loadGridsLLSD(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "grids.xml"));
	// see if we have a grids_custom.xml file to append
	loadGridsLLSD(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "grids_custom.xml"));

	entry = mGridList.emptyMap();
	entry.insert("label", "Other");
	entry.insert("name", "");
	entry.insert("login_uri", "");
	entry.insert("helper_uri", "");
	mGridList["grids"].append(entry);

	GRID_INFO_OTHER = (EGridInfo)mGridList.get("grids").size() - 1;
}

void LLViewerLogin::loadGridsLLSD(std::string xml_filename)
{
	LLSD other_grids;
	llifstream llsd_xml;
	llsd_xml.open(xml_filename.c_str(), std::ios::in | std::ios::binary);

	if (llsd_xml.is_open())
	{
		llinfos << "Reading grid info: " << xml_filename << llendl;
		LLSDSerialize::fromXML(other_grids, llsd_xml);
		for (LLSD::map_iterator grid_itr = other_grids.beginMap(); 
			 grid_itr != other_grids.endMap(); grid_itr++)
		{
			LLSD::String key_name = grid_itr->first;
			LLSD grid_array = grid_itr->second;
			llinfos << "reading: " << key_name << llendl;
			if (grid_array.isArray())
			{
				for (int i = 0; i < grid_array.size(); i++)
				{
					LLSD gmap = grid_array[i];
					if (gmap.has("name") && gmap.has("label") && 
						gmap.has("login_uri") && gmap.has("helper_uri"))
					{
						mGridList["grids"].append(gmap);
						llinfos << "Added grid: " << gmap.get("name") << llendl;
					}
					else
					{
						if (gmap.has("name"))
						{
							llwarns << "Incomplete grid definition: " << gmap.get("name") << llendl;													
						}
						else
						{
							llwarns << "Incomplete grid definition: no name specified" << llendl;					
						}
					}
				}
			}
			else
			{
				llwarns << "\"grids\" is not an array" << llendl;										
			}
		}
		llsd_xml.close();
	}	
}

void LLViewerLogin::setMenuColor() const
{
	if (mGridList["grids"][mGridChoice].has("menu_color"))
	{
		std::string colorName = mGridList["grids"][mGridChoice].get("menu_color").asString();
		LLColor4 color4;
		LLColor4::parseColor(colorName.c_str(), &color4);
		if (color4 != LLColor4::black)
		{
			gMenuBarView->setBackgroundColor(color4);			
		}
	}
}

void LLViewerLogin::setGridChoice(EGridInfo grid)
{	
	if (grid < 0 || grid > GRID_INFO_OTHER)
  	{
		llerrs << "Invalid grid index specified." << llendl;
  	}

	mGridChoice = grid;
	std::string name = mGridList.get("grids")[grid].get("label").asString();
	LLStringUtil::toLower(name);
	if (name.find("local") == 0)
	{
		mGridName = LOOPBACK_ADDRESS_STRING;
	}
	else if (name == "other")
	{
		// *FIX:Mani - could this possibly be valid?
		mGridName = "other";
	}
	else
	{
		mGridName = mGridList.get("grids")[mGridChoice].get("label").asString();
	}

	gSavedSettings.setS32("ServerChoice", mGridChoice);
	gSavedSettings.setString("CustomServer", mGridName);
}

void LLViewerLogin::setGridChoice(const std::string& grid_name)
{
	// Set the grid choice based on a string.
	// The string can be:
	// - a grid label or a name from the known grids list
	// - an ip address
	if (!grid_name.empty())
	{
		// find the grid choice from the user setting.
		std::string pattern(grid_name);
		LLStringUtil::toLower(pattern);
		for (EGridInfo grid_index = GRID_INFO_NONE; grid_index < GRID_INFO_OTHER; grid_index++)
		{
			std::string label = mGridList["grids"][grid_index].get("label").asString();
			std::string name = mGridList["grids"][grid_index].get("name").asString();
			LLStringUtil::toLower(label);
			LLStringUtil::toLower(name);
			if (label.find(pattern) == 0 || name.find(pattern) == 0)
			{
				// Found a matching label in the list...
				setGridChoice(grid_index);
				return;
			}
		}

		// *FIX:MEP Can and should we validate that this is an IP address?
		mGridChoice = GRID_INFO_OTHER;
		mGridName = grid_name;
		gSavedSettings.setS32("ServerChoice", mGridChoice);
		gSavedSettings.setString("CustomServer", mGridName);
	}
}

std::string LLViewerLogin::getGridLabel() const
{
	if(mGridChoice == GRID_INFO_NONE)
	{
		return "None";
	}
	else if(mGridChoice < GRID_INFO_OTHER)
	{
		return mGridList["grids"][mGridChoice].get("label").asString();
	}

	return mGridName;
}

std::string LLViewerLogin::getLoginPageURI() const
{
	if (mGridChoice == GRID_INFO_NONE)
	{
		return "";
	}
	else if (mGridChoice < GRID_INFO_OTHER)
	{
		return mGridList["grids"][mGridChoice].get("login_page").asString();
	}

	return "";
}

std::string LLViewerLogin::getKnownGridLabel(EGridInfo grid) const
{
	if( grid > GRID_INFO_NONE && grid < GRID_INFO_OTHER)
	{
		return mGridList.get("grids")[grid].get("label").asString();
	}
	return mGridList.get("grids")[GRID_INFO_NONE].get("label").asString();
}

void LLViewerLogin::resetURIs()
{
    // Clear URIs when picking a new server
	gSavedSettings.setValue("CmdLineLoginURI", LLSD::emptyArray());
	gSavedSettings.setString("CmdLineHelperURI", "");
}

void LLViewerLogin::getLoginURIs(std::vector<std::string>& uris) const
{
	// return the login uri set on the command line.
	LLControlVariable* c = gSavedSettings.getControl("CmdLineLoginURI");
	if(c)
	{
		LLSD v = c->getValue();
		if(v.isArray())
		{
			for(LLSD::array_const_iterator itr = v.beginArray();
				itr != v.endArray(); ++itr)
			{
				std::string uri = itr->asString();
				if(!uri.empty())
				{
					uris.push_back(uri);
				}
			}
		}
		else
		{
			std::string uri = v.asString();
			if(!uri.empty())
			{
				uris.push_back(uri);
			}
		}
	}

	// If there was no command line uri...
	if(uris.empty())
	{
		// If its a known grid choice, get the uri from the table,
		// else try the grid name.
		if(mGridChoice > GRID_INFO_NONE && mGridChoice < GRID_INFO_OTHER)
		{
			uris.push_back(mGridList["grids"][mGridChoice].get("login_uri").asString());
		}
		else
		{
			uris.push_back(mGridName);
		}
	}
}

std::string LLViewerLogin::getHelperURI() const
{
	std::string helper_uri = gSavedSettings.getString("CmdLineHelperURI");
	if (helper_uri.empty())
	{
		// grab URI from selected grid
		if(mGridChoice > GRID_INFO_NONE && mGridChoice < GRID_INFO_OTHER)
		{
			helper_uri = mGridList["grids"][mGridChoice].get("helper_uri").asString();
		}

		if (helper_uri.empty())
		{
			// what do we do with unnamed/miscellaneous grids?
			// for now, operations that rely on the helper URI (currency/land purchasing) will fail
			llwarns << "Missing Helper URI for this grid ! Currency/land purchasing) will fail..." << llendl;
		}
	}
	return helper_uri;
}

bool LLViewerLogin::isInProductionGrid()
{
	std::vector<std::string> uris;
	getLoginURIs(uris);
	LLStringUtil::toLower(uris[0]);
	return ((uris[0].find("aditi") == std::string::npos));
}
