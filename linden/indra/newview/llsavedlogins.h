/**
 * @file llsavedlogins.h
 * @brief Manages a list of previous successful logins
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 *
 * Copyright (c) 2009, Linden Research, Inc.
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
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#ifndef LLLOGINHISTORY_H
#define LLLOGINHISTORY_H

#include "llviewernetwork.h"

class LLSD;

/**
 * @brief	Holds data for a single login attempt.
 */
class LLSavedLoginEntry
{
public:
	/**
	 * @brief	Constructs a history entry from an appropriate LLSD.
	 * @param	LLSD containing serialized LLSavedLoginEntry.
	 * @throw	invalid_argument	If the LLSD is null or does not contain the required fields.
	 */
	LLSavedLoginEntry(const LLSD& entry_data);
	/**
	 * @brief	Constructs a history entry from individual fields.
	 * @param	gridinfo	Grid as EGridInfo enumeration.
	 * @param	firstname	Resident first name.
	 * @param	lastname	Resident last name.
	 * @param	password	Munged password of PASSWORD_HASH_LENGTH.
	 */
	LLSavedLoginEntry(const EGridInfo gridinfo, const std::string& firstname,
					  const std::string& lastname, const std::string& password);
	/**
	 * @brief	Returns the display name of the grid ID associated with this entry.
	 * @return	String containing grid name.
	 */
	const std::string getGridName() const;

	/**
	 * @brief	Gets the grid ID associated with this entry.
	 * @return	EGridInfo enumeration corresponding to grid.
	 */
	const EGridInfo getGrid() const;

	/**
	 * @brief	Sets the grid associated with the entry.
	 */
	void setGrid(EGridInfo grid)
	{
		mEntry.insert("grid", LLViewerLogin::getInstance()->getKnownGridLabel(grid));
	}
	/**
	 * @brief	Gets the grid URI associated with the entry, if any.
	 */
	const LLURI getGridURI() const
	{
		return (mEntry.has("griduri") ? mEntry.get("griduri").asURI() : LLURI());
	}
	/**
	 * @brief	Sets the grid URI associated with the entry.
	 */
	void setGridURI(const LLURI& uri)
	{
		mEntry.insert("griduri", uri);
	}
	/**
	 * @brief	Gets the login page URI associated with the entry, if any.
	 */
	const LLURI getLoginPageURI() const
	{
		return (mEntry.has("loginpageuri") ? mEntry.get("loginpageuri").asURI() : LLURI());
	}
	/**
	 * @brief	Sets the login page URI associated with the entry.
	 */
	void setLoginPageURI(const LLURI& uri)
	{
		mEntry.insert("loginpageuri", uri);
	}
	/**
	 * @brief	Gets the helper URI associated with the entry, if any.
	 */
	const LLURI getHelperURI() const
	{
		return (mEntry.has("helperuri") ? mEntry.get("helperuri").asURI() : LLURI());
	}
	/**
	 * @brief	Sets the helper URI associated with the entry.
	 */
	void setHelperURI(const LLURI& uri)
	{
		mEntry.insert("helperuri", uri);
	}
	/**
	 * @brief	Returns the first name associated with this login entry.
	 * @return	First name as string.
	 */
	const std::string getFirstName() const
	{
		return (mEntry.has("firstname") ? mEntry.get("firstname").asString() : std::string());
	}
	/**
	 * @brief	Sets the first name associated with this login entry.
	 * @param	value	String value to set.
	 */
	void setFirstName(std::string& value)
	{
		mEntry.insert("firstname", LLSD(value));
	}
	/**
	 * @brief	Returns the last name associated with this login entry.
	 * @return	Last name as string.
	 */
	const std::string getLastName() const
	{
		return (mEntry.has("lastname") ? mEntry.get("lastname").asString() : std::string());
	}
	/**
	 * @brief	Sets the last name associated with this login entry.
	 * @param	value	String value to set.
	 */
	void setLastName(std::string& value)
	{
		mEntry.insert("lastname", LLSD(value));
	}
	/**
	 * @brief	Returns the password associated with this entry.
	 * @note	The password is stored encrypted, but will be returned as
	 *			a plain-text, pre-munged string of PASSWORD_HASH_LENGTH.
	 * @return	Munged password suitable for login.
	 */
	const std::string getPassword() const;
	/**
	 * @brief	Sets the password associated with this entry.
	 * @note	The password is stored with system-specific encryption
	 *			internally. It must be supplied to this method as a
	 *			munged string of PASSWORD_HASH_LENGTH.
	 * @param	value	Munged password suitable for login.
	 */
	void setPassword(const std::string& value);
	/**
	 * @brief	Returns the login entry as an LLSD for serialization.
	 * *return	LLSD containing login entry details.
	 */
	LLSD asLLSD() const;
	/**
	 * @brief	Provides a string containing the username and grid for display.
	 * @return	Formatted string with login details.
	 */
	const std::string getDisplayString() const;
	static const size_t PASSWORD_HASH_LENGTH = 32;
private:
	static const std::string decryptPassword(const LLSD& pwdata);
	static const LLSD encryptPassword(const std::string& password);
	LLSD mEntry;
};

typedef std::list<LLSavedLoginEntry> LLSavedLoginsList;

/**
 * @brief Holds a user's login history.
 */
class LLSavedLogins
{
public:
	/**
	 * @brief	Constructs an empty login history.
	 */
	LLSavedLogins();
	/**
	 * @brief	Constructs a login history from an LLSD array of history entries.
	 * @param	LLSD containing serialized history data.
	 * @throw	invalid_argument	If the LLSD is not in array form.
	 */
	LLSavedLogins(const LLSD& history_data);
	/**
	 * @brief	Add a new login history entry.
	 * @param	Entry to add.
	 */
	void addEntry(const LLSavedLoginEntry& entry);
	/**
	 * @brief	Deletes a login history entry by looking up its name and grid.
	 * @param	grid	EGridInfo enumeration of the grid.
	 * @param	firstname	First name to find and delete.
	 * @param	lastname	Last name to find and delete.
	 * @param	griduri		Full URI if grid is GRID_INFO_OTHER.
	 */
	void deleteEntry(const EGridInfo grid, const std::string& firstname, const std::string& lastname, const std::string& griduri);
	/**
	 * @brief	Access internal vector of login entries from the history.
	 * @return	Const reference to internal login history storage.
	 */
	const LLSavedLoginsList& getEntries() const { return mEntries; }
	/**
	 * @brief	Return the login history as an LLSD for serialization.
	 * @return	LLSD containing history data.
	 */
	LLSD asLLSD() const;
	/**
	 * @brief	Get the count of login entries in the history.
	 * @return	Count of login entries.
	 */
	const size_t size() const {	return mEntries.size(); }
	/**
	 * @brief	Loads a login history object from disk.
	 * @param	filepath	Absolute path of history file.
	 * @return	History object; if the load failed, the history will be empty.
	 */
	static LLSavedLogins loadFile(const std::string& filepath);
	/**
	 * @brief	Saves a login history object to an absolute path on disk as XML.
	 * @param	history		History object to save.
	 * @param	filepath	Absolute path of output file.
	 * @return	True if history was successfully saved; false if it was not.
	 */
	static bool saveFile(const LLSavedLogins& history, const std::string& filepath);
private:
	LLSavedLoginsList mEntries;
};

#endif // LLLOGINHISTORY_H
