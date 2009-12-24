/** 
 * @file llpanelgrouproles.cpp
 * @brief Panel for roles information about a particular group.
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

#include "llcheckboxctrl.h"

#include "llagent.h"
#include "llbutton.h"
#include "llfloateravatarinfo.h"
#include "llfloatergroupinvite.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llnotify.h"
#include "llpanelgrouproles.h"
#include "llscrolllistctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "llviewerimagelist.h"
#include "llviewerwindow.h"
#include "llfocusmgr.h"

#include "roles_constants.h"

bool agentCanRemoveFromRole(const LLUUID& group_id,
							const LLUUID& role_id)
{
	return gAgent.hasPowerInGroup(group_id, GP_ROLE_REMOVE_MEMBER);
}

bool agentCanAddToRole(const LLUUID& group_id,
					   const LLUUID& role_id)
{
	if (gAgent.isGodlike())
		return true;
    
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_id);
	if (!gdatap) 
	{
		llwarns << "agentCanAddToRole "
				<< "-- No group data!" << llendl;
		return false;
	}

	//make sure the agent is in the group
	LLGroupMgrGroupData::member_list_t::iterator mi = gdatap->mMembers.find(gAgent.getID());
	if (mi == gdatap->mMembers.end())
	{
		return false;
	}
	
	LLGroupMemberData* member_data = (*mi).second;

	// Owners can add to any role.
	if ( member_data->isInRole(gdatap->mOwnerRole) )
	{
		return true;
	}

	// 'Limited assign members' can add to roles the user is in.
	if ( gAgent.hasPowerInGroup(group_id, GP_ROLE_ASSIGN_MEMBER_LIMITED) &&
			member_data->isInRole(role_id) )
	{
		return true;
	}

	// 'assign members' can add to non-owner roles.
	if ( gAgent.hasPowerInGroup(group_id, GP_ROLE_ASSIGN_MEMBER) &&
			 role_id != gdatap->mOwnerRole )
	{
		return true;
	}

	return false;
}

// static
void* LLPanelGroupRoles::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupRoles("panel group roles", *group_id);
}

LLPanelGroupRoles::LLPanelGroupRoles(const std::string& name, const LLUUID& group_id)
:	LLPanelGroupTab(name, group_id),
	mCurrentTab(NULL),
	mRequestedTab( NULL ),
	mSubTabContainer( NULL ),
	mFirstUse( TRUE ),
	mIgnoreTransition( FALSE )
{
}

LLPanelGroupRoles::~LLPanelGroupRoles()
{
	int i;
	for (i = 0; i < mSubTabContainer->getTabCount(); ++i)
	{
		LLPanelGroupSubTab* subtabp = (LLPanelGroupSubTab*) mSubTabContainer->getPanelByIndex(i);

		subtabp->removeObserver(this);
	}
}

BOOL LLPanelGroupRoles::postBuild()
{
	lldebugs << "LLPanelGroupRoles::postBuild()" << llendl;

	mSubTabContainer = getChild<LLTabContainer>("roles_tab_container");

	if (!mSubTabContainer) return FALSE;

	// Hook up each sub-tabs callback and widgets.
	S32 i;
	for (i = 0; i < mSubTabContainer->getTabCount(); ++i)
	{
		LLPanelGroupSubTab* subtabp = (LLPanelGroupSubTab*) mSubTabContainer->getPanelByIndex(i);

		// Add click callbacks to all the tabs.
		mSubTabContainer->setTabChangeCallback(subtabp, onClickSubTab);
		mSubTabContainer->setTabUserData(subtabp, this);

		// Hand the subtab a pointer to this LLPanelGroupRoles, so that it can
		// look around for the widgets it is interested in.
		if (!subtabp->postBuildSubTab(this)) return FALSE;

		subtabp->addObserver(this);
	}

	// Set the current tab to whatever is currently being shown.
	mCurrentTab = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (!mCurrentTab)
	{
		// Need to select a tab.
		mSubTabContainer->selectFirstTab();
		mCurrentTab = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	}

	if (!mCurrentTab) return FALSE;

	// Act as though this tab was just activated.
	mCurrentTab->activate();

	// Read apply text from the xml file.
	mDefaultNeedsApplyMesg = getString("default_needs_apply_text");
	mWantApplyMesg = getString("want_apply_text");

	return LLPanelGroupTab::postBuild();
}

BOOL LLPanelGroupRoles::isVisibleByAgent(LLAgent* agentp)
{
	/* This power was removed to make group roles simpler
	return agentp->hasPowerInGroup(mGroupID, 
								   GP_ROLE_CREATE |
								   GP_ROLE_DELETE |
								   GP_ROLE_PROPERTIES |
								   GP_ROLE_VIEW |
								   GP_ROLE_ASSIGN_MEMBER |
								   GP_ROLE_REMOVE_MEMBER |
								   GP_ROLE_CHANGE_ACTIONS |
								   GP_MEMBER_INVITE |
								   GP_MEMBER_EJECT |
								   GP_MEMBER_OPTIONS );
	*/
	return mAllowEdit && agentp->isInGroup(mGroupID);
								   
}

// static
void LLPanelGroupRoles::onClickSubTab(void* user_data, bool from_click)
{
	LLPanelGroupRoles* self = static_cast<LLPanelGroupRoles*>(user_data);
	self->handleClickSubTab();
}

void LLPanelGroupRoles::handleClickSubTab()
{
	// If we are already handling a transition,
	// ignore this.
	if (mIgnoreTransition)
	{
		return;
	}

	mRequestedTab = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();

	// Make sure they aren't just clicking the same tab...
	if (mRequestedTab == mCurrentTab)
	{
		return;
	}

	// Try to switch from the current panel to the panel the user selected.
	attemptTransition();
}

BOOL LLPanelGroupRoles::attemptTransition()
{
	// Check if the current tab needs to be applied.
	std::string mesg;
	if (mCurrentTab && mCurrentTab->needsApply(mesg))
	{
		// If no message was provided, give a generic one.
		if (mesg.empty())
		{
			mesg = mDefaultNeedsApplyMesg;
		}
		// Create a notify box, telling the user about the unapplied tab.
		LLStringUtil::format_map_t args;
		args["[NEEDS_APPLY_MESSAGE]"] = mesg;
		args["[WANT_APPLY_MESSAGE]"] = mWantApplyMesg;
		gViewerWindow->alertXml("PanelGroupApply", args,
								onNotifyCallback, (void*) this);
		mHasModal = TRUE;
		// We need to reselect the current tab, since it isn't finished.
		if (mSubTabContainer)
		{
			mIgnoreTransition = TRUE;
			mSubTabContainer->selectTabPanel( mCurrentTab );
			mIgnoreTransition = FALSE;
		}
		// Returning FALSE will block a close action from finishing until
		// we get a response back from the user.
		return FALSE;
	}
	else
	{
		// The current panel didn't have anything it needed to apply.
		if (mRequestedTab)
		{
			transitionToTab();
		}
		return TRUE;
	}
}

void LLPanelGroupRoles::transitionToTab()
{
	// Tell the current panel that it is being deactivated.
	if (mCurrentTab)
	{
		mCurrentTab->deactivate();
	}
	
	// Tell the new panel that it is being activated.
	if (mRequestedTab)
	{
		// This is now the current tab;
		mCurrentTab = mRequestedTab;
		mCurrentTab->activate();
	}
}

// static
void LLPanelGroupRoles::onNotifyCallback(S32 option, void* user_data)
{
	LLPanelGroupRoles* self = static_cast<LLPanelGroupRoles*>(user_data);
	if (self)
	{
		self->handleNotifyCallback(option);
	}
}

void LLPanelGroupRoles::handleNotifyCallback(S32 option)
{
	mHasModal = FALSE;
	switch (option)
	{
	case 0: // "Apply Changes"
	{
		// Try to apply changes, and switch to the requested tab.
		std::string apply_mesg;
		if ( !apply( apply_mesg ) )
		{
			// There was a problem doing the apply.
			if ( !apply_mesg.empty() )
			{
				mHasModal = TRUE;
				LLStringUtil::format_map_t args;
				args["[MESSAGE]"] = apply_mesg;
				gViewerWindow->alertXml("GenericAlert", args, onModalClose, (void*) this);
			}
			// Skip switching tabs.
			break;
		}

		// This panel's info successfully applied.
		// Switch to the next panel.
		// No break!  Continue into 'Ignore Changes' which just switches tabs.
		mIgnoreTransition = TRUE;
		mSubTabContainer->selectTabPanel( mRequestedTab );
		mIgnoreTransition = FALSE;
		transitionToTab();
		break;
	}
	case 1: // "Ignore Changes"
		// Switch to the requested panel without applying changes
		cancel();
		mIgnoreTransition = TRUE;
		mSubTabContainer->selectTabPanel( mRequestedTab );
		mIgnoreTransition = FALSE;
		transitionToTab();
		break;
	case 2: // "Cancel"
	default:
		// Do nothing.  The user is canceling the action.
		break;
	}
}

// static
void LLPanelGroupRoles::onModalClose(S32 option, void* user_data)
{
	LLPanelGroupRoles* self = static_cast<LLPanelGroupRoles*>(user_data);
	if (self)
	{
		self->mHasModal = FALSE;
	}
}


