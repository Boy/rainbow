/** 
 * @file llfloatersearchreplace.cpp
 * @brief LLFloaterSearchReplace class implementation
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

#include "llviewerprecompiledheaders.h"
#include "llcheckboxctrl.h"
#include "llfocusmgr.h"
#include "lluictrlfactory.h"

#include "llfloatersearchreplace.h"

const S32 SEARCH_REPLACE_WIDTH = 300;
const S32 SEARCH_REPLACE_HEIGHT = 120;
const std::string SEARCH_REPLACE_TITLE = "Search and Replace";

LLFloaterSearchReplace* LLFloaterSearchReplace::sInstance = NULL;

LLFloaterSearchReplace::LLFloaterSearchReplace() : mEditor(NULL),
	LLFloater(std::string("searchreplace"), LLRect(0, 0, SEARCH_REPLACE_WIDTH, SEARCH_REPLACE_HEIGHT), SEARCH_REPLACE_TITLE)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_search_replace.xml");
}

LLFloaterSearchReplace::~LLFloaterSearchReplace()
{
	sInstance = NULL;
}

void LLFloaterSearchReplace::open()
{
	LLFloater::open();

	if (mEditor)
	{
		bool fReadOnly = mEditor->isReadOnly();
		childSetEnabled("replace_label", !fReadOnly);
		childSetEnabled("replace_text", !fReadOnly);
		childSetEnabled("replace_btn", !fReadOnly);
		childSetEnabled("replace_all_btn", !fReadOnly);
	}

	childSetFocus("search_text", TRUE); 
}

BOOL LLFloaterSearchReplace::postBuild()
{
	childSetAction("search_btn", onBtnSearch, this);
	childSetAction("replace_btn", onBtnReplace, this);
	childSetAction("replace_all_btn", onBtnReplaceAll, this);
	
	setDefaultBtn("search_btn");

	return TRUE;
}

void LLFloaterSearchReplace::show(LLTextEditor* editor)
{
	if (!sInstance)
	{
		sInstance = new LLFloaterSearchReplace();
	}

	if (sInstance && editor)
	{
		sInstance->mEditor = editor;

		LLFloater* newdependee, *olddependee = sInstance->getDependee();
		LLView* viewp = editor->getParent();
		while (viewp)
		{
			newdependee = dynamic_cast<LLFloater*>(viewp);
			if (newdependee)
			{
				if (newdependee != olddependee)
				{
					if (olddependee)
						olddependee->removeDependentFloater(sInstance);

					if (!newdependee->getHost())
						newdependee->addDependentFloater(sInstance);
					else
						newdependee->getHost()->addDependentFloater(sInstance);
				}
				break;
			}
			viewp = viewp->getParent();
		}

		sInstance->open();
	}
}

void LLFloaterSearchReplace::onBtnSearch(void* userdata)
{
	if (!sInstance || !sInstance->mEditor || !sInstance->getDependee())
		return;

	LLCheckBoxCtrl* caseChk = sInstance->getChild<LLCheckBoxCtrl>("case_text");
	sInstance->mEditor->selectNext(sInstance->childGetText("search_text"), caseChk->get());
}

void LLFloaterSearchReplace::onBtnReplace(void* userdata)
{
	if (!sInstance || !sInstance->mEditor || !sInstance->getDependee())
		return;

	LLCheckBoxCtrl* caseChk = sInstance->getChild<LLCheckBoxCtrl>("case_text");
	sInstance->mEditor->replaceText(sInstance->childGetText("search_text"), sInstance->childGetText("replace_text"), caseChk->get());
}

void LLFloaterSearchReplace::onBtnReplaceAll(void* userdata)
{
	if (!sInstance || !sInstance->mEditor || !sInstance->getDependee())
		return;

	LLCheckBoxCtrl* caseChk = sInstance->getChild<LLCheckBoxCtrl>("case_text");
	sInstance->mEditor->replaceTextAll(sInstance->childGetText("search_text"), sInstance->childGetText("replace_text"), caseChk->get());
}
