/**
 * @file lltrans.cpp
 * @brief LLTrans implementation
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


#include "llviewerprecompiledheaders.h"
#include "lltrans.h"
#include "llxmlnode.h"
#include "lluictrlfactory.h"
#include "llalertdialog.h"

#include <map>

LLTrans::template_map_t LLTrans::sStringTemplates;

//static 
bool LLTrans::parseStrings(const std::string& xml_filename)
{
	LLXMLNodePtr root;
	BOOL success  = LLUICtrlFactory::getLayeredXMLNode(xml_filename, root);

	if (!success || root.isNull() || !root->hasName( "strings" ))
	{
		llerrs << "Problem reading strings: " << xml_filename << llendl;
		return false;
	}
	
	for (LLXMLNode* string = root->getFirstChild();
		 string != NULL; string = string->getNextSibling())
	{
		if (!string->hasName("string"))
		{
			continue;
		}
		
		std::string string_name;

		if (! string->getAttributeString("name", string_name))
		{
			llwarns << "Unable to parse string with no name" << llendl;
			continue;
		}
	
		LLTransTemplate xml_template(string_name, string->getTextContents());
		sStringTemplates[xml_template.mName] = xml_template;
	}

	return true;
}

//static 
std::string LLTrans::getString(const std::string &xml_desc, const LLStringUtil::format_map_t& args)
{
	template_map_t::iterator iter = sStringTemplates.find(xml_desc);

	if (iter != sStringTemplates.end())
	{
		std::string text = iter->second.mText;
		LLStringUtil::format(text, args);
		
		return text;
	}
	else
	{
		LLStringUtil::format_map_t args;
		args["[STRING_NAME]"] = xml_desc;
		llwarns << "Missing String: [" << xml_desc << "]" << llendl;
		LLAlertDialog* dialogp = LLAlertDialog::showXml("MissingString", args);
		if (dialogp == NULL)
		{
			llerrs << "Bad or missing alerts.xml!" << llendl;
		}
		return "";
	}
}