bool LLPanelGroupRoles::apply(std::string& mesg)
{
	// Pass this along to the currently visible sub tab.
	if (!mSubTabContainer) return false;

	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (!panelp) return false;
	
	// Ignore the needs apply message.
	std::string ignore_mesg;
	if ( !panelp->needsApply(ignore_mesg) )
	{
		// We don't need to apply anything.
		// We're done.
		return true;
	}

	// Try to do the actual apply.
	return panelp->apply(mesg);
}

void LLPanelGroupRoles::cancel()
{
	// Pass this along to the currently visible sub tab.
	if (!mSubTabContainer) return;

	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (!panelp) return;

	panelp->cancel();
}

// Pass all of these messages to the currently visible sub tab.
std::string LLPanelGroupRoles::getHelpText() const
{
	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (panelp)
	{
		return panelp->getHelpText();
	}
	else
	{
		return mHelpText;
	}
}

void LLPanelGroupRoles::update(LLGroupChange gc)
{
	if (mGroupID.isNull()) return;

	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (panelp)
	{
		panelp->update(gc);
	}
	else
	{
		llwarns << "LLPanelGroupRoles::update() -- No subtab to update!" << llendl;
	}
}

void LLPanelGroupRoles::activate()
{
	// Start requesting member and role data if needed.
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	//if (!gdatap || mFirstUse)
	{
		// Check member data.
		
		if (!gdatap || !gdatap->isMemberDataComplete() )
		{
			LLGroupMgr::getInstance()->sendGroupMembersRequest(mGroupID);
		}

		// Check role data.
		if (!gdatap || !gdatap->isRoleDataComplete() )
		{
			// Mildly hackish - clear all pending changes
			cancel();

			LLGroupMgr::getInstance()->sendGroupRoleDataRequest(mGroupID);
		}

		// Check role-member mapping data.
		if (!gdatap || !gdatap->isRoleMemberDataComplete() )
		{
			LLGroupMgr::getInstance()->sendGroupRoleMembersRequest(mGroupID);
		}

		// Need this to get base group member powers
		if (!gdatap || !gdatap->isGroupPropertiesDataComplete() )
		{
			LLGroupMgr::getInstance()->sendGroupPropertiesRequest(mGroupID);
		}

		mFirstUse = FALSE;
	}

	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (panelp) panelp->activate();
}

void LLPanelGroupRoles::deactivate()
{
	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (panelp) panelp->deactivate();
}

bool LLPanelGroupRoles::needsApply(std::string& mesg)
{
	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (!panelp) return false;
		
	return panelp->needsApply(mesg);
}

BOOL LLPanelGroupRoles::hasModal()
{
	if (mHasModal) return TRUE;

	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (!panelp) return FALSE;
		
	return panelp->hasModal();
}

// PanelGroupTab observer trigger
void LLPanelGroupRoles::tabChanged()
{
	notifyObservers();
}

////////////////////////////
// LLPanelGroupSubTab
////////////////////////////
LLPanelGroupSubTab::LLPanelGroupSubTab(const std::string& name, const LLUUID& group_id)
:	LLPanelGroupTab(name, group_id),
	mHeader(NULL),
	mFooter(NULL),
	mSearchLineEditor(NULL),
	mSearchButton(NULL),
	mShowAllButton(NULL)
{
}

LLPanelGroupSubTab::~LLPanelGroupSubTab()
{
}

BOOL LLPanelGroupSubTab::postBuild()
{
	// Hook up the search widgets.
	bool recurse = true;
	mSearchLineEditor = getChild<LLLineEditor>("search_text", recurse);

	if (!mSearchLineEditor) return FALSE;
	mSearchLineEditor->setKeystrokeCallback(onSearchKeystroke);
	mSearchLineEditor->setCallbackUserData(this);

	mSearchButton = getChild<LLButton>("search_button", recurse);

	if (!mSearchButton) return FALSE;
	mSearchButton->setClickedCallback(onClickSearch);
	mSearchButton->setCallbackUserData(this);
	mSearchButton->setEnabled(FALSE);

	mShowAllButton = getChild<LLButton>("show_all_button", recurse);

	if (!mShowAllButton) return FALSE;
	mShowAllButton->setClickedCallback(onClickShowAll);
	mShowAllButton->setCallbackUserData(this);
	mShowAllButton->setEnabled(FALSE);
	
	// Get icons for later use.
	mActionIcons.clear();

	bool no_recurse = false;

	LLIconCtrl* icon = getChild<LLIconCtrl>("power_folder_icon",no_recurse);
	if (icon && !icon->getImageName().empty())
	{
		mActionIcons["folder"] = icon->getImageName();
		removeChild(icon, TRUE);
	}

	icon = getChild<LLIconCtrl>("power_all_have_icon",no_recurse);
	if (icon && !icon->getImageName().empty())
	{
		mActionIcons["full"] = icon->getImageName();
		removeChild(icon, TRUE);
	}

	icon = getChild<LLIconCtrl>("power_partial_icon",no_recurse);
	if (icon && !icon->getImageName().empty())
	{
		mActionIcons["partial"] = icon->getImageName();
		removeChild(icon, TRUE);
	}

	return LLPanelGroupTab::postBuild();
}

// static
void LLPanelGroupSubTab::onSearchKeystroke(LLLineEditor* caller, void* user_data)
{
	LLPanelGroupSubTab* self = static_cast<LLPanelGroupSubTab*>(user_data);
	self->handleSearchKeystroke(caller);
}

void LLPanelGroupSubTab::handleSearchKeystroke(LLLineEditor* caller)
{
	if (caller->getText().size())
	{
		setDefaultBtn( mSearchButton );
		mSearchButton->setEnabled(TRUE);
	}
	else
	{
		setDefaultBtn( NULL );
		mSearchButton->setEnabled(FALSE);
	}
}

// static 
void LLPanelGroupSubTab::onClickSearch(void* user_data)
{
	LLPanelGroupSubTab* self = static_cast<LLPanelGroupSubTab*>(user_data);
	self->handleClickSearch();
}
 
void LLPanelGroupSubTab::handleClickSearch()
{
	lldebugs << "LLPanelGroupSubTab::handleClickSearch()" << llendl;

	if (0 == mSearchLineEditor->getText().size())
	{
		// No search text.  (This shouldn't happen... the search button should have been disabled).
		llwarns << "handleClickSearch with no search text!" << llendl;
		mSearchButton->setEnabled(FALSE);
		return;
	}

	setSearchFilter( mSearchLineEditor->getText() );
	mShowAllButton->setEnabled(TRUE);
}

// static
void LLPanelGroupSubTab::onClickShowAll(void* user_data)
{
	LLPanelGroupSubTab* self = static_cast<LLPanelGroupSubTab*>(user_data);
	self->handleClickShowAll();
}

void LLPanelGroupSubTab::handleClickShowAll()
{
	lldebugs << "LLPanelGroupSubTab::handleClickShowAll()" << llendl;
	setSearchFilter( LLStringUtil::null );
	mShowAllButton->setEnabled(FALSE);
}

void LLPanelGroupSubTab::setSearchFilter(const std::string& filter)
{
	lldebugs << "LLPanelGroupSubTab::setSearchFilter() ==> '" << filter << "'" << llendl;
	mSearchFilter = filter;
	LLStringUtil::toLower(mSearchFilter);
	update(GC_ALL);
}

void LLPanelGroupSubTab::activate()
{
	lldebugs << "LLPanelGroupSubTab::activate()" << llendl;
	setOthersVisible(TRUE);
}

void LLPanelGroupSubTab::deactivate()
{
	lldebugs << "LLPanelGroupSubTab::deactivate()" << llendl;
	setOthersVisible(FALSE);
}

void LLPanelGroupSubTab::setOthersVisible(BOOL b)
{
	if (mHeader)
	{
		mHeader->setVisible( b );
	}
	else
	{
		llwarns << "LLPanelGroupSubTab missing header!" << llendl;
	}

	if (mFooter)
	{
		mFooter->setVisible( b );
	}
	else
	{
		llwarns << "LLPanelGroupSubTab missing footer!" << llendl;
	}
}

bool LLPanelGroupSubTab::matchesActionSearchFilter(std::string action)
{
	// If the search filter is empty, everything passes.
	if (mSearchFilter.empty()) return true;

	LLStringUtil::toLower(action);
	std::string::size_type match = action.find(mSearchFilter);

	if (std::string::npos == match)
	{
		// not found
		return false;
	}
	else
	{
		return true;
	}
}

void LLPanelGroupSubTab::buildActionsList(LLScrollListCtrl* ctrl, 
										  U64 allowed_by_some, 
										  U64 allowed_by_all,
										  icon_map_t& icons,
										  void (*commit_callback)(LLUICtrl*,void*),
										  BOOL show_all,
										  BOOL filter,
										  BOOL is_owner_role)
{
	if (LLGroupMgr::getInstance()->mRoleActionSets.empty())
	{
		llwarns << "Can't build action list - no actions found." << llendl;
		return;
	}

	std::vector<LLRoleActionSet*>::iterator ras_it = LLGroupMgr::getInstance()->mRoleActionSets.begin();
	std::vector<LLRoleActionSet*>::iterator ras_end = LLGroupMgr::getInstance()->mRoleActionSets.end();

	for ( ; ras_it != ras_end; ++ras_it)
	{
		buildActionCategory(ctrl,
							allowed_by_some,
							allowed_by_all,
							(*ras_it),
							icons, 
							commit_callback,
							show_all,
							filter,
							is_owner_role);
	}
}

