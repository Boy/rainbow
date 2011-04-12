/** 
 * @file llviewernetwork.h
 * @author James Cook
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

#ifndef LL_LLVIEWERNETWORK_H
#define LL_LLVIEWERNETWORK_H

#define DYNAMIC_GRIDS

class LLHost;
class LLSD;

#define EGridInfo int

/**
 * @brief A class to manage the viewer's login state.
 * 
 **/
class LLViewerLogin : public LLSingleton<LLViewerLogin>
{
public:
	LLViewerLogin();

	void setGridChoice(EGridInfo grid);
	void setGridChoice(const std::string& grid_name);
	void setGridURI(const std::string& uri);
	void setGridURIs(const std::vector<std::string>& urilist);

	/**
	* @brief Get the enumeration of the grid choice.
	* Should only return values > 0 && <= GRID_INFO_OTHER
	**/
	EGridInfo getGridChoice() const { return mGridChoice; }

	/**
	* @brief Get a readable label for the grid choice.
	* Returns the readable name for the grid choice. 
	* If the grid is 'other', returns something
	* the string used to specifiy the grid.
	**/
	std::string getGridLabel();

	std::string getKnownGridLabel(EGridInfo grid_index) const; 
	const std::string getStaticGridURI(const EGridInfo grid) const;
	const std::string getStaticGridHelperURI(const EGridInfo grid) const;

	const std::string getCurrentGridURI();
	bool tryNextURI();

	const std::vector<std::string>& getCommandLineURIs();
	const std::vector<std::string>& getGridURIs();
	const std::string getHelperURI() const;
	void setHelperURI(const std::string& uri);
	const std::string getLoginPageURI() const;
	void setLoginPageURI(const std::string& uri);
	void setNameEditted(bool value) { mNameEditted = value; }

	bool isInProductionGrid();
	bool nameEditted(void) const { return mNameEditted; }

	void setMenuColor() const;
	
	void loadGridsLLSD(std::string filename);
	
private:
	void parseCommandLineURIs();

	LLSD mGridList;
	EGridInfo mGridChoice;
	std::string mGridName;
	std::string mHelperURI;
	std::string mLoginPageURI;
	std::vector<std::string> mCommandLineURIs;
	std::vector<std::string> mGridURIs;

	int mCurrentURI;	// Index into mGridURIs.
	bool mNameEditted;	// Set if the user edits/sets the First or Last name field.
};

const EGridInfo DEFAULT_GRID_CHOICE = 1;
const EGridInfo GRID_INFO_NONE = 0;
extern EGridInfo GRID_INFO_OTHER;

const S32 MAC_ADDRESS_BYTES = 6;
extern unsigned char gMACAddress[MAC_ADDRESS_BYTES];		/* Flawfinder: ignore */

#endif
