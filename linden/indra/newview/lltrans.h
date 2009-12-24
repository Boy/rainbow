/**
 * @file lltrans.h
 * @brief LLTrans definition
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_TRANS_H
#define LL_TRANS_H

#include <map>

/**
 * @brief String template loaded from strings.xml
 */
class LLTransTemplate
{
public:
	LLTransTemplate(const std::string& name = LLStringUtil::null, const std::string& text = LLStringUtil::null) : mName(name), mText(text) {}

	std::string mName;
	std::string mText;
};

/**
 * @brief Localized strings class
 * This class is used to retrieve translations of strings used to build larger ones, as well as
 * strings with a general usage that don't belong to any specific floater. For example,
 * "Owner:", "Retrieving..." used in the place of a not yet known name, etc.
 */
class LLTrans
{
public:
	LLTrans();

	/**
	 * @brief Parses the xml file that holds the strings. Used once on startup
	 * @param xml_filename Filename to parse
	 * @returns true if the file was parsed successfully, true if something went wrong
	 */
	static bool parseStrings(const std::string& xml_filename);

	/**
	 * @brief Returns a translated string
	 * @param xml_desc String's description
	 * @param args A list of substrings to replace in the string
	 * @returns Translated string
	 */
	static std::string getString(const std::string &xml_desc, const LLStringUtil::format_map_t& args);

	/**
	 * @brief Returns a translated string
	 * @param xml_desc String's description
	 * @returns Translated string
	 */
	static std::string getString(const std::string &xml_desc)
	{
		LLStringUtil::format_map_t empty;
		return getString(xml_desc, empty);
	}
	

private:
	typedef std::map<std::string, LLTransTemplate > template_map_t;
	static template_map_t sStringTemplates;
};

#endif