void LLPanelGroupSubTab::buildActionCategory(LLScrollListCtrl* ctrl,
											 U64 allowed_by_some,
											 U64 allowed_by_all,
											 LLRoleActionSet* action_set,
											 icon_map_t& icons,
											 void (*commit_callback)(LLUICtrl*,void*),
											 BOOL show_all,
											 BOOL filter,
											 BOOL is_owner_role)
{
	lldebugs << "Building role list for: " << action_set->mActionSetData->mName << llendl;
	// See if the allow mask matches anything in this category.
	if (show_all || (allowed_by_some & action_set->mActionSetData->mPowerBit))
	{
		// List all the actions in this category that at least some members have.
		LLSD row;

		row["columns"][0]["column"] = "icon";
		icon_map_t::iterator iter = icons.find("folder");
		if (iter != icons.end())
		{
			row["columns"][0]["type"] = "icon";
			row["columns"][0]["value"] = (*iter).second;
		}

		row["columns"][1]["column"] = "action";
		row["columns"][1]["value"] = action_set->mActionSetData->mName;
		row["columns"][1]["font-style"] = "BOLD";

		LLScrollListItem* title_row = ctrl->addElement(row, ADD_BOTTOM, action_set->mActionSetData);

		bool category_matches_filter = (filter) ? matchesActionSearchFilter(action_set->mActionSetData->mName) : true;

		std::vector<LLRoleAction*>::iterator ra_it = action_set->mActions.begin();
		std::vector<LLRoleAction*>::iterator ra_end = action_set->mActions.end();

		bool items_match_filter = false;
		BOOL can_change_actions = (!is_owner_role && gAgent.hasPowerInGroup(mGroupID, GP_ROLE_CHANGE_ACTIONS));

		for ( ; ra_it != ra_end; ++ra_it)
		{
			// See if anyone has these action.
			if (!show_all && !(allowed_by_some & (*ra_it)->mPowerBit))
			{
				continue;
			}

			// See if we are filtering out these actions
			// If we aren't using filters, category_matches_filter will be true.
			if (!category_matches_filter
				&& !matchesActionSearchFilter((*ra_it)->mDescription))
			{
				continue;										
			}

			items_match_filter = true;

			// See if everyone has these actions.
			bool show_full_strength = false;
			if ( (allowed_by_some & (*ra_it)->mPowerBit) == (allowed_by_all & (*ra_it)->mPowerBit) )
			{
				show_full_strength = true;
			}

			LLSD row;

			S32 column_index = 0;
			row["columns"][column_index]["column"] = "icon";
			++column_index;

			
			S32 check_box_index = -1;
			if (commit_callback)
			{
				row["columns"][column_index]["column"] = "checkbox";
				row["columns"][column_index]["type"] = "checkbox";
				check_box_index = column_index;
				++column_index;
			}
			else
			{
				if (show_full_strength)
				{
					icon_map_t::iterator iter = icons.find("full");
					if (iter != icons.end())
					{
						row["columns"][column_index]["column"] = "checkbox";
						row["columns"][column_index]["type"] = "icon";
						row["columns"][column_index]["value"] = (*iter).second;
						++column_index;
					}
				}
				else
				{
					icon_map_t::iterator iter = icons.find("partial");
					if (iter != icons.end())
					{
						row["columns"][column_index]["column"] = "checkbox";
						row["columns"][column_index]["type"] = "icon";
						row["columns"][column_index]["value"] = (*iter).second;
						++column_index;
					}
					row["enabled"] = false;
				}
			}

			row["columns"][column_index]["column"] = "action";
			row["columns"][column_index]["value"] = (*ra_it)->mDescription;
			row["columns"][column_index]["font"] = "SANSSERIFSMALL";

			LLScrollListItem* item = ctrl->addElement(row, ADD_BOTTOM, (*ra_it));

			if (-1 != check_box_index)
			{
				// Extract the checkbox that was created.
				LLScrollListCheck* check_cell = (LLScrollListCheck*) item->getColumn(check_box_index);
				LLCheckBoxCtrl* check = check_cell->getCheckBox();
				check->setEnabled(can_change_actions);
				check->setCommitCallback(commit_callback);
				check->setCallbackUserData(ctrl->getCallbackUserData());
				check->setToolTip( check->getLabel() );

				if (show_all)
				{
					check->setTentative(FALSE);
					if (allowed_by_some & (*ra_it)->mPowerBit)
					{
						check->set(TRUE);
					}
					else
					{
						check->set(FALSE);
					}
				}
				else
				{
					check->set(TRUE);
					if (show_full_strength)
					{
						check->setTentative(FALSE);
					}
					else
					{
						check->setTentative(TRUE);
					}
				}
			}
		}

		if (!items_match_filter)
		{
			S32 title_index = ctrl->getItemIndex(title_row);
			ctrl->deleteSingleItem(title_index);
		}
	}
}

void LLPanelGroupSubTab::setFooterEnabled(BOOL enable)
{
	if (mFooter)
	{
		mFooter->setAllChildrenEnabled(enable);
	}
}

////////////////////////////
// LLPanelGroupMembersSubTab
////////////////////////////

// static
void* LLPanelGroupMembersSubTab::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupMembersSubTab("panel group members sub tab", *group_id);
}

LLPanelGroupMembersSubTab::LLPanelGroupMembersSubTab(const std::string& name, const LLUUID& group_id)
: 	LLPanelGroupSubTab(name, group_id),
	mMembersList(NULL),
	mAssignedRolesList(NULL),
	mAllowedActionsList(NULL),
	mChanged(FALSE),
	mPendingMemberUpdate(FALSE),
	mHasMatch(FALSE),
	mNumOwnerAdditions(0)
{
}

LLPanelGroupMembersSubTab::~LLPanelGroupMembersSubTab()
{
}

BOOL LLPanelGroupMembersSubTab::postBuildSubTab(LLView* root)
{
	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*) root;

	// Look recursively from the parent to find all our widgets.
	bool recurse = true;
	mHeader = parent->getChild<LLPanel>("members_header", recurse);
	mFooter = parent->getChild<LLPanel>("members_footer", recurse);

	mMembersList 		= parent->getChild<LLNameListCtrl>("member_list", recurse);
	mAssignedRolesList	= parent->getChild<LLScrollListCtrl>("member_assigned_roles", recurse);
	mAllowedActionsList	= parent->getChild<LLScrollListCtrl>("member_allowed_actions", recurse);

	if (!mMembersList || !mAssignedRolesList || !mAllowedActionsList) return FALSE;

	// We want to be notified whenever a member is selected.
	mMembersList->setCallbackUserData(this);
	mMembersList->setCommitOnSelectionChange(TRUE);
	mMembersList->setCommitCallback(onMemberSelect);
	// Show the member's profile on double click.
	mMembersList->setDoubleClickCallback(onMemberDoubleClick);

	LLButton* button = parent->getChild<LLButton>("member_invite", recurse);
	if ( button )
	{
		button->setClickedCallback(onInviteMember);
		button->setCallbackUserData(this);
		button->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_MEMBER_INVITE));
	}

	mEjectBtn = parent->getChild<LLButton>("member_eject", recurse);
	if ( mEjectBtn )
	{
		mEjectBtn->setClickedCallback(onEjectMembers);
		mEjectBtn->setCallbackUserData(this);
		mEjectBtn->setEnabled(FALSE);
	}

	return TRUE;
}


// static
void LLPanelGroupMembersSubTab::onMemberSelect(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupMembersSubTab* self = static_cast<LLPanelGroupMembersSubTab*>(user_data);
	self->handleMemberSelect();
}

