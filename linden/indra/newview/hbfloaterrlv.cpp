/** 
 * @file hbfloaterrlv.cpp
 * @brief The HBFloaterRLV class definitions
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 * 
 * Copyright (c) 2011, Henri Beauchamp
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

#include "llinventory.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h" 

#include "hbfloaterrlv.h"
#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"

HBFloaterRLV* HBFloaterRLV::sInstance = NULL;

HBFloaterRLV::HBFloaterRLV()
: LLFloater(std::string("restrained love")),
  mIsDirty(false)
{
    LLUICtrlFactory::getInstance()->buildFloater(this, "floater_rlv_restrictions.xml");
}

//virtual
HBFloaterRLV::~HBFloaterRLV()
{
    sInstance = NULL;
}

BOOL HBFloaterRLV::postBuild()
{
	mRestrictions = getChild<LLScrollListCtrl>("restrictions_list");

	if (mRestrictions)
	{
		childSetAction("refresh", setDirty, NULL);
		childSetAction("close", onButtonClose, this);
		mIsDirty = true;
	}

	return TRUE;
}

void set_element(LLSD& element, LLUUID& uuid, std::string& restrictions)
{
	std::string object_name;
	LLInventoryItem* item = NULL;
	LLViewerObject* object = gObjectList.findObject(uuid);
	bool is_missing = (object == NULL);
	bool is_root = true;
	if (!is_missing)
	{
		is_root = (object == object->getRootEdit());
		item = gAgent.mRRInterface.getItem(uuid);
	}
	if (!is_missing && item)
	{
		// Attached object
		object_name = item->getName();
		if (is_root)
		{
			element["columns"][0]["font-style"] = "BOLD";
		}
		else
		{
			element["columns"][0]["font-style"] = "NORMAL";
		}
	}
	else
	{
		// In-world (it should normally have used a relay !), or missing object
		object_name = uuid.asString();
		if (is_missing)
		{
			element["columns"][0]["color"] = LLColor4::red2.getValue();
		}
		else if (is_root)
		{
			element["columns"][0]["font-style"] = "BOLD|ITALIC";
		}
		else
		{
			element["columns"][0]["font-style"] = "NORMAL|ITALIC";
		}
	}
	element["columns"][0]["column"] = "object_name";
	element["columns"][0]["font"] = "SANSSERIF_SMALL";
	element["columns"][0]["value"] = object_name;

	element["columns"][1]["column"] = "restrictions";
	element["columns"][1]["font"] = "SANSSERIF_SMALL";
	element["columns"][1]["font-style"] = "NORMAL";
	element["columns"][1]["value"] = restrictions;
}

//virtual
void HBFloaterRLV::draw()
{
	if (mIsDirty && mRestrictions)
	{
		S32 scrollpos = mRestrictions->getScrollPos();
		mRestrictions->deleteAllItems();

		if (!gAgent.mRRInterface.mSpecialObjectBehaviours.empty())
		{
			LLUUID uuid(LLUUID::null), old_uuid(LLUUID::null);
			std::string restrictions;

			bool first = true;
			RRMAP::iterator it = gAgent.mRRInterface.mSpecialObjectBehaviours.begin();
			while (it != gAgent.mRRInterface.mSpecialObjectBehaviours.end())
			{
				old_uuid = uuid;
				uuid = LLUUID(it->first);
				if (!first && uuid != old_uuid)
				{
					LLSD element;
					set_element(element, old_uuid, restrictions);
					mRestrictions->addElement(element, ADD_BOTTOM);
					restrictions = "";
				}
				if (!restrictions.empty())
				{
					restrictions += ",";
				}
				restrictions += it->second;
				it++;
				first = false;
			}
			LLSD element;
			set_element(element, uuid, restrictions);
			mRestrictions->addElement(element, ADD_BOTTOM);
		}

		mRestrictions->setScrollPos(scrollpos);
		mIsDirty = false;
	}

	LLFloater::draw();
}

//static
void HBFloaterRLV::showInstance()
{
	if (!sInstance)
	{
		sInstance = new HBFloaterRLV();
 	}
	sInstance->open();
}

//static
void HBFloaterRLV::setDirty(void*)
{
    if (sInstance)
	{
		sInstance->mIsDirty = true;
	}
}

//static
void HBFloaterRLV::onButtonClose(void* data)
{
	HBFloaterRLV* self = (HBFloaterRLV*)data;
	if (self)
	{
		self->destroy();
	}
}
