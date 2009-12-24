/** 
 * @file llnamelistctrl.cpp
 * @brief A list of names, automatically refreshed from name cache.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include <boost/tokenizer.hpp>

#include "llnamelistctrl.h"

#include "llcachename.h"
#include "llagent.h"
#include "llinventory.h"

static LLRegisterWidget<LLNameListCtrl> r("name_list");

// statics
std::set<LLNameListCtrl*> LLNameListCtrl::sInstances;

LLNameListCtrl::LLNameListCtrl(const std::string& name,
							   const LLRect& rect,
							   LLUICtrlCallback cb,
							   void* userdata,
							   BOOL allow_multiple_selection,
							   BOOL draw_border,
							   S32 name_column_index,
							   const std::string& tooltip)
:	LLScrollListCtrl(name, rect, cb, userdata, allow_multiple_selection,
					 draw_border),
	mNameColumnIndex(name_column_index),
	mAllowCallingCardDrop(FALSE)
{
	setToolTip(tooltip);
	LLNameListCtrl::sInstances.insert(this);
}


// virtual
LLNameListCtrl::~LLNameListCtrl()
{
	LLNameListCtrl::sInstances.erase(this);
}


// public
BOOL LLNameListCtrl::addNameItem(const LLUUID& agent_id, EAddPosition pos,
								 BOOL enabled, std::string& suffix)
{
	//llinfos << "LLNameListCtrl::addNameItem " << agent_id << llendl;

	std::string fullname;
	BOOL result = gCacheName->getFullName(agent_id, fullname);

	fullname.append(suffix);

	addStringUUIDItem(fullname, agent_id, pos, enabled);

	return result;
}

// virtual, public
BOOL LLNameListCtrl::handleDragAndDrop( 
		S32 x, S32 y, MASK mask,
		BOOL drop,
		EDragAndDropType cargo_type, void *cargo_data, 
		EAcceptance *accept,
		std::string& tooltip_msg)
{
	if (!mAllowCallingCardDrop)
	{
		return FALSE;
	}

	BOOL handled = FALSE;

	if (cargo_type == DAD_CALLINGCARD)
	{
		if (drop)
		{
			LLInventoryItem* item = (LLInventoryItem *)cargo_data;
			addNameItem(item->getCreatorUUID());
		}

		*accept = ACCEPT_YES_MULTI;
	}
	else
	{
		*accept = ACCEPT_NO;
		if (tooltip_msg.empty())
		{
			if (!getToolTip().empty())
			{
				tooltip_msg = getToolTip();
			}
			else
			{
				// backwards compatable English tooltip (should be overridden in xml)
				tooltip_msg.assign("Drag a calling card here\nto add a resident.");
			}
		}
	}

	handled = TRUE;
	lldebugst(LLERR_USER_INPUT) << "dragAndDrop handled by LLNameListCtrl " << getName() << llendl;

	return handled;
}

// public
void LLNameListCtrl::addGroupNameItem(const LLUUID& group_id, EAddPosition pos,
									  BOOL enabled)
{
	//llinfos << "LLNameListCtrl::addGroupNameItem " << group_id << llendl;
	std::string group_name;
	gCacheName->getGroupName(group_id, group_name);
	addStringUUIDItem(group_name, group_id, pos, enabled);
}

// public
void LLNameListCtrl::addGroupNameItem(LLScrollListItem* item, EAddPosition pos)
					
{
	//llinfos << "LLNameListCtrl::addGroupNameItem " << item->getUUID() << llendl;

	std::string group_name;
	gCacheName->getGroupName(item->getUUID(), group_name);

	LLScrollListCell* cell = (LLScrollListCell*)item->getColumn(mNameColumnIndex);
	((LLScrollListText*)cell)->setText( std::string(group_name) );

	addItem(item, pos);
}

BOOL LLNameListCtrl::addNameItem(LLScrollListItem* item, EAddPosition pos)
{
	//llinfos << "LLNameListCtrl::addNameItem " << item->getUUID() << llendl;

	std::string fullname;
	BOOL result = gCacheName->getFullName(item->getUUID(), fullname);

	LLScrollListCell* cell = (LLScrollListCell*)item->getColumn(mNameColumnIndex);
	((LLScrollListText*)cell)->setText( fullname );

	addItem(item, pos);

	// this column is resizable
	LLScrollListColumn* columnp = getColumn(mNameColumnIndex);
	if (columnp && columnp->mHeader)
	{
		columnp->mHeader->setHasResizableElement(TRUE);
	}

	return result;
}

LLScrollListItem* LLNameListCtrl::addElement(const LLSD& value, EAddPosition pos, void* userdata)
{
	LLScrollListItem* item = LLScrollListCtrl::addElement(value, pos, userdata);

	// use supplied name by default
	std::string fullname = value["name"].asString();
	if (value["target"].asString() == "GROUP")
	{
		gCacheName->getGroupName(item->getUUID(), fullname);
		// fullname will be "nobody" if group not found
	}
	else if (value["target"].asString() == "SPECIAL")
	{
		// just use supplied name
	}
	else // normal resident
	{
		std::string name;
		if (gCacheName->getFullName(item->getUUID(), name))
		{
			fullname = name;
		}
	}
	
	LLScrollListCell* cell = (LLScrollListCell*)item->getColumn(mNameColumnIndex);
	((LLScrollListText*)cell)->setText( fullname );

	dirtyColumns();

	// this column is resizable
	LLScrollListColumn* columnp = getColumn(mNameColumnIndex);
	if (columnp && columnp->mHeader)
	{
		columnp->mHeader->setHasResizableElement(TRUE);
	}

	return item;
}

// public
void LLNameListCtrl::removeNameItem(const LLUUID& agent_id)
{
	BOOL item_exists = selectByID( agent_id );
	if(item_exists)
	{
		S32 index = getItemIndex(getFirstSelected());
		if(index >= 0)
		{
			deleteSingleItem(index);
		}
	}
}

// public
void LLNameListCtrl::refresh(const LLUUID& id, const std::string& first, 
							 const std::string& last, BOOL is_group)
{
	//llinfos << "LLNameListCtrl::refresh " << id << " '" << first << " "
	//	<< last << "'" << llendl;

	std::string fullname;
	if (!is_group)
	{
		fullname = first + " " + last;
	}
	else
	{
		fullname = first;
	}

	// TODO: scan items for that ID, fix if necessary
	item_list::iterator iter;
	for (iter = getItemList().begin(); iter != getItemList().end(); iter++)
	{
		LLScrollListItem* item = *iter;
		if (item->getUUID() == id)
		{
			LLScrollListCell* cell = (LLScrollListCell*)item->getColumn(0);
			cell = (LLScrollListCell*)item->getColumn(mNameColumnIndex);

			((LLScrollListText*)cell)->setText( fullname );
		}
	}

	dirtyColumns();
}


// static
void LLNameListCtrl::refreshAll(const LLUUID& id, const std::string& first,
								const std::string& last, BOOL is_group)
{
	std::set<LLNameListCtrl*>::iterator it;
	for (it = LLNameListCtrl::sInstances.begin();
		 it != LLNameListCtrl::sInstances.end();
		 ++it)
	{
		LLNameListCtrl* ctrl = *it;
		ctrl->refresh(id, first, last, is_group);
	}
}

// virtual
LLXMLNodePtr LLNameListCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLScrollListCtrl::getXML();

	node->createChild("allow_calling_card_drop", TRUE)->setBoolValue(mAllowCallingCardDrop);

	if (mNameColumnIndex != 0)
	{
		node->createChild("name_column_index", TRUE)->setIntValue(mNameColumnIndex);
	}

	// Don't save contents, probably filled by code

	return node;
}

LLView* LLNameListCtrl::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string name("name_list");
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	BOOL multi_select = FALSE;
	node->getAttributeBOOL("multi_select", multi_select);

	BOOL draw_border = TRUE;
	node->getAttributeBOOL("draw_border", draw_border);

	BOOL draw_heading = FALSE;
	node->getAttributeBOOL("draw_heading", draw_heading);

	S32 name_column_index = 0;
	node->getAttributeS32("name_column_index", name_column_index);

	LLUICtrlCallback callback = NULL;

	LLNameListCtrl* name_list = new LLNameListCtrl(name,
				   rect,
				   callback,
				   NULL,
				   multi_select,
				   draw_border,
				   name_column_index);

	name_list->setDisplayHeading(draw_heading);
	if (node->hasAttribute("heading_height"))
	{
		S32 heading_height;
		node->getAttributeS32("heading_height", heading_height);
		name_list->setHeadingHeight(heading_height);
	}

	BOOL allow_calling_card_drop = FALSE;
	if (node->getAttributeBOOL("allow_calling_card_drop", allow_calling_card_drop))
	{
		name_list->setAllowCallingCardDrop(allow_calling_card_drop);
	}

	name_list->setScrollListParameters(node);

	name_list->initFromXML(node, parent);

	LLSD columns;
	S32 index = 0;
	S32 total_static = 0;
	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		if (child->hasName("column"))
		{
			std::string labelname("");
			child->getAttributeString("label", labelname);

			std::string columnname(labelname);
			child->getAttributeString("name", columnname);

			BOOL columndynamicwidth = FALSE;
			child->getAttributeBOOL("dynamicwidth", columndynamicwidth);

			std::string sortname(columnname);
			child->getAttributeString("sort", sortname);
		
			S32 columnwidth = -1;
			if (child->hasAttribute("relwidth"))
			{
				F32 columnrelwidth = 0.f;
				child->getAttributeF32("relwidth", columnrelwidth);
				columns[index]["relwidth"] = columnrelwidth;
			}
			else
			{
				child->getAttributeS32("width", columnwidth);
				columns[index]["width"] = columnwidth;
			}

			LLFontGL::HAlign h_align = LLFontGL::LEFT;
			h_align = LLView::selectFontHAlign(child);

			if(!columndynamicwidth) total_static += llmax(0, columnwidth);

			columns[index]["name"] = columnname;
			columns[index]["label"] = labelname;
			columns[index]["halign"] = (S32)h_align;
			columns[index]["dynamicwidth"] = columndynamicwidth;
			columns[index]["sort"] = sortname;

			index++;
		}
	}
	name_list->setTotalStaticColumnWidth(total_static);
	name_list->setColumnHeadings(columns);


	for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
	{
		if (child->hasName("row"))
		{
			LLUUID id;
			child->getAttributeUUID("id", id);

			LLSD row;

			row["id"] = id;

			S32 column_idx = 0;
			LLXMLNodePtr row_child;
			for (row_child = node->getFirstChild(); row_child.notNull(); row_child = row_child->getNextSibling())
			{
				if (row_child->hasName("column"))
				{
					std::string value = row_child->getTextContents();

					std::string columnname("");
					row_child->getAttributeString("name", columnname);

					std::string font("");
					row_child->getAttributeString("font", font);

					std::string font_style("");
					row_child->getAttributeString("font-style", font_style);

					row["columns"][column_idx]["column"] = columnname;
					row["columns"][column_idx]["value"] = value;
					row["columns"][column_idx]["font"] = font;
					row["columns"][column_idx]["font-style"] = font_style;
					column_idx++;
				}
			}
			name_list->addElement(row);
		}
	}

	std::string contents = node->getTextContents();

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("\t\n");
	tokenizer tokens(contents, sep);
	tokenizer::iterator token_iter = tokens.begin();

	while(token_iter != tokens.end())
	{
		const std::string& line = *token_iter;
		name_list->addCommentText(line);
		++token_iter;
	}

	return name_list;
}