void LLPanelGroupMembersSubTab::handleMemberSelect()
{
	lldebugs << "LLPanelGroupMembersSubTab::handleMemberSelect" << llendl;

	mAssignedRolesList->deleteAllItems();
	mAllowedActionsList->deleteAllItems();
	
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		llwarns << "LLPanelGroupMembersSubTab::handleMemberSelect() "
				<< "-- No group data!" << llendl;
		return;
	}

	// Check if there is anything selected.
	std::vector<LLScrollListItem*> selection = mMembersList->getAllSelected();
	if (selection.empty()) return;

	// Build a vector of all selected members, and gather allowed actions.
	std::vector<LLUUID> selected_members;
	U64 allowed_by_all = 0xffffffffffffLL;
	U64 allowed_by_some = 0;

	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = selection.begin();
		 itor != selection.end(); ++itor)
	{
		selected_members.push_back( (*itor)->getUUID() );
		// Get this member's power mask including any unsaved changes

		U64 powers = getAgentPowersBasedOnRoleChanges((*itor)->getUUID());

		allowed_by_all &= powers;
		allowed_by_some |= powers;
	}
	std::sort(selected_members.begin(), selected_members.end());

	//////////////////////////////////
	// Build the allowed actions list.
	//////////////////////////////////
	buildActionsList(mAllowedActionsList,
					 allowed_by_some,
					 allowed_by_all,
					 mActionIcons,
					 NULL,
					 FALSE,
					 FALSE,
					 FALSE);

	//////////////////////////////////
	// Build the assigned roles list.
	//////////////////////////////////
	// Add each role to the assigned roles list.
	LLGroupMgrGroupData::role_list_t::iterator iter = gdatap->mRoles.begin();
	LLGroupMgrGroupData::role_list_t::iterator end  = gdatap->mRoles.end();

	BOOL can_eject_members = gAgent.hasPowerInGroup(mGroupID,
													GP_MEMBER_EJECT);
	BOOL member_is_owner = FALSE;
	
	for( ; iter != end; ++iter)
	{
		// Count how many selected users are in this role.
		const LLUUID& role_id = iter->first;
		LLGroupRoleData* group_role_data = iter->second;

		if (group_role_data)
		{
			const BOOL needs_sort = FALSE;
			S32 count = group_role_data->getMembersInRole(
											selected_members, needs_sort);
			//check if the user has permissions to assign/remove
			//members to/from the role (but the ability to add/remove
			//should only be based on the "saved" changes to the role
			//not in the temp/meta data. -jwolk
			BOOL cb_enable = ( (count > 0) ?
							   agentCanRemoveFromRole(mGroupID, role_id) :
							   agentCanAddToRole(mGroupID, role_id) );


			// Owner role has special enabling permissions for removal.
			if (cb_enable && (count > 0) && role_id == gdatap->mOwnerRole)
			{
				// Check if any owners besides this agent are selected.
				std::vector<LLUUID>::const_iterator member_iter;
				std::vector<LLUUID>::const_iterator member_end =
												selected_members.end();
				for (member_iter = selected_members.begin();
					 member_iter != member_end;	
					 ++member_iter)
				{
					// Don't count the agent.
					if ((*member_iter) == gAgent.getID()) continue;
					
					// Look up the member data.
					LLGroupMgrGroupData::member_list_t::iterator mi = 
									gdatap->mMembers.find((*member_iter));
					if (mi == gdatap->mMembers.end()) continue;
					LLGroupMemberData* member_data = (*mi).second;
					// Is the member an owner?
					if ( member_data && member_data->isInRole(gdatap->mOwnerRole) )
					{
						// Can't remove other owners.
						cb_enable = FALSE;
						break;
					}
				}
			}

			//now see if there are any role changes for the selected
			//members and remember to include them
			std::vector<LLUUID>::iterator sel_mem_iter = selected_members.begin();
			for (; sel_mem_iter != selected_members.end(); sel_mem_iter++)
			{
				LLRoleMemberChangeType type;
				if ( getRoleChangeType(*sel_mem_iter, role_id, type) )
				{
					if ( type == RMC_ADD ) count++;
					else if ( type == RMC_REMOVE ) count--;
				}
			}
			
			// If anyone selected is in any role besides 'Everyone' then they can't be ejected.
			if (role_id.notNull() && (count > 0))
			{
				can_eject_members = FALSE;
				if (role_id == gdatap->mOwnerRole)
				{
					member_is_owner = TRUE;
				}
			}

			LLRoleData rd;
			if (gdatap->getRoleData(role_id,rd))
			{
				std::ostringstream label;
				label << rd.mRoleName;
				// Don't bother showing a count, if there is only 0 or 1.
				if (count > 1)
				{
					label << ": " << count ;
				}
	
				LLSD row;
				row["id"] = role_id;

				row["columns"][0]["column"] = "checkbox";
				row["columns"][0]["type"] = "checkbox";

				row["columns"][1]["column"] = "role";
				row["columns"][1]["value"] = label.str();

				if (row["id"].asUUID().isNull())
				{
					// This is the everyone role, you can't take people out of the everyone role!
					row["enabled"] = false;
				}

				LLScrollListItem* item = mAssignedRolesList->addElement(row);

				// Extract the checkbox that was created.
				LLScrollListCheck* check_cell = (LLScrollListCheck*) item->getColumn(0);
				LLCheckBoxCtrl* check = check_cell->getCheckBox();
				check->setCommitCallback(onRoleCheck);
				check->setCallbackUserData(this);
				check->set( count > 0 );
				check->setTentative(
					(0 != count)
					&& (selected_members.size() !=
						(std::vector<LLUUID>::size_type)count));

				//NOTE: as of right now a user can break the group
				//by removing himself from a role if he is the
				//last owner.  We should check for this special case
				// -jwolk
				check->setEnabled(cb_enable);
			}
		}
		else
		{
			// This could happen if changes are not synced right on sub-panel change.
			llwarns << "No group role data for " << iter->second << llendl;
		}
	}
	mAssignedRolesList->setEnabled(TRUE);

	if (gAgent.isGodlike())
		can_eject_members = TRUE;

	if (!can_eject_members && !member_is_owner)
	{
		// Maybe we can eject them because we are an owner...
		LLGroupMgrGroupData::member_list_t::iterator mi = gdatap->mMembers.find(gAgent.getID());
		if (mi != gdatap->mMembers.end())
		{
			LLGroupMemberData* member_data = (*mi).second;

			if ( member_data && member_data->isInRole(gdatap->mOwnerRole) )
			{
				can_eject_members = TRUE;
			}
		}
	}

	mEjectBtn->setEnabled(can_eject_members);
}

// static
void LLPanelGroupMembersSubTab::onMemberDoubleClick(void* user_data)
{
	LLPanelGroupMembersSubTab* self = static_cast<LLPanelGroupMembersSubTab*>(user_data);
	self->handleMemberDoubleClick();
}

//static
void LLPanelGroupMembersSubTab::onInviteMember(void *userdata)
{
	LLPanelGroupMembersSubTab* selfp = (LLPanelGroupMembersSubTab*) userdata;

	if ( selfp )
	{
		selfp->handleInviteMember();
	}
}

void LLPanelGroupMembersSubTab::handleInviteMember()
{
	LLFloaterGroupInvite::showForGroup(mGroupID);
}

void LLPanelGroupMembersSubTab::onEjectMembers(void *userdata)
{
	LLPanelGroupMembersSubTab* selfp = (LLPanelGroupMembersSubTab*) userdata;

	if ( selfp )
	{
		selfp->handleEjectMembers();
	}
}

void LLPanelGroupMembersSubTab::handleEjectMembers()
{
	//send down an eject message
	std::vector<LLUUID> selected_members;

	std::vector<LLScrollListItem*> selection = mMembersList->getAllSelected();
	if (selection.empty()) return;

	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = selection.begin() ; 
		 itor != selection.end(); ++itor)
	{
		selected_members.push_back((*itor)->getUUID());
	}

	mMembersList->deleteSelectedItems();

	LLGroupMgr::getInstance()->sendGroupMemberEjects(mGroupID,
									 selected_members);
}

void LLPanelGroupMembersSubTab::handleRoleCheck(const LLUUID& role_id,
												LLRoleMemberChangeType type)
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) return;

	//add that the user is requesting to change the roles for selected
	//members
	U64 powers_all_have  = 0xffffffffffffLL;
	U64 powers_some_have = 0;

	BOOL   is_owner_role = ( gdatap->mOwnerRole == role_id );
	LLUUID member_id;
	

	std::vector<LLScrollListItem*> selection = mMembersList->getAllSelected();
	if (selection.empty())
	{
		return;
	}
	
	for (std::vector<LLScrollListItem*>::iterator itor = selection.begin() ; 
		 itor != selection.end(); ++itor)
	{
		member_id = (*itor)->getUUID();

		//see if we requested a change for this member before
		if ( mMemberRoleChangeData.find(member_id) == mMemberRoleChangeData.end() )
		{
			mMemberRoleChangeData[member_id] = new role_change_data_map_t;
		}
		role_change_data_map_t* role_change_datap = mMemberRoleChangeData[member_id];

		//now check to see if the selected group member
		//had changed his association with the selected role before

		role_change_data_map_t::iterator  role = role_change_datap->find(role_id);
		if ( role != role_change_datap->end() )
		{
			//see if the new change type cancels out the previous change
			if (role->second != type)
			{
				role_change_datap->erase(role_id);
				if ( is_owner_role ) mNumOwnerAdditions--;
			}
			//else do nothing

			if ( role_change_datap->empty() )
			{
				//the current member now has no role changes
				//so erase the role change and erase the member's entry
				delete role_change_datap;
                role_change_datap = NULL;

				mMemberRoleChangeData.erase(member_id);
			}
		}
		else
		{
			//a previously unchanged role is being changed
			(*role_change_datap)[role_id] = type;
			if ( is_owner_role && type == RMC_ADD ) mNumOwnerAdditions++;
		}

		//we need to calculate what powers the selected members
		//have (including the role changes we're making)
		//so that we can rebuild the action list
		U64 new_powers = getAgentPowersBasedOnRoleChanges(member_id);

		powers_all_have  &= new_powers;
		powers_some_have |= new_powers;
	}

	
	mChanged = !mMemberRoleChangeData.empty();
	notifyObservers();

	//alrighty now we need to update the actions list
	//to reflect the changes
	mAllowedActionsList->deleteAllItems();
	buildActionsList(mAllowedActionsList,
					 powers_some_have,
					 powers_all_have,
					 mActionIcons,
					 NULL,
					 FALSE,
					 FALSE,
					 FALSE);
}


// static 
void LLPanelGroupMembersSubTab::onRoleCheck(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupMembersSubTab* self = static_cast<LLPanelGroupMembersSubTab*>(user_data);
	LLCheckBoxCtrl* check_box = static_cast<LLCheckBoxCtrl*>(ctrl);
	if (!check_box || !self) return;

	LLScrollListItem* first_selected =
		self->mAssignedRolesList->getFirstSelected();
	if (first_selected)
	{
		LLUUID role_id = first_selected->getUUID();
		LLRoleMemberChangeType change_type = (check_box->get() ? 
						      RMC_ADD : 
						      RMC_REMOVE);
		
		self->handleRoleCheck(role_id, change_type);
	}
}

void LLPanelGroupMembersSubTab::handleMemberDoubleClick()
{
	LLScrollListItem* selected = mMembersList->getFirstSelected();
	if (selected)
	{
		LLFloaterAvatarInfo::showFromDirectory( selected->getUUID() );
	}
}

void LLPanelGroupMembersSubTab::activate()
{
	LLPanelGroupSubTab::activate();

	update(GC_ALL);
}

void LLPanelGroupMembersSubTab::deactivate()
{
	LLPanelGroupSubTab::deactivate();
}

bool LLPanelGroupMembersSubTab::needsApply(std::string& mesg)
{
	return mChanged;
}

void LLPanelGroupMembersSubTab::cancel()
{
	if ( mChanged )
	{
		std::for_each(mMemberRoleChangeData.begin(),
					  mMemberRoleChangeData.end(),
					  DeletePairedPointer());
		mMemberRoleChangeData.clear();

		mChanged = FALSE;
		notifyObservers();
	}
}

bool LLPanelGroupMembersSubTab::apply(std::string& mesg)
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "Unable to get group data for group " << mGroupID << llendl;

		mesg.assign("Unable to save member data.  Try again later.");
		return false;
	}

	if (mChanged)
	{
		//figure out if we are somehow adding an owner or not and alert
		//the user...possibly make it ignorable
		if ( mNumOwnerAdditions > 0 )
		{
			LLRoleData rd;
			LLStringUtil::format_map_t args;

			if ( gdatap->getRoleData(gdatap->mOwnerRole, rd) )
			{
				mHasModal = TRUE;
				args["[ROLE_NAME]"] = rd.mRoleName;
				gViewerWindow->alertXml("AddGroupOwnerWarning",
										args,
										addOwnerCB,
										this);
			}
			else
			{
				llwarns << "Unable to get role information for the owner role in group " << mGroupID << llendl;

				mesg.assign("Unable to retried specific group information.  Try again later");
				return false;
			}
				 
		}
		else
		{
			applyMemberChanges();
		}
	}

	return true;
}

//static
void LLPanelGroupMembersSubTab::addOwnerCB(S32 option, void* data)
{
	LLPanelGroupMembersSubTab* self = (LLPanelGroupMembersSubTab*) data;

	if (!self) return;
	
	self->mHasModal = FALSE;

	if (0 == option)
	{
		// User clicked "Yes"
		self->applyMemberChanges();
	}
}

void LLPanelGroupMembersSubTab::applyMemberChanges()
{
	//sucks to do a find again here, but it is in constant time, so, could
	//be worse
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "Unable to get group data for group " << mGroupID << llendl;
		return;
	}

	//we need to add all of the changed roles data
	//for each member whose role changed
	for (member_role_changes_map_t::iterator member = mMemberRoleChangeData.begin();
		 member != mMemberRoleChangeData.end(); ++member)
	{
		for (role_change_data_map_t::iterator role = member->second->begin();
			 role != member->second->end(); ++role)
		{
			gdatap->changeRoleMember(role->first, //role_id
									 member->first, //member_id
									 role->second); //add/remove
		}

		member->second->clear();
		delete member->second;
	}
	mMemberRoleChangeData.clear();

	LLGroupMgr::getInstance()->sendGroupRoleMemberChanges(mGroupID);	
	//force a UI update
	handleMemberSelect();

	mChanged = FALSE;
	mNumOwnerAdditions = 0;
	notifyObservers();
}

bool LLPanelGroupMembersSubTab::matchesSearchFilter(const std::string& fullname)
{
	// If the search filter is empty, everything passes.
	if (mSearchFilter.empty()) return true;

	// Create a full name, and compare it to the search filter.
	std::string fullname_lc(fullname);
	LLStringUtil::toLower(fullname_lc);

	std::string::size_type match = fullname_lc.find(mSearchFilter);

	if (std::string::npos == match)
	{
		// not found
		return false;
	}
	else
	{
		return true;
	}
}

U64 LLPanelGroupMembersSubTab::getAgentPowersBasedOnRoleChanges(const LLUUID& agent_id)
{
	//we loop over all of the changes
	//if we are adding a role, then we simply add the role's powers
	//if we are removing a role, we store that role id away
	//and then we have to build the powers up bases on the roles the agent
	//is in

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		llwarns << "LLPanelGroupMembersSubTab::getAgentPowersBasedOnRoleChanges() -- No group data!" << llendl;
		return GP_NO_POWERS;
	}

	LLGroupMemberData* member_data = gdatap->mMembers[agent_id];
	if ( !member_data )
	{
		llwarns << "LLPanelGroupMembersSubTab::getAgentPowersBasedOnRoleChanges() -- No member data for member with UUID " << agent_id << llendl;
		return GP_NO_POWERS;
	}

	//see if there are unsaved role changes for this agent
	role_change_data_map_t* role_change_datap = NULL;
	member_role_changes_map_t::iterator member = mMemberRoleChangeData.find(agent_id);
	if ( member != mMemberRoleChangeData.end() )
	{
		//this member has unsaved role changes
		//so grab them
		role_change_datap = (*member).second;
	}

	U64 new_powers = GP_NO_POWERS;

	if ( role_change_datap )
	{
		std::vector<LLUUID> roles_to_be_removed;

		for (role_change_data_map_t::iterator role = role_change_datap->begin();
			 role != role_change_datap->end(); ++ role)
		{
			if ( role->second == RMC_ADD )
			{
				new_powers |= gdatap->getRolePowers(role->first);
			}
			else
			{
				roles_to_be_removed.push_back(role->first);
			}
		}

		//loop over the member's current roles, summing up
		//the powers (not including the role we are removing)
		for (LLGroupMemberData::role_list_t::iterator current_role = member_data->roleBegin();
			 current_role != member_data->roleEnd(); ++current_role)
		{
			bool role_in_remove_list =
				(std::find(roles_to_be_removed.begin(),
						   roles_to_be_removed.end(),
						   current_role->second->getID()) !=
				 roles_to_be_removed.end());

			if ( !role_in_remove_list )
			{
				new_powers |= 
					current_role->second->getRoleData().mRolePowers;
			}
		}
	}
	else
	{
		//there are no changes for this member
		//the member's powers are just the ones stored in the group
		//manager
		new_powers = member_data->getAgentPowers();
	}

	return new_powers;
}

//If there is no change, returns false be sure to verify
//that there is a role change before attempting to get it or else
//the data will make no sense.  Stores the role change type
bool LLPanelGroupMembersSubTab::getRoleChangeType(const LLUUID& member_id,
												  const LLUUID& role_id,
												  LLRoleMemberChangeType& type)
{
	member_role_changes_map_t::iterator member_changes_iter = mMemberRoleChangeData.find(member_id);
	if ( member_changes_iter != mMemberRoleChangeData.end() )
	{
		role_change_data_map_t::iterator role_changes_iter = member_changes_iter->second->find(role_id);
		if ( role_changes_iter != member_changes_iter->second->end() )
		{
			type = role_changes_iter->second;
			return true;
		}
	}

	return false;
}

void LLPanelGroupMembersSubTab::draw()
{
	LLPanelGroupSubTab::draw();

	if (mPendingMemberUpdate)
	{
		updateMembers();
	}
}

void LLPanelGroupMembersSubTab::update(LLGroupChange gc)
{
	if (mGroupID.isNull()) return;

	if ( GC_TITLES == gc || GC_PROPERTIES == gc )
	{
		// Don't care about title or general group properties updates.
		return;
	}

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		llwarns << "LLPanelGroupMembersSubTab::update() -- No group data!" << llendl;
		return;
	}

	// Rebuild the members list.
	mMembersList->deleteAllItems();

	// Wait for both all data to be retrieved before displaying anything.
	if (   gdatap->isMemberDataComplete() 
		&& gdatap->isRoleDataComplete()
		&& gdatap->isRoleMemberDataComplete())
	{
		mMemberProgress = gdatap->mMembers.begin();
		mPendingMemberUpdate = TRUE;
		mHasMatch = FALSE;
	}
	else
	{
		// Build a string with info on retrieval progress.
		std::ostringstream retrieved;
		if ( !gdatap->isMemberDataComplete() )
		{
			// Still busy retreiving member list.
			retrieved << "Retrieving member list (" << gdatap->mMembers.size()
					  << " / " << gdatap->mMemberCount << ")...";
		}
		else if( !gdatap->isRoleDataComplete() )
		{
			// Still busy retreiving role list.
			retrieved << "Retrieving role list (" << gdatap->mRoles.size()
					  << " / " << gdatap->mRoleCount << ")...";
		}
		else // (!gdatap->isRoleMemberDataComplete())
		{
			// Still busy retreiving role/member mappings.
			retrieved << "Retrieving role member mappings...";
		}
		mMembersList->setEnabled(FALSE);
		mMembersList->addCommentText(retrieved.str());
	}
}

void LLPanelGroupMembersSubTab::updateMembers()
{
	mPendingMemberUpdate = FALSE;

	lldebugs << "LLPanelGroupMembersSubTab::updateMembers()" << llendl;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		llwarns << "LLPanelGroupMembersSubTab::updateMembers() -- No group data!" << llendl;
		return;
	}

	// Make sure all data is still complete.  Incomplete data
	// may occur if we refresh.
	if (   !gdatap->isMemberDataComplete() 
		|| !gdatap->isRoleDataComplete()
		|| !gdatap->isRoleMemberDataComplete())
	{
		return;
	}
		
	LLGroupMgrGroupData::member_list_t::iterator end = gdatap->mMembers.end();

	S32 i = 0;
	for( ; mMemberProgress != end && i<UPDATE_MEMBERS_PER_FRAME; 
			++mMemberProgress, ++i)
	{
		if (!mMemberProgress->second)
			continue;
		// Do filtering on name if it is already in the cache.
		bool add_member = true;

		std::string fullname;
		if (gCacheName->getFullName(mMemberProgress->first, fullname))
		{
			if ( !matchesSearchFilter(fullname) )
			{
				add_member = false;
			}
		}

		if (add_member)
		{
			// Build the donated tier string.
			std::ostringstream donated;
			donated << mMemberProgress->second->getContribution() << " sq. m.";

			LLSD row;
			row["id"] = (*mMemberProgress).first;

			row["columns"][0]["column"] = "name";
			// value is filled in by name list control

			row["columns"][1]["column"] = "donated";
			row["columns"][1]["value"] = donated.str();

			row["columns"][2]["column"] = "online";
			row["columns"][2]["value"] = mMemberProgress->second->getOnlineStatus();
			row["columns"][2]["font"] = "SANSSERIFSMALL";

			mMembersList->addElement(row);//, ADD_SORTED);
			mHasMatch = TRUE;
		}
	}

	if (mMemberProgress == end)
	{
		if (mHasMatch)
		{
			mMembersList->setEnabled(TRUE);
		}
		else
		{
			mMembersList->setEnabled(FALSE);
			mMembersList->addCommentText(std::string("No match."));
		}
	}
	else
	{
		mPendingMemberUpdate = TRUE;
	}

	// This should clear the other two lists, since nothing is selected.
	handleMemberSelect();
}



////////////////////////////
// LLPanelGroupRolesSubTab
////////////////////////////

// static
void* LLPanelGroupRolesSubTab::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupRolesSubTab("panel group roles sub tab", *group_id);
}

LLPanelGroupRolesSubTab::LLPanelGroupRolesSubTab(const std::string& name, const LLUUID& group_id)
: LLPanelGroupSubTab(name, group_id), mHasRoleChange(FALSE)
{
}

LLPanelGroupRolesSubTab::~LLPanelGroupRolesSubTab()
{
}

BOOL LLPanelGroupRolesSubTab::postBuildSubTab(LLView* root)
{
	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*) root;

	// Look recursively from the parent to find all our widgets.
	bool recurse = true;
	mHeader = parent->getChild<LLPanel>("roles_header", recurse);
	mFooter = parent->getChild<LLPanel>("roles_footer", recurse);


	mRolesList 		= parent->getChild<LLScrollListCtrl>("role_list", recurse);
	mAssignedMembersList	= parent->getChild<LLNameListCtrl>("role_assigned_members", recurse);
	mAllowedActionsList	= parent->getChild<LLScrollListCtrl>("role_allowed_actions", recurse);

	mRoleName = parent->getChild<LLLineEditor>("role_name", recurse);
	mRoleTitle = parent->getChild<LLLineEditor>("role_title", recurse);
	mRoleDescription = parent->getChild<LLTextEditor>("role_description", recurse);

	mMemberVisibleCheck = parent->getChild<LLCheckBoxCtrl>("role_visible_in_list", recurse);

	if (!mRolesList || !mAssignedMembersList || !mAllowedActionsList
		|| !mRoleName || !mRoleTitle || !mRoleDescription || !mMemberVisibleCheck)
	{
		llwarns << "ARG! element not found." << llendl;
		return FALSE;
	}

	mRemoveEveryoneTxt = getString("cant_delete_role");

	mCreateRoleButton = 
		parent->getChild<LLButton>("role_create", recurse);
	if ( mCreateRoleButton )
	{
		mCreateRoleButton->setCallbackUserData(this);
		mCreateRoleButton->setClickedCallback(onCreateRole);
		mCreateRoleButton->setEnabled(FALSE);
	}
	
	mDeleteRoleButton =  
		parent->getChild<LLButton>("role_delete", recurse);
	if ( mDeleteRoleButton )
	{
		mDeleteRoleButton->setCallbackUserData(this);
		mDeleteRoleButton->setClickedCallback(onDeleteRole);
		mDeleteRoleButton->setEnabled(FALSE);
	}

	mRolesList->setCommitOnSelectionChange(TRUE);
	mRolesList->setCallbackUserData(this);
	mRolesList->setCommitCallback(onRoleSelect);

	mMemberVisibleCheck->setCallbackUserData(this);
	mMemberVisibleCheck->setCommitCallback(onMemberVisibilityChange);

	mAllowedActionsList->setCommitOnSelectionChange(TRUE);
	mAllowedActionsList->setCallbackUserData(this);

	mRoleName->setCommitOnFocusLost(TRUE);
	mRoleName->setCallbackUserData(this);
	mRoleName->setKeystrokeCallback(onPropertiesKey);

	mRoleTitle->setCommitOnFocusLost(TRUE);
	mRoleTitle->setCallbackUserData(this);
	mRoleTitle->setKeystrokeCallback(onPropertiesKey);

	mRoleDescription->setCommitOnFocusLost(TRUE);
	mRoleDescription->setCallbackUserData(this);
	mRoleDescription->setCommitCallback(onDescriptionCommit);
	mRoleDescription->setFocusReceivedCallback(onDescriptionFocus, this);

	setFooterEnabled(FALSE);

	return TRUE;
}

void LLPanelGroupRolesSubTab::activate()
{
	LLPanelGroupSubTab::activate();

	mRolesList->deselectAllItems();
	mAssignedMembersList->deleteAllItems();
	mAllowedActionsList->deleteAllItems();
	mRoleName->clear();
	mRoleDescription->clear();
	mRoleTitle->clear();

	setFooterEnabled(FALSE);

	mHasRoleChange = FALSE;
	update(GC_ALL);
}

void LLPanelGroupRolesSubTab::deactivate()
{
	lldebugs << "LLPanelGroupRolesSubTab::deactivate()" << llendl;

	LLPanelGroupSubTab::deactivate();
}

bool LLPanelGroupRolesSubTab::needsApply(std::string& mesg)
{
	lldebugs << "LLPanelGroupRolesSubTab::needsApply()" << llendl;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	return (mHasRoleChange								// Text changed in current role
			|| (gdatap && gdatap->pendingRoleChanges()));	// Pending role changes in the group
}

bool LLPanelGroupRolesSubTab::apply(std::string& mesg)
{
	lldebugs << "LLPanelGroupRolesSubTab::apply()" << llendl;

	saveRoleChanges();
	LLGroupMgr::getInstance()->sendGroupRoleChanges(mGroupID);

	notifyObservers();

	return true;
}

void LLPanelGroupRolesSubTab::cancel()
{
	mHasRoleChange = FALSE;
	LLGroupMgr::getInstance()->cancelGroupRoleChanges(mGroupID);

	notifyObservers();
}

LLSD LLPanelGroupRolesSubTab::createRoleItem(const LLUUID& role_id, 
								 std::string name, 
								 std::string title, 
								 S32 members)
{
	LLSD row;
	row["id"] = role_id;

	row["columns"][0]["column"] = "name";
	row["columns"][0]["value"] = name;

	row["columns"][1]["column"] = "title";
	row["columns"][1]["value"] = title;

	row["columns"][2]["column"] = "members";
	row["columns"][2]["value"] = members;
	
	return row;
}

bool LLPanelGroupRolesSubTab::matchesSearchFilter(std::string rolename, std::string roletitle)
{
	// If the search filter is empty, everything passes.
	if (mSearchFilter.empty()) return true;

	LLStringUtil::toLower(rolename);
	LLStringUtil::toLower(roletitle);
	std::string::size_type match_name = rolename.find(mSearchFilter);
	std::string::size_type match_title = roletitle.find(mSearchFilter);

	if ( (std::string::npos == match_name)
		&& (std::string::npos == match_title))
	{
		// not found
		return false;
	}
	else
	{
		return true;
	}
}

void LLPanelGroupRolesSubTab::update(LLGroupChange gc)
{
	lldebugs << "LLPanelGroupRolesSubTab::update()" << llendl;

	if (mGroupID.isNull()) return;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap || !gdatap->isRoleDataComplete())
	{
		LLGroupMgr::getInstance()->sendGroupRoleDataRequest(mGroupID);
	}
	else
	{
		bool had_selection = false;
		LLUUID last_selected;
		if (mRolesList->getFirstSelected())
		{
			last_selected = mRolesList->getFirstSelected()->getUUID();
			had_selection = true;
		}
		mRolesList->deleteAllItems();

		LLScrollListItem* item = NULL;

		LLGroupMgrGroupData::role_list_t::iterator rit = gdatap->mRoles.begin();
		LLGroupMgrGroupData::role_list_t::iterator end = gdatap->mRoles.end();

		for ( ; rit != end; ++rit)
		{
			LLRoleData rd;
			if (gdatap->getRoleData((*rit).first,rd))
			{
				if (matchesSearchFilter(rd.mRoleName, rd.mRoleTitle))
				{
					// If this is the everyone role, then EVERYONE is in it.
					S32 members_in_role = (*rit).first.isNull() ? gdatap->mMembers.size() : (*rit).second->getTotalMembersInRole();
					LLSD row = createRoleItem((*rit).first,rd.mRoleName, rd.mRoleTitle, members_in_role);
					item = mRolesList->addElement(row, ((*rit).first.isNull()) ? ADD_TOP : ADD_BOTTOM, this);
					if (had_selection && ((*rit).first == last_selected))
					{
						item->setSelected(TRUE);
					}
				}
			}
			else
			{
				llwarns << "LLPanelGroupRolesSubTab::update() No role data for role " << (*rit).first << llendl;
			}
		}

		mRolesList->sortByColumn(std::string("name"), TRUE);

		if ( (gdatap->mRoles.size() < (U32)MAX_ROLES)
			&& gAgent.hasPowerInGroup(mGroupID, GP_ROLE_CREATE) )
		{
			mCreateRoleButton->setEnabled(TRUE);
		}
		else
		{
			mCreateRoleButton->setEnabled(FALSE);
		}

		if (had_selection)
		{
			handleRoleSelect();
		}
		else
		{
			mAssignedMembersList->deleteAllItems();
			mAllowedActionsList->deleteAllItems();
			mRoleName->clear();
			mRoleDescription->clear();
			mRoleTitle->clear();
			setFooterEnabled(FALSE);
			mDeleteRoleButton->setEnabled(FALSE);
		}
	}

	if (!gdatap || !gdatap->isMemberDataComplete())
	{
		LLGroupMgr::getInstance()->sendGroupMembersRequest(mGroupID);
	}
	
	if (!gdatap || !gdatap->isRoleMemberDataComplete())
	{
		LLGroupMgr::getInstance()->sendGroupRoleMembersRequest(mGroupID);
	}

	if ((GC_ROLE_MEMBER_DATA == gc || GC_MEMBER_DATA == gc)
	    && gdatap
	    && gdatap->isMemberDataComplete()
	    && gdatap->isRoleMemberDataComplete())
	{
		buildMembersList();
	}
}

// static
void LLPanelGroupRolesSubTab::onRoleSelect(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	self->handleRoleSelect();
}

void LLPanelGroupRolesSubTab::handleRoleSelect()
{
	BOOL can_delete = TRUE;
	lldebugs << "LLPanelGroupRolesSubTab::handleRoleSelect()" << llendl;

	mAssignedMembersList->deleteAllItems();
	mAllowedActionsList->deleteAllItems();
	
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		llwarns << "LLPanelGroupRolesSubTab::handleRoleSelect() "
				<< "-- No group data!" << llendl;
		return;
	}

	saveRoleChanges();

	// Check if there is anything selected.
	LLScrollListItem* item = mRolesList->getFirstSelected();
	if (!item)
	{
		setFooterEnabled(FALSE);
		return;
	}

	setFooterEnabled(TRUE);

	LLRoleData rd;
	if (gdatap->getRoleData(item->getUUID(),rd))
	{
		BOOL is_owner_role = ( gdatap->mOwnerRole == item->getUUID() );
		mRoleName->setText(rd.mRoleName);
		mRoleTitle->setText(rd.mRoleTitle);
		mRoleDescription->setText(rd.mRoleDescription);

		mAllowedActionsList->setEnabled(gAgent.hasPowerInGroup(mGroupID,
									   GP_ROLE_CHANGE_ACTIONS));
		buildActionsList(mAllowedActionsList,
						 rd.mRolePowers,
						 0LL,
						 mActionIcons,
						 onActionCheck,
						 TRUE,
						 FALSE,
						 is_owner_role);


		mMemberVisibleCheck->set((rd.mRolePowers & GP_MEMBER_VISIBLE_IN_DIR) == GP_MEMBER_VISIBLE_IN_DIR);
		mRoleName->setEnabled(!is_owner_role &&
				gAgent.hasPowerInGroup(mGroupID, GP_ROLE_PROPERTIES));
		mRoleTitle->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_ROLE_PROPERTIES));
		mRoleDescription->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_ROLE_PROPERTIES));
		mMemberVisibleCheck->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_ROLE_PROPERTIES));

		if (item->getUUID().isNull())
		{
			// Everyone role, can't edit description or name or delete
			mRoleDescription->setEnabled(FALSE);
			mRoleName->setEnabled(FALSE);
			can_delete = FALSE;
		}
		//you can't delete the owner role
		if ( is_owner_role ) can_delete = FALSE;
	}
	else
	{
		mRolesList->deselectAllItems();
		mAssignedMembersList->deleteAllItems();
		mAllowedActionsList->deleteAllItems();
		mRoleName->clear();
		mRoleDescription->clear();
		mRoleTitle->clear();
		setFooterEnabled(FALSE);

		can_delete = FALSE;
	}
	mSelectedRole = item->getUUID();
	buildMembersList();

	can_delete = can_delete && gAgent.hasPowerInGroup(mGroupID,
													  GP_ROLE_DELETE);
	mDeleteRoleButton->setEnabled(can_delete);
}

void LLPanelGroupRolesSubTab::buildMembersList()
{
	mAssignedMembersList->deleteAllItems();

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		llwarns << "LLPanelGroupRolesSubTab::handleRoleSelect() "
				<< "-- No group data!" << llendl;
		return;
	}

	// Check if there is anything selected.
	LLScrollListItem* item = mRolesList->getFirstSelected();
	if (!item) return;

	if (item->getUUID().isNull())
	{
		// Special cased 'Everyone' role
		LLGroupMgrGroupData::member_list_t::iterator mit = gdatap->mMembers.begin();
		LLGroupMgrGroupData::member_list_t::iterator end = gdatap->mMembers.end();
		for ( ; mit != end; ++mit)
		{
			mAssignedMembersList->addNameItem((*mit).first);
		}
	}
	else
	{
		LLGroupMgrGroupData::role_list_t::iterator rit = gdatap->mRoles.find(item->getUUID());
		if (rit != gdatap->mRoles.end())
		{
			LLGroupRoleData* rdatap = (*rit).second;
			if (rdatap)
			{
				std::vector<LLUUID>::const_iterator mit = rdatap->getMembersBegin();
				std::vector<LLUUID>::const_iterator end = rdatap->getMembersEnd();
				for ( ; mit != end; ++mit)
				{
					mAssignedMembersList->addNameItem((*mit));
				}
			}
		}
	}
}

// static
void LLPanelGroupRolesSubTab::onActionCheck(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	LLCheckBoxCtrl* check = static_cast<LLCheckBoxCtrl*>(ctrl);
	if (!check || !self) return;

	self->handleActionCheck(check);
}

struct ActionCBData
{
	LLPanelGroupRolesSubTab*	mSelf;
	LLCheckBoxCtrl* 			mCheck;
};

void LLPanelGroupRolesSubTab::handleActionCheck(LLCheckBoxCtrl* check, bool force)
{
	lldebugs << "LLPanelGroupRolesSubTab::handleActionSelect()" << llendl;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		llwarns << "LLPanelGroupRolesSubTab::handleRoleSelect() "
				<< "-- No group data!" << llendl;
		return;
	}

	LLScrollListItem* action_item = mAllowedActionsList->getFirstSelected();
	if (!action_item)
	{
		return;
	}

	LLScrollListItem* role_item = mRolesList->getFirstSelected();
	if (!role_item)
	{
		return;
	}
	LLUUID role_id = role_item->getUUID();

	LLRoleAction* rap = (LLRoleAction*)action_item->getUserdata();
	U64 power = rap->mPowerBit;

	if (check->get())
	{
		if (!force && (    (GP_ROLE_ASSIGN_MEMBER == power)
						|| (GP_ROLE_CHANGE_ACTIONS == power) ))
		{
			// Uncheck the item, for now.  It will be
			// checked if they click 'Yes', below.
			check->set(FALSE);

			LLRoleData rd;
			LLStringUtil::format_map_t args;

			if ( gdatap->getRoleData(role_id, rd) )
			{
				args["[ACTION_NAME]"] = rap->mDescription;
				args["[ROLE_NAME]"] = rd.mRoleName;
				struct ActionCBData* cb_data = new ActionCBData;
				cb_data->mSelf = this;
				cb_data->mCheck = check;
				mHasModal = TRUE;
				std::string warning = "AssignDangerousActionWarning";
				if (GP_ROLE_CHANGE_ACTIONS == power)
				{
					warning = "AssignDangerousAbilityWarning";
				}
				gViewerWindow->alertXml(warning, args, addActionCB, cb_data);
			}
			else
			{
				llwarns << "Unable to look up role information for role id: "
						<< role_id << llendl;
			}
		}
		else
		{
			gdatap->addRolePower(role_id,power);
		}
	}
	else
	{
		gdatap->removeRolePower(role_id,power);
	}

	mHasRoleChange = TRUE;
	notifyObservers();
}

//static
void LLPanelGroupRolesSubTab::addActionCB(S32 option, void* data)
{
	struct ActionCBData* cb_data = (struct ActionCBData*) data;

	if (!cb_data || !cb_data->mSelf || !cb_data->mCheck) return;

	cb_data->mSelf->mHasModal = FALSE;

	if (0 == option)
	{
		// User clicked "Yes"
		cb_data->mCheck->set(TRUE);
		const bool force_add = true;
		cb_data->mSelf->handleActionCheck(cb_data->mCheck, force_add);
	}
}


// static
void LLPanelGroupRolesSubTab::onPropertiesKey(LLLineEditor* ctrl, void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	if (!self) return;

	self->mHasRoleChange = TRUE;
	self->notifyObservers();
}

// static 
void LLPanelGroupRolesSubTab::onDescriptionFocus(LLFocusableElement* ctrl, void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	if (!self) return;

	self->mHasRoleChange = TRUE;
	self->notifyObservers();
}

// static 
void LLPanelGroupRolesSubTab::onDescriptionCommit(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	if (!self) return;

	self->mHasRoleChange = TRUE;
	self->notifyObservers();
}

// static 
void LLPanelGroupRolesSubTab::onMemberVisibilityChange(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	LLCheckBoxCtrl* check = static_cast<LLCheckBoxCtrl*>(ctrl);
	if (!check || !self) return;

	self->handleMemberVisibilityChange(check->get());
}

void LLPanelGroupRolesSubTab::handleMemberVisibilityChange(bool value)
{
	lldebugs << "LLPanelGroupRolesSubTab::handleMemberVisibilityChange()" << llendl;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		llwarns << "LLPanelGroupRolesSubTab::handleRoleSelect() "
				<< "-- No group data!" << llendl;
		return;
	}

	LLScrollListItem* role_item = mRolesList->getFirstSelected();
	if (!role_item)
	{
		return;
	}

	if (value)
	{
		gdatap->addRolePower(role_item->getUUID(),GP_MEMBER_VISIBLE_IN_DIR);
	}
	else
	{
		gdatap->removeRolePower(role_item->getUUID(),GP_MEMBER_VISIBLE_IN_DIR);
	}
}

// static 
void LLPanelGroupRolesSubTab::onCreateRole(void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	if (!self) return;

	self->handleCreateRole();
}

void LLPanelGroupRolesSubTab::handleCreateRole()
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap) return;

	LLUUID new_role_id;
	new_role_id.generate();

	LLRoleData rd;
	rd.mRoleName = "New Role";
	gdatap->createRole(new_role_id,rd);

	mRolesList->deselectAllItems(TRUE);
	LLSD row;
	row["id"] = new_role_id;
	row["columns"][0]["column"] = "name";
	row["columns"][0]["value"] = rd.mRoleName;
	mRolesList->addElement(row, ADD_BOTTOM, this);
	mRolesList->selectByID(new_role_id);

	// put focus on name field and select its contents
	if(mRoleName)
	{
		mRoleName->setFocus(TRUE);
		mRoleName->onTabInto();
		gFocusMgr.triggerFocusFlash();
	}

	notifyObservers();
}

// static 
void LLPanelGroupRolesSubTab::onDeleteRole(void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	if (!self) return;

	self->handleDeleteRole();
}

void LLPanelGroupRolesSubTab::handleDeleteRole()
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap) return;

	LLScrollListItem* role_item = mRolesList->getFirstSelected();
	if (!role_item)
	{
		return;
	}

	if (role_item->getUUID().isNull() || role_item->getUUID() == gdatap->mOwnerRole)
	{
		LLStringUtil::format_map_t args;
		args["[MESSAGE]"] = mRemoveEveryoneTxt;
		LLNotifyBox::showXml("GenericNotify", args);
		return;
	}

	gdatap->deleteRole(role_item->getUUID());
	mRolesList->deleteSingleItem(mRolesList->getFirstSelectedIndex());
	mRolesList->selectFirstItem();

	notifyObservers();
}

void LLPanelGroupRolesSubTab::saveRoleChanges()
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap) return;

	if (mHasRoleChange)
	{
		LLRoleData rd;
		if (!gdatap->getRoleData(mSelectedRole,rd)) return;

		rd.mRoleName = mRoleName->getText();
		rd.mRoleDescription = mRoleDescription->getText();
		rd.mRoleTitle = mRoleTitle->getText();

		gdatap->setRoleData(mSelectedRole,rd);

		mRolesList->deleteSingleItem(mRolesList->getItemIndex(mSelectedRole));
		
		LLSD row = createRoleItem(mSelectedRole,rd.mRoleName,rd.mRoleTitle,0);
		LLScrollListItem* item = mRolesList->addElement(row, ADD_BOTTOM, this);
		item->setSelected(TRUE);

		mHasRoleChange = FALSE;
	}
}
////////////////////////////
// LLPanelGroupActionsSubTab
////////////////////////////

// static
void* LLPanelGroupActionsSubTab::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupActionsSubTab("panel group actions sub tab", *group_id);
}

LLPanelGroupActionsSubTab::LLPanelGroupActionsSubTab(const std::string& name, const LLUUID& group_id)
: LLPanelGroupSubTab(name, group_id)
{
}

LLPanelGroupActionsSubTab::~LLPanelGroupActionsSubTab()
{
}

BOOL LLPanelGroupActionsSubTab::postBuildSubTab(LLView* root)
{
	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*) root;

	// Look recursively from the parent to find all our widgets.
	bool recurse = true;
	mHeader = parent->getChild<LLPanel>("actions_header", recurse);
	mFooter = parent->getChild<LLPanel>("actions_footer", recurse);

	mActionDescription = parent->getChild<LLTextEditor>("action_description", recurse);

	mActionList = parent->getChild<LLScrollListCtrl>("action_list",recurse);
	mActionRoles = parent->getChild<LLScrollListCtrl>("action_roles",recurse);
	mActionMembers  = parent->getChild<LLNameListCtrl>("action_members",recurse);

	if (!mActionList || !mActionDescription || !mActionRoles || !mActionMembers) return FALSE;

	mActionList->setCallbackUserData(this);
	mActionList->setCommitOnSelectionChange(TRUE);
	mActionList->setCommitCallback(onActionSelect);

	mActionMembers->setCallbackUserData(this);
	mActionRoles->setCallbackUserData(this);

	update(GC_ALL);

	return TRUE;
}

void LLPanelGroupActionsSubTab::activate()
{
	LLPanelGroupSubTab::activate();
	lldebugs << "LLPanelGroupActionsSubTab::activate()" << llendl;

	mActionList->deselectAllItems();
	mActionMembers->deleteAllItems();
	mActionRoles->deleteAllItems();
	mActionDescription->clear();
}

void LLPanelGroupActionsSubTab::deactivate()
{
	lldebugs << "LLPanelGroupActionsSubTab::deactivate()" << llendl;

	LLPanelGroupSubTab::deactivate();
}

bool LLPanelGroupActionsSubTab::needsApply(std::string& mesg)
{
	lldebugs << "LLPanelGroupActionsSubTab::needsApply()" << llendl;

	return false;
}

bool LLPanelGroupActionsSubTab::apply(std::string& mesg)
{
	lldebugs << "LLPanelGroupActionsSubTab::apply()" << llendl;
	return true;
}

void LLPanelGroupActionsSubTab::update(LLGroupChange gc)
{
	lldebugs << "LLPanelGroupActionsSubTab::update()" << llendl;

	if (mGroupID.isNull()) return;

	mActionList->deselectAllItems();
	mActionMembers->deleteAllItems();
	mActionRoles->deleteAllItems();
	mActionDescription->clear();

	mActionList->deleteAllItems();
	buildActionsList(mActionList,
					 GP_ALL_POWERS,
					 GP_ALL_POWERS,
					 mActionIcons,
					 NULL,
					 FALSE,
					 TRUE,
					 FALSE);
}

// static 
void LLPanelGroupActionsSubTab::onActionSelect(LLUICtrl* scroll, void* data)
{
	LLPanelGroupActionsSubTab* self = static_cast<LLPanelGroupActionsSubTab*>(data);
	self->handleActionSelect();
}

void LLPanelGroupActionsSubTab::handleActionSelect()
{
	mActionMembers->deleteAllItems();
	mActionRoles->deleteAllItems();

	U64 power_mask = GP_NO_POWERS;
	std::vector<LLScrollListItem*> selection = 
							mActionList->getAllSelected();
	if (selection.empty()) return;

	LLRoleAction* rap;

	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = selection.begin() ; 
		 itor != selection.end(); ++itor)
	{
		rap = (LLRoleAction*)( (*itor)->getUserdata() );
		power_mask |= rap->mPowerBit;
	}

	if (selection.size() == 1)
	{
		LLScrollListItem* item = selection[0];
		rap = (LLRoleAction*)(item->getUserdata());

		if (rap->mLongDescription.empty())
		{
			mActionDescription->setText(rap->mDescription);
		}
		else
		{
			mActionDescription->setText(rap->mLongDescription);
		}
	}
	else
	{
		mActionDescription->clear();
	}

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap) return;

	if (gdatap->isMemberDataComplete())
	{
		LLGroupMgrGroupData::member_list_t::iterator it = gdatap->mMembers.begin();
		LLGroupMgrGroupData::member_list_t::iterator end = gdatap->mMembers.end();
		LLGroupMemberData* gmd;

		for ( ; it != end; ++it)
		{
			gmd = (*it).second;
			if (!gmd) continue;
			if ((gmd->getAgentPowers() & power_mask) == power_mask)
			{
				mActionMembers->addNameItem(gmd->getID());
			}
		}
	}
	else
	{
		LLGroupMgr::getInstance()->sendGroupMembersRequest(mGroupID);
	}

	if (gdatap->isRoleDataComplete())
	{
		LLGroupMgrGroupData::role_list_t::iterator it = gdatap->mRoles.begin();
		LLGroupMgrGroupData::role_list_t::iterator end = gdatap->mRoles.end();
		LLGroupRoleData* rmd;

		for ( ; it != end; ++it)
		{
			rmd = (*it).second;
			if (!rmd) continue;
			if ((rmd->getRoleData().mRolePowers & power_mask) == power_mask)
			{
				mActionRoles->addSimpleElement(rmd->getRoleData().mRoleName);
			}
		}
	}
	else
	{
		LLGroupMgr::getInstance()->sendGroupRoleDataRequest(mGroupID);
	}
}
