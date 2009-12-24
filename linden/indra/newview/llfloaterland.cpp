/** 
 * @file llfloaterland.cpp
 * @brief "About Land" floater, allowing display and editing of land parcel properties.
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

#include <sstream>
#include <time.h>

#include "llfloaterland.h"

#include "llcachename.h"
#include "llfocusmgr.h"
#include "llparcel.h"
#include "message.h"

#include "llagent.h"
#include "llfloateravatarpicker.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llradiogroup.h"
#include "llcombobox.h"
#include "llfloaterauction.h"
#include "llfloateravatarinfo.h"
#include "llfloatergroups.h"
#include "llfloatergroupinfo.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llnotify.h"
#include "llpanellandmedia.h"
#include "llradiogroup.h"
#include "llscrolllistctrl.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lluiconstants.h"
#include "lluictrlfactory.h"
#include "llviewermessage.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "roles_constants.h"

static std::string OWNER_ONLINE 	= "0";
static std::string OWNER_OFFLINE	= "1";
static std::string OWNER_GROUP 		= "2";

// constants used in callbacks below - syntactic sugar.
static const BOOL BUY_GROUP_LAND = TRUE;
static const BOOL BUY_PERSONAL_LAND = FALSE;

// Statics
LLParcelSelectionObserver* LLFloaterLand::sObserver = NULL;
S32 LLFloaterLand::sLastTab = 0;

LLHandle<LLFloater> LLPanelLandGeneral::sBuyPassDialogHandle;

// Local classes
class LLParcelSelectionObserver : public LLParcelObserver
{
public:
	virtual void changed() { LLFloaterLand::refreshAll(); }
};

//---------------------------------------------------------------------------
// LLFloaterLand
//---------------------------------------------------------------------------

void send_parcel_select_objects(S32 parcel_local_id, U32 return_type,
								uuid_list_t* return_ids = NULL)
{
	LLMessageSystem *msg = gMessageSystem;

	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if (!region) return;

	// Since new highlight will be coming in, drop any highlights
	// that exist right now.
	LLSelectMgr::getInstance()->unhighlightAll();

	msg->newMessageFast(_PREHASH_ParcelSelectObjects);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
	msg->addU32Fast(_PREHASH_ReturnType, return_type);

	// Throw all return ids into the packet.
	// TODO: Check for too many ids.
	if (return_ids)
	{
		uuid_list_t::iterator end = return_ids->end();
		for (uuid_list_t::iterator it = return_ids->begin();
			 it != end;
			 ++it)
		{
			msg->nextBlockFast(_PREHASH_ReturnIDs);
			msg->addUUIDFast(_PREHASH_ReturnID, (*it));
		}
	}
	else
	{
		// Put in a null key so that the message is complete.
		msg->nextBlockFast(_PREHASH_ReturnIDs);
		msg->addUUIDFast(_PREHASH_ReturnID, LLUUID::null);
	}

	msg->sendReliable(region->getHost());
}


//static
LLPanelLandObjects* LLFloaterLand::getCurrentPanelLandObjects()
{
	return LLFloaterLand::getInstance()->mPanelObjects;
}

//static
LLPanelLandCovenant* LLFloaterLand::getCurrentPanelLandCovenant()
{
	return LLFloaterLand::getInstance()->mPanelCovenant;
}

// static
void LLFloaterLand::refreshAll()
{
	LLFloaterLand::getInstance()->refresh();
}

void LLFloaterLand::onOpen()
{
	// Done automatically when the selected parcel's properties arrive
	// (and hence we have the local id).
	// LLViewerParcelMgr::getInstance()->sendParcelAccessListRequest(AL_ACCESS | AL_BAN | AL_RENTER);

	mParcel = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection();
	
	// Refresh even if not over a region so we don't get an
	// uninitialized dialog. The dialog is 0-region aware.
	refresh();
}


// virtual
void LLFloaterLand::onClose(bool app_quitting)
{
	LLViewerParcelMgr::getInstance()->removeObserver( sObserver );
	delete sObserver;
	sObserver = NULL;

	// Might have been showing owned objects
	LLSelectMgr::getInstance()->unhighlightAll();

	// Save which panel we had open
	sLastTab = mTabLand->getCurrentPanelIndex();

	destroy();
}


LLFloaterLand::LLFloaterLand(const LLSD& seed)
:	LLFloater(std::string("floaterland"), std::string("FloaterLandRect5"), std::string("About Land"))
{


	LLCallbackMap::map_t factory_map;
	factory_map["land_general_panel"] = LLCallbackMap(createPanelLandGeneral, this);

	
	factory_map["land_covenant_panel"] = LLCallbackMap(createPanelLandCovenant, this);
	factory_map["land_objects_panel"] = LLCallbackMap(createPanelLandObjects, this);
	factory_map["land_options_panel"] = LLCallbackMap(createPanelLandOptions, this);
	factory_map["land_media_panel"] =	LLCallbackMap(createPanelLandMedia, this);
	factory_map["land_access_panel"] =	LLCallbackMap(createPanelLandAccess, this);

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_about_land.xml", &factory_map, false);

	sObserver = new LLParcelSelectionObserver();
	LLViewerParcelMgr::getInstance()->addObserver( sObserver );
}

BOOL LLFloaterLand::postBuild()
{
	LLTabContainer* tab = getChild<LLTabContainer>("landtab");

	mTabLand = (LLTabContainer*) tab;

	if (tab)
	{
		tab->selectTab(sLastTab);
	}

	return TRUE;
}


// virtual
LLFloaterLand::~LLFloaterLand()
{
}

// public
void LLFloaterLand::refresh()
{
	mPanelGeneral->refresh();
	mPanelObjects->refresh();
	mPanelOptions->refresh();
	mPanelMedia->refresh();
	mPanelAccess->refresh();
}



void* LLFloaterLand::createPanelLandGeneral(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelGeneral = new LLPanelLandGeneral(self->mParcel);
	return self->mPanelGeneral;
}

// static
void* LLFloaterLand::createPanelLandCovenant(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelCovenant = new LLPanelLandCovenant(self->mParcel);
	return self->mPanelCovenant;
}


// static
void* LLFloaterLand::createPanelLandObjects(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelObjects = new LLPanelLandObjects(self->mParcel);
	return self->mPanelObjects;
}

// static
void* LLFloaterLand::createPanelLandOptions(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelOptions = new LLPanelLandOptions(self->mParcel);
	return self->mPanelOptions;
}

// static
void* LLFloaterLand::createPanelLandMedia(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelMedia = new LLPanelLandMedia(self->mParcel);
	return self->mPanelMedia;
}

// static
void* LLFloaterLand::createPanelLandAccess(void* data)
{
	LLFloaterLand* self = (LLFloaterLand*)data;
	self->mPanelAccess = new LLPanelLandAccess(self->mParcel);
	return self->mPanelAccess;
}

//---------------------------------------------------------------------------
// LLPanelLandGeneral
//---------------------------------------------------------------------------


LLPanelLandGeneral::LLPanelLandGeneral(LLParcelSelectionHandle& parcel)
:	LLPanel(std::string("land_general_panel")),
	mUncheckedSell(FALSE),
	mParcel(parcel)
{
}

BOOL LLPanelLandGeneral::postBuild()
{

	mEditName = getChild<LLLineEditor>("Name");
	mEditName->setCommitCallback(onCommitAny);	
	childSetPrevalidate("Name", LLLineEditor::prevalidatePrintableNotPipe);
	childSetUserData("Name", this);

	mEditDesc = getChild<LLTextEditor>("Description");
	mEditDesc->setCommitOnFocusLost(TRUE);
	mEditDesc->setCommitCallback(onCommitAny);	
	childSetPrevalidate("Description", LLLineEditor::prevalidatePrintableNotPipe);
	childSetUserData("Description", this);

	
	mTextSalePending = getChild<LLTextBox>("SalePending");
	mTextOwnerLabel = getChild<LLTextBox>("Owner:");
	mTextOwner = getChild<LLTextBox>("OwnerText");

	
	mBtnProfile = getChild<LLButton>("Profile...");
	mBtnProfile->setClickedCallback(onClickProfile, this);

	
	mTextGroupLabel = getChild<LLTextBox>("Group:");
	mTextGroup = getChild<LLTextBox>("GroupText");

	
	mBtnSetGroup = getChild<LLButton>("Set...");
	mBtnSetGroup->setClickedCallback(onClickSetGroup, this);

	

	mCheckDeedToGroup = getChild<LLCheckBoxCtrl>( "check deed");
	childSetCommitCallback("check deed", onCommitAny, this);

	
	mBtnDeedToGroup = getChild<LLButton>("Deed...");
	mBtnDeedToGroup->setClickedCallback(onClickDeed, this);

	
	mCheckContributeWithDeed = getChild<LLCheckBoxCtrl>( "check contrib");
	childSetCommitCallback("check contrib", onCommitAny, this);

	
	
	mSaleInfoNotForSale = getChild<LLTextBox>("Not for sale.");
	
	mSaleInfoForSale1 = getChild<LLTextBox>("For Sale: Price L$[PRICE].");

	
	mBtnSellLand = getChild<LLButton>("Sell Land...");
	mBtnSellLand->setClickedCallback(onClickSellLand, this);
	
	mSaleInfoForSale2 = getChild<LLTextBox>("For sale to");
	
	mSaleInfoForSaleObjects = getChild<LLTextBox>("Sell with landowners objects in parcel.");
	
	mSaleInfoForSaleNoObjects = getChild<LLTextBox>("Selling with no objects in parcel.");

	
	mBtnStopSellLand = getChild<LLButton>("Cancel Land Sale");
	mBtnStopSellLand->setClickedCallback(onClickStopSellLand, this);

	
	mTextClaimDateLabel = getChild<LLTextBox>("Claimed:");
	mTextClaimDate = getChild<LLTextBox>("DateClaimText");

	
	mTextPriceLabel = getChild<LLTextBox>("PriceLabel");
	mTextPrice = getChild<LLTextBox>("PriceText");

	
	mTextDwell = getChild<LLTextBox>("DwellText");

	
	mBtnBuyLand = getChild<LLButton>("Buy Land...");
	mBtnBuyLand->setClickedCallback(onClickBuyLand, (void*)&BUY_PERSONAL_LAND);
	
	mBtnBuyGroupLand = getChild<LLButton>("Buy For Group...");
	mBtnBuyGroupLand->setClickedCallback(onClickBuyLand, (void*)&BUY_GROUP_LAND);
	
	
	mBtnBuyPass = getChild<LLButton>("Buy Pass...");
	mBtnBuyPass->setClickedCallback(onClickBuyPass, this);

	mBtnReleaseLand = getChild<LLButton>("Abandon Land...");
	mBtnReleaseLand->setClickedCallback(onClickRelease, NULL);

	mBtnReclaimLand = getChild<LLButton>("Reclaim Land...");
	mBtnReclaimLand->setClickedCallback(onClickReclaim, NULL);
	
	mBtnStartAuction = getChild<LLButton>("Linden Sale...");
	mBtnStartAuction->setClickedCallback(onClickStartAuction, NULL);

	return TRUE;
}


// virtual
LLPanelLandGeneral::~LLPanelLandGeneral()
{ }


// public
void LLPanelLandGeneral::refresh()
{
	mBtnStartAuction->setVisible(gAgent.isGodlike());

	LLParcel *parcel = mParcel->getParcel();
	bool region_owner = false;
	LLViewerRegion* regionp = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if(regionp && (regionp->getOwner() == gAgent.getID()))
	{
		region_owner = true;
		mBtnReleaseLand->setVisible(FALSE);
		mBtnReclaimLand->setVisible(TRUE);
	}
	else
	{
		mBtnReleaseLand->setVisible(TRUE);
		mBtnReclaimLand->setVisible(FALSE);
	}
	if (!parcel)
	{
		// nothing selected, disable panel
		mEditName->setEnabled(FALSE);
		mEditName->setText(LLStringUtil::null);

		mEditDesc->setEnabled(FALSE);
		mEditDesc->setText(getString("no_selection_text"));

		mTextSalePending->setText(LLStringUtil::null);
		mTextSalePending->setEnabled(FALSE);

		mBtnDeedToGroup->setEnabled(FALSE);
		mBtnSetGroup->setEnabled(FALSE);
		mBtnStartAuction->setEnabled(FALSE);

		mCheckDeedToGroup	->set(FALSE);
		mCheckDeedToGroup	->setEnabled(FALSE);
		mCheckContributeWithDeed->set(FALSE);
		mCheckContributeWithDeed->setEnabled(FALSE);

		mTextOwner->setText(LLStringUtil::null);
		mBtnProfile->setLabel(getString("profile_text"));
		mBtnProfile->setEnabled(FALSE);

		mTextClaimDate->setText(LLStringUtil::null);
		mTextGroup->setText(LLStringUtil::null);
		mTextPrice->setText(LLStringUtil::null);

		mSaleInfoForSale1->setVisible(FALSE);
		mSaleInfoForSale2->setVisible(FALSE);
		mSaleInfoForSaleObjects->setVisible(FALSE);
		mSaleInfoForSaleNoObjects->setVisible(FALSE);
		mSaleInfoNotForSale->setVisible(FALSE);
		mBtnSellLand->setVisible(FALSE);
		mBtnStopSellLand->setVisible(FALSE);

		mTextPriceLabel->setText(LLStringUtil::null);
		mTextDwell->setText(LLStringUtil::null);

		mBtnBuyLand->setEnabled(FALSE);
		mBtnBuyGroupLand->setEnabled(FALSE);
		mBtnReleaseLand->setEnabled(FALSE);
		mBtnReclaimLand->setEnabled(FALSE);
		mBtnBuyPass->setEnabled(FALSE);
	}
	else
	{
		// something selected, hooray!
		BOOL is_leased = (LLParcel::OS_LEASED == parcel->getOwnershipStatus());
		BOOL region_xfer = FALSE;
		if(regionp
		   && !(regionp->getRegionFlags() & REGION_FLAGS_BLOCK_LAND_RESELL))
		{
			region_xfer = TRUE;
		}

		// estate owner/manager cannot edit other parts of the parcel
		BOOL estate_manager_sellable = !parcel->getAuctionID()
										&& gAgent.canManageEstate()
										// estate manager/owner can only sell parcels owned by estate owner
										&& regionp
										&& (parcel->getOwnerID() == regionp->getOwner());
		BOOL owner_sellable = region_xfer && !parcel->getAuctionID()
							&& LLViewerParcelMgr::isParcelModifiableByAgent(
										parcel, GP_LAND_SET_SALE_INFO);
		BOOL can_be_sold = owner_sellable || estate_manager_sellable;

		const LLUUID &owner_id = parcel->getOwnerID();
		BOOL is_public = parcel->isPublic();

		// Is it owned?
		if (is_public)
		{
			mTextSalePending->setText(LLStringUtil::null);
			mTextSalePending->setEnabled(FALSE);
			mTextOwner->setText(getString("public_text"));
			mTextOwner->setEnabled(FALSE);
			mBtnProfile->setEnabled(FALSE);
			mTextClaimDate->setText(LLStringUtil::null);
			mTextClaimDate->setEnabled(FALSE);
			mTextGroup->setText(getString("none_text"));
			mTextGroup->setEnabled(FALSE);
			mBtnStartAuction->setEnabled(FALSE);
		}
		else
		{
			if(!is_leased && (owner_id == gAgent.getID()))
			{
				mTextSalePending->setText(getString("need_tier_to_modify"));
				mTextSalePending->setEnabled(TRUE);
			}
			else if(parcel->getAuctionID())
			{
				mTextSalePending->setText(getString("auction_id_text"));
				mTextSalePending->setTextArg("[ID]", llformat("%u", parcel->getAuctionID()));
				mTextSalePending->setEnabled(TRUE);
			}
			else
			{
				// not the owner, or it is leased
				mTextSalePending->setText(LLStringUtil::null);
				mTextSalePending->setEnabled(FALSE);
			}
			//refreshNames();
			mTextOwner->setEnabled(TRUE);

			// We support both group and personal profiles
			mBtnProfile->setEnabled(TRUE);

			if (parcel->getGroupID().isNull())
			{
				// Not group owned, so "Profile"
				mBtnProfile->setLabel(getString("profile_text"));

				mTextGroup->setText(getString("none_text"));
				mTextGroup->setEnabled(FALSE);
			}
			else
			{
				// Group owned, so "Info"
				mBtnProfile->setLabel(getString("info_text"));

				//mTextGroup->setText("HIPPOS!");//parcel->getGroupName());
				mTextGroup->setEnabled(TRUE);
			}

			// Display claim date
			// *TODO:Localize (Time format may need Translating)
			time_t claim_date = parcel->getClaimDate();
			mTextClaimDate->setText(formatted_time(claim_date));
			mTextClaimDate->setEnabled(is_leased);

			BOOL enable_auction = (gAgent.getGodLevel() >= GOD_LIAISON)
								  && (owner_id == GOVERNOR_LINDEN_ID)
								  && (parcel->getAuctionID() == 0);
			mBtnStartAuction->setEnabled(enable_auction);
		}

		// Display options
		BOOL can_edit_identity = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_CHANGE_IDENTITY);
		mEditName->setEnabled(can_edit_identity);
		mEditDesc->setEnabled(can_edit_identity);

		BOOL can_edit_agent_only = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_NO_POWERS);
		mBtnSetGroup->setEnabled(can_edit_agent_only && !parcel->getIsGroupOwned());

		const LLUUID& group_id = parcel->getGroupID();

		// Can only allow deeding if you own it and it's got a group.
		BOOL enable_deed = (owner_id == gAgent.getID()
							&& group_id.notNull()
							&& gAgent.isInGroup(group_id));
		// You don't need special powers to allow your object to
		// be deeded to the group.
		mCheckDeedToGroup->setEnabled(enable_deed);
		mCheckDeedToGroup->set( parcel->getAllowDeedToGroup() );
		mCheckContributeWithDeed->setEnabled(enable_deed && parcel->getAllowDeedToGroup());
		mCheckContributeWithDeed->set(parcel->getContributeWithDeed());

		// Actually doing the deeding requires you to have GP_LAND_DEED
		// powers in the group.
		BOOL can_deed = gAgent.hasPowerInGroup(group_id, GP_LAND_DEED);
		mBtnDeedToGroup->setEnabled(   parcel->getAllowDeedToGroup()
									&& group_id.notNull()
									&& can_deed
									&& !parcel->getIsGroupOwned()
									);

		mEditName->setText( parcel->getName() );
		mEditDesc->setText( parcel->getDesc() );

		BOOL for_sale = parcel->getForSale();
				
		mBtnSellLand->setVisible(FALSE);
		mBtnStopSellLand->setVisible(FALSE);
		
		if (for_sale)
		{
			mSaleInfoForSale1->setVisible(TRUE);
			mSaleInfoForSale2->setVisible(TRUE);
			if (parcel->getSellWithObjects())
			{
				mSaleInfoForSaleObjects->setVisible(TRUE);
				mSaleInfoForSaleNoObjects->setVisible(FALSE);
			}
			else
			{
				mSaleInfoForSaleObjects->setVisible(FALSE);
				mSaleInfoForSaleNoObjects->setVisible(TRUE);
			}
			mSaleInfoNotForSale->setVisible(FALSE);
			mSaleInfoForSale1->setTextArg("[PRICE]", llformat("%d", parcel->getSalePrice()));
			if (can_be_sold)
			{
				mBtnStopSellLand->setVisible(TRUE);
			}
		}
		else
		{
			mSaleInfoForSale1->setVisible(FALSE);
			mSaleInfoForSale2->setVisible(FALSE);
			mSaleInfoForSaleObjects->setVisible(FALSE);
			mSaleInfoForSaleNoObjects->setVisible(FALSE);
			mSaleInfoNotForSale->setVisible(TRUE);
			if (can_be_sold)
			{
				mBtnSellLand->setVisible(TRUE);
			}
		}
		
		refreshNames();

		mBtnBuyLand->setEnabled(
			LLViewerParcelMgr::getInstance()->canAgentBuyParcel(parcel, false));
		mBtnBuyGroupLand->setEnabled(
			LLViewerParcelMgr::getInstance()->canAgentBuyParcel(parcel, true));

		// show pricing information
		S32 area;
		S32 claim_price;
		S32 rent_price;
		F32 dwell;
		LLViewerParcelMgr::getInstance()->getDisplayInfo(&area,
								   &claim_price,
								   &rent_price,
								   &for_sale,
								   &dwell);

		// Area
		LLUIString price = getString("area_size_text");
		price.setArg("[AREA]", llformat("%d",area));	
		mTextPriceLabel->setText(getString("area_text"));
		mTextPrice->setText(price.getString());
		
		mTextDwell->setText(llformat("%.0f", dwell));

		if(region_owner)
		{
			mBtnReclaimLand->setEnabled(
				!is_public && (parcel->getOwnerID() != gAgent.getID()));
		}
		else
		{
			BOOL is_owner_release = LLViewerParcelMgr::isParcelOwnedByAgent(parcel, GP_LAND_RELEASE);
			BOOL is_manager_release = (gAgent.canManageEstate() && 
									regionp && 
									(parcel->getOwnerID() != regionp->getOwner()));
			BOOL can_release = is_owner_release || is_manager_release;
			mBtnReleaseLand->setEnabled( can_release );
		}

		BOOL use_pass = parcel->getParcelFlag(PF_USE_PASS_LIST) && !LLViewerParcelMgr::getInstance()->isCollisionBanned();;
		mBtnBuyPass->setEnabled(use_pass);
	}
}

// public
void LLPanelLandGeneral::refreshNames()
{
	LLParcel *parcel = mParcel->getParcel();
	if (!parcel)
	{
		mTextOwner->setText(LLStringUtil::null);
		return;
	}

	std::string owner;
	if (parcel->getIsGroupOwned())
	{
		owner = getString("group_owned_text");
	}
	else
	{
		// Figure out the owner's name
		gCacheName->getFullName(parcel->getOwnerID(), owner);
	}

	if(LLParcel::OS_LEASE_PENDING == parcel->getOwnershipStatus())
	{
		owner += getString("sale_pending_text");
	}
	mTextOwner->setText(owner);

	std::string group;
	if(!parcel->getGroupID().isNull())
	{
		gCacheName->getGroupName(parcel->getGroupID(), group);
	}
	mTextGroup->setText(group);

	const LLUUID& auth_buyer_id = parcel->getAuthorizedBuyerID();
	if(auth_buyer_id.notNull())
	{
		std::string name;
		gCacheName->getFullName(auth_buyer_id, name);
		mSaleInfoForSale2->setTextArg("[BUYER]", name);
	}
	else
	{
		mSaleInfoForSale2->setTextArg("[BUYER]", getString("anyone"));
	}
}


// virtual
void LLPanelLandGeneral::draw()
{
	refreshNames();
	LLPanel::draw();
}

// static
void LLPanelLandGeneral::onClickSetGroup(void* userdata)
{
	LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)userdata;
	LLFloaterGroupPicker* fg;

	LLFloater* parent_floater = gFloaterView->getParentFloater(panelp);

	fg = LLFloaterGroupPicker::showInstance(LLSD(gAgent.getID()));
	fg->setSelectCallback( cbGroupID, userdata );

	if (parent_floater)
	{
		LLRect new_rect = gFloaterView->findNeighboringPosition(parent_floater, fg);
		fg->setOrigin(new_rect.mLeft, new_rect.mBottom);
		parent_floater->addDependentFloater(fg);
	}
}

// static
void LLPanelLandGeneral::onClickProfile(void* data)
{
	LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
	LLParcel* parcel = panelp->mParcel->getParcel();
	if (!parcel) return;

	if (parcel->getIsGroupOwned())
	{
		const LLUUID& group_id = parcel->getGroupID();
		LLFloaterGroupInfo::showFromUUID(group_id);
	}
	else
	{
		const LLUUID& avatar_id = parcel->getOwnerID();
		LLFloaterAvatarInfo::showFromObject(avatar_id);
	}
}

// static
void LLPanelLandGeneral::cbGroupID(LLUUID group_id, void* userdata)
{
	LLPanelLandGeneral* self = (LLPanelLandGeneral*)userdata;
	self->setGroup(group_id);
}

// public
void LLPanelLandGeneral::setGroup(const LLUUID& group_id)
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel) return;

	// Set parcel properties and send message
	parcel->setGroupID(group_id);
	//parcel->setGroupName(group_name);
	//mTextGroup->setText(group_name);

	// Send update
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(parcel);

	// Update UI
	refresh();
}

// static
void LLPanelLandGeneral::onClickBuyLand(void* data)
{
	BOOL* for_group = (BOOL*)data;
	LLViewerParcelMgr::getInstance()->startBuyLand(*for_group);
}

BOOL LLPanelLandGeneral::enableDeedToGroup(void* data)
{
	LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
	LLParcel* parcel = panelp->mParcel->getParcel();
	return (parcel != NULL) && (parcel->getParcelFlag(PF_ALLOW_DEED_TO_GROUP));
}

// static
void LLPanelLandGeneral::onClickDeed(void*)
{
	//LLParcel* parcel = mParcel->getParcel();
	//if (parcel)
	//{
	LLViewerParcelMgr::getInstance()->startDeedLandToGroup();
	//}
}

// static
void LLPanelLandGeneral::onClickRelease(void*)
{
	LLViewerParcelMgr::getInstance()->startReleaseLand();
}

// static
void LLPanelLandGeneral::onClickReclaim(void*)
{
	lldebugs << "LLPanelLandGeneral::onClickReclaim()" << llendl;
	LLViewerParcelMgr::getInstance()->reclaimParcel();
}

// static
BOOL LLPanelLandGeneral::enableBuyPass(void* data)
{
	LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
	LLParcel* parcel = panelp != NULL ? panelp->mParcel->getParcel() : LLViewerParcelMgr::getInstance()->getParcelSelection()->getParcel();
	return (parcel != NULL) && (parcel->getParcelFlag(PF_USE_PASS_LIST) && !LLViewerParcelMgr::getInstance()->isCollisionBanned());
}


// static
void LLPanelLandGeneral::onClickBuyPass(void* data)
{
	LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
	LLParcel* parcel = panelp != NULL ? panelp->mParcel->getParcel() : LLViewerParcelMgr::getInstance()->getParcelSelection()->getParcel();

	if (!parcel) return;

	S32 pass_price = parcel->getPassPrice();
	std::string parcel_name = parcel->getName();
	F32 pass_hours = parcel->getPassHours();

	std::string cost, time;
	cost = llformat("%d", pass_price);
	time = llformat("%.2f", pass_hours);

	LLStringUtil::format_map_t args;
	args["[COST]"] = cost;
	args["[PARCEL_NAME]"] = parcel_name;
	args["[TIME]"] = time;
	
	sBuyPassDialogHandle = gViewerWindow->alertXml("LandBuyPass", args, cbBuyPass)->getHandle();
}

// static
void LLPanelLandGeneral::onClickStartAuction(void* data)
{
	LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
	LLParcel* parcelp = panelp->mParcel->getParcel();
	if(parcelp)
	{
		if(parcelp->getForSale())
		{
			gViewerWindow->alertXml("CannotStartAuctionAlreadForSale");
		}
		else
		{
			LLFloaterAuction::show();
		}
	}
}

// static
void LLPanelLandGeneral::cbBuyPass(S32 option, void* data)
{
	if (0 == option)
	{
		// User clicked OK
		LLViewerParcelMgr::getInstance()->buyPass();
	}
}

//static 
BOOL LLPanelLandGeneral::buyPassDialogVisible()
{
	return sBuyPassDialogHandle.get() != NULL;
}

// static
void LLPanelLandGeneral::onCommitAny(LLUICtrl *ctrl, void *userdata)
{
	LLPanelLandGeneral *panelp = (LLPanelLandGeneral *)userdata;

	LLParcel* parcel = panelp->mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	std::string name = panelp->mEditName->getText();
	std::string desc = panelp->mEditDesc->getText();

	// Valid data from UI

	// Stuff data into selected parcel
	parcel->setName(name);
	parcel->setDesc(desc);

	BOOL allow_deed_to_group= panelp->mCheckDeedToGroup->get();
	BOOL contribute_with_deed = panelp->mCheckContributeWithDeed->get();

	parcel->setParcelFlag(PF_ALLOW_DEED_TO_GROUP, allow_deed_to_group);
	parcel->setContributeWithDeed(contribute_with_deed);

	// Send update to server
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

	// Might have changed properties, so let's redraw!
	panelp->refresh();
}

// static
void LLPanelLandGeneral::onClickSellLand(void* data)
{
	LLViewerParcelMgr::getInstance()->startSellLand();
}

// static
void LLPanelLandGeneral::onClickStopSellLand(void* data)
{
	LLPanelLandGeneral* panelp = (LLPanelLandGeneral*)data;
	LLParcel* parcel = panelp->mParcel->getParcel();

	parcel->setParcelFlag(PF_FOR_SALE, FALSE);
	parcel->setSalePrice(0);
	parcel->setAuthorizedBuyerID(LLUUID::null);

	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(parcel);
}

//---------------------------------------------------------------------------
// LLPanelLandObjects
//---------------------------------------------------------------------------
LLPanelLandObjects::LLPanelLandObjects(LLParcelSelectionHandle& parcel)
:	LLPanel(std::string("land_objects_panel")), mParcel(parcel)
{
}



BOOL LLPanelLandObjects::postBuild()
{
	
	mFirstReply = TRUE;
	mParcelObjectBonus = getChild<LLTextBox>("parcel_object_bonus");
	mSWTotalObjects = getChild<LLTextBox>("objects_available");
	mObjectContribution = getChild<LLTextBox>("object_contrib_text");
	mTotalObjects = getChild<LLTextBox>("total_objects_text");
	mOwnerObjects = getChild<LLTextBox>("owner_objects_text");
	
	mBtnShowOwnerObjects = getChild<LLButton>("ShowOwner");
	mBtnShowOwnerObjects->setClickedCallback(onClickShowOwnerObjects, this);
	
	mBtnReturnOwnerObjects = getChild<LLButton>("ReturnOwner...");
	mBtnReturnOwnerObjects->setClickedCallback(onClickReturnOwnerObjects, this);
	
	mGroupObjects = getChild<LLTextBox>("group_objects_text");
	mBtnShowGroupObjects = getChild<LLButton>("ShowGroup");
	mBtnShowGroupObjects->setClickedCallback(onClickShowGroupObjects, this);
	
	mBtnReturnGroupObjects = getChild<LLButton>("ReturnGroup...");
	mBtnReturnGroupObjects->setClickedCallback(onClickReturnGroupObjects, this);
	
	mOtherObjects = getChild<LLTextBox>("other_objects_text");
	mBtnShowOtherObjects = getChild<LLButton>("ShowOther");
	mBtnShowOtherObjects->setClickedCallback(onClickShowOtherObjects, this);
	
	mBtnReturnOtherObjects = getChild<LLButton>("ReturnOther...");
	mBtnReturnOtherObjects->setClickedCallback(onClickReturnOtherObjects, this);
	
	mSelectedObjects = getChild<LLTextBox>("selected_objects_text");
	mCleanOtherObjectsTime = getChild<LLLineEditor>("clean other time");

	mCleanOtherObjectsTime->setFocusLostCallback(onLostFocus, this);
	mCleanOtherObjectsTime->setCommitCallback(onCommitClean);

	childSetPrevalidate("clean other time", LLLineEditor::prevalidateNonNegativeS32);
	childSetUserData("clean other time", this);
	
	mBtnRefresh = getChild<LLButton>("Refresh List");
	mBtnRefresh->setClickedCallback(onClickRefresh, this);
	
	mBtnReturnOwnerList = getChild<LLButton>("Return objects...");
	mBtnReturnOwnerList->setClickedCallback(onClickReturnOwnerList, this);

	mIconAvatarOnline = LLUIImageList::getInstance()->getUIImage("icon_avatar_online.tga");
	mIconAvatarOffline = LLUIImageList::getInstance()->getUIImage("icon_avatar_offline.tga");
	mIconGroup = LLUIImageList::getInstance()->getUIImage("icon_group.tga");

	mOwnerList = getChild<LLNameListCtrl>("owner list");
	mOwnerList->sortByColumnIndex(3, FALSE);
	childSetCommitCallback("owner list", onCommitList, this);
	mOwnerList->setDoubleClickCallback(onDoubleClickOwner);

	return TRUE;
}




// virtual
LLPanelLandObjects::~LLPanelLandObjects()
{ }

// static
void LLPanelLandObjects::onDoubleClickOwner(void *userdata)
{
	LLPanelLandObjects *self = (LLPanelLandObjects *)userdata;

	LLScrollListItem* item = self->mOwnerList->getFirstSelected();
	if (item)
	{
		LLUUID owner_id = item->getUUID();
		// Look up the selected name, for future dialog box use.
		const LLScrollListCell* cell;
		cell = item->getColumn(1);
		if (!cell)
		{
			return;
		}
		// Is this a group?
		BOOL is_group = cell->getValue().asString() == OWNER_GROUP;
		if (is_group)
		{
			LLFloaterGroupInfo::showFromUUID(owner_id);
		}
		else
		{
			LLFloaterAvatarInfo::showFromDirectory(owner_id);
		}
	}
}

// public
void LLPanelLandObjects::refresh()
{
	LLParcel *parcel = mParcel->getParcel();

	mBtnShowOwnerObjects->setEnabled(FALSE);
	mBtnShowGroupObjects->setEnabled(FALSE);
	mBtnShowOtherObjects->setEnabled(FALSE);
	mBtnReturnOwnerObjects->setEnabled(FALSE);
	mBtnReturnGroupObjects->setEnabled(FALSE);
	mBtnReturnOtherObjects->setEnabled(FALSE);
	mCleanOtherObjectsTime->setEnabled(FALSE);
	mBtnRefresh->			setEnabled(FALSE);
	mBtnReturnOwnerList->	setEnabled(FALSE);

	mSelectedOwners.clear();
	mOwnerList->deleteAllItems();
	mOwnerList->setEnabled(FALSE);

	if (!parcel)
	{
		mSWTotalObjects->setTextArg("[COUNT]", llformat("%d", 0));
		mSWTotalObjects->setTextArg("[TOTAL]", llformat("%d", 0));
		mSWTotalObjects->setTextArg("[AVAILABLE]", llformat("%d", 0));
		mObjectContribution->setTextArg("[COUNT]", llformat("%d", 0));
		mTotalObjects->setTextArg("[COUNT]", llformat("%d", 0));
		mOwnerObjects->setTextArg("[COUNT]", llformat("%d", 0));
		mGroupObjects->setTextArg("[COUNT]", llformat("%d", 0));
		mOtherObjects->setTextArg("[COUNT]", llformat("%d", 0));
		mSelectedObjects->setTextArg("[COUNT]", llformat("%d", 0));
	}
	else
	{
		S32 sw_max = parcel->getSimWideMaxPrimCapacity();
		S32 sw_total = parcel->getSimWidePrimCount();
		S32 max = llround(parcel->getMaxPrimCapacity() * parcel->getParcelPrimBonus());
		S32 total = parcel->getPrimCount();
		S32 owned = parcel->getOwnerPrimCount();
		S32 group = parcel->getGroupPrimCount();
		S32 other = parcel->getOtherPrimCount();
		S32 selected = parcel->getSelectedPrimCount();
		F32 parcel_object_bonus = parcel->getParcelPrimBonus();
		mOtherTime = parcel->getCleanOtherTime();

		// Can't have more than region max tasks, regardless of parcel
		// object bonus factor.
		LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
		if (region)
		{
			S32 max_tasks_per_region = (S32)region->getMaxTasks();
			sw_max = llmin(sw_max, max_tasks_per_region);
			max = llmin(max, max_tasks_per_region);
		}

		if (parcel_object_bonus != 1.0f)
		{
			mParcelObjectBonus->setVisible(TRUE);
			mParcelObjectBonus->setTextArg("[BONUS]", llformat("%.2f", parcel_object_bonus));
		}
		else
		{
			mParcelObjectBonus->setVisible(FALSE);
		}

		if (sw_total > sw_max)
		{
			mSWTotalObjects->setText(getString("objects_deleted_text"));
			mSWTotalObjects->setTextArg("[DELETED]", llformat("%d", sw_total - sw_max));
		}
		else
		{
			mSWTotalObjects->setText(getString("objects_available_text"));
			mSWTotalObjects->setTextArg("[AVAILABLE]", llformat("%d", sw_max - sw_total));
		}
		mSWTotalObjects->setTextArg("[COUNT]", llformat("%d", sw_total));
		mSWTotalObjects->setTextArg("[MAX]", llformat("%d", sw_max));

		mObjectContribution->setTextArg("[COUNT]", llformat("%d", max));
		mTotalObjects->setTextArg("[COUNT]", llformat("%d", total));
		mOwnerObjects->setTextArg("[COUNT]", llformat("%d", owned));
		mGroupObjects->setTextArg("[COUNT]", llformat("%d", group));
		mOtherObjects->setTextArg("[COUNT]", llformat("%d", other));
		mSelectedObjects->setTextArg("[COUNT]", llformat("%d", selected));
		mCleanOtherObjectsTime->setText(llformat("%d", mOtherTime));

		BOOL can_return_owned = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_RETURN_GROUP_OWNED);
		BOOL can_return_group_set = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_RETURN_GROUP_SET);
		BOOL can_return_other = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_RETURN_NON_GROUP);

		if (can_return_owned || can_return_group_set || can_return_other)
		{
			if (owned && can_return_owned)
			{
				mBtnShowOwnerObjects->setEnabled(TRUE);
				mBtnReturnOwnerObjects->setEnabled(TRUE);
			}
			if (group && can_return_group_set)
			{
				mBtnShowGroupObjects->setEnabled(TRUE);
				mBtnReturnGroupObjects->setEnabled(TRUE);
			}
			if (other && can_return_other)
			{
				mBtnShowOtherObjects->setEnabled(TRUE);
				mBtnReturnOtherObjects->setEnabled(TRUE);
			}

			mCleanOtherObjectsTime->setEnabled(TRUE);
			mBtnRefresh->setEnabled(TRUE);
		}
	}
}

// virtual
void LLPanelLandObjects::draw()
{
	LLPanel::draw();
}

void send_other_clean_time_message(S32 parcel_local_id, S32 other_clean_time)
{
	LLMessageSystem *msg = gMessageSystem;

	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if (!region) return;

	msg->newMessageFast(_PREHASH_ParcelSetOtherCleanTime);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
	msg->addS32Fast(_PREHASH_OtherCleanTime, other_clean_time);

	msg->sendReliable(region->getHost());
}

void send_return_objects_message(S32 parcel_local_id, S32 return_type, 
								 uuid_list_t* owner_ids = NULL)
{
	LLMessageSystem *msg = gMessageSystem;

	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if (!region) return;

	msg->newMessageFast(_PREHASH_ParcelReturnObjects);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel_local_id);
	msg->addU32Fast(_PREHASH_ReturnType, (U32) return_type);

	// Dummy task id, not used
	msg->nextBlock("TaskIDs");
	msg->addUUID("TaskID", LLUUID::null);

	// Throw all return ids into the packet.
	// TODO: Check for too many ids.
	if (owner_ids)
	{
		uuid_list_t::iterator end = owner_ids->end();
		for (uuid_list_t::iterator it = owner_ids->begin();
			 it != end;
			 ++it)
		{
			msg->nextBlockFast(_PREHASH_OwnerIDs);
			msg->addUUIDFast(_PREHASH_OwnerID, (*it));
		}
	}
	else
	{
		msg->nextBlockFast(_PREHASH_OwnerIDs);
		msg->addUUIDFast(_PREHASH_OwnerID, LLUUID::null);
	}

	msg->sendReliable(region->getHost());
}

// static
void LLPanelLandObjects::callbackReturnOwnerObjects(S32 option, void* userdata)
{
	LLPanelLandObjects	*lop = (LLPanelLandObjects *)userdata;
	LLParcel *parcel = lop->mParcel->getParcel();
	if (0 == option)
	{
		if (parcel)
		{
			LLUUID owner_id = parcel->getOwnerID();	
			LLStringUtil::format_map_t args;
			if (owner_id == gAgentID)
			{
				LLNotifyBox::showXml("OwnedObjectsReturned");
			}
			else
			{
				std::string first, last;
				gCacheName->getName(owner_id, first, last);
				args["[FIRST]"] = first;
				args["[LAST]"] = last;
				LLNotifyBox::showXml("OtherObjectsReturned", args);
			}
			send_return_objects_message(parcel->getLocalID(), RT_OWNER);
		}
	}

	LLSelectMgr::getInstance()->unhighlightAll();
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
	lop->refresh();
}

// static
void LLPanelLandObjects::callbackReturnGroupObjects(S32 option, void* userdata)
{
	LLPanelLandObjects	*lop = (LLPanelLandObjects *)userdata;
	LLParcel *parcel = lop->mParcel->getParcel();
	if (0 == option)
	{
		if (parcel)
		{
			std::string group_name;
			gCacheName->getGroupName(parcel->getGroupID(), group_name);
			LLStringUtil::format_map_t args;
			args["[GROUPNAME]"] = group_name;
			LLNotifyBox::showXml("GroupObjectsReturned", args);
			send_return_objects_message(parcel->getLocalID(), RT_GROUP);
		}
	}
	LLSelectMgr::getInstance()->unhighlightAll();
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
	lop->refresh();
}

// static
void LLPanelLandObjects::callbackReturnOtherObjects(S32 option, void* userdata)
{
	LLPanelLandObjects	*lop = (LLPanelLandObjects *)userdata;
	LLParcel *parcel = lop->mParcel->getParcel();
	if (0 == option)
	{
		if (parcel)
		{
			LLNotifyBox::showXml("UnOwnedObjectsReturned");
			send_return_objects_message(parcel->getLocalID(), RT_OTHER);
		}
	}
	LLSelectMgr::getInstance()->unhighlightAll();
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
	lop->refresh();
}

// static
void LLPanelLandObjects::callbackReturnOwnerList(S32 option, void* userdata)
{
	LLPanelLandObjects	*self = (LLPanelLandObjects *)userdata;
	LLParcel *parcel = self->mParcel->getParcel();
	if (0 == option)
	{
		if (parcel)
		{
			// Make sure we have something selected.
			uuid_list_t::iterator selected = self->mSelectedOwners.begin();
			if (selected != self->mSelectedOwners.end())
			{
				LLStringUtil::format_map_t args;
				if (self->mSelectedIsGroup)
				{
					args["[GROUPNAME]"] = self->mSelectedName;
					LLNotifyBox::showXml("GroupObjectsReturned", args);
				}
				else
				{
					args["[NAME]"] = self->mSelectedName;
					LLNotifyBox::showXml("OtherObjectsReturned2", args);
				}

				send_return_objects_message(parcel->getLocalID(), RT_LIST, &(self->mSelectedOwners));
			}
		}
	}
	LLSelectMgr::getInstance()->unhighlightAll();
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );
	self->refresh();
}


// static
void LLPanelLandObjects::onClickReturnOwnerList(void* userdata)
{
	LLPanelLandObjects	*self = (LLPanelLandObjects *)userdata;

	LLParcel* parcelp = self->mParcel->getParcel();
	if (!parcelp) return;

	// Make sure we have something selected.
	if (self->mSelectedOwners.empty())
	{
		return;
	}
	//uuid_list_t::iterator selected_itr = self->mSelectedOwners.begin();
	//if (selected_itr == self->mSelectedOwners.end()) return;

	send_parcel_select_objects(parcelp->getLocalID(), RT_LIST, &(self->mSelectedOwners));

	LLStringUtil::format_map_t args;
	args["[NAME]"] = self->mSelectedName;
	args["[N]"] = llformat("%d",self->mSelectedCount);
	if (self->mSelectedIsGroup)
	{
		gViewerWindow->alertXml("ReturnObjectsDeededToGroup", args, callbackReturnOwnerList, userdata);	
	}
	else 
	{
		gViewerWindow->alertXml("ReturnObjectsOwnedByUser", args, callbackReturnOwnerList, userdata);	
	}
}


// static
void LLPanelLandObjects::onClickRefresh(void* userdata)
{
	LLPanelLandObjects *self = (LLPanelLandObjects*)userdata;

	LLMessageSystem *msg = gMessageSystem;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if (!region) return;

	// ready the list for results
	self->mOwnerList->deleteAllItems();
	self->mOwnerList->addCommentText(std::string("Searching...")); // *TODO: Translate
	self->mOwnerList->setEnabled(FALSE);
	self->mFirstReply = TRUE;

	// send the message
	msg->newMessageFast(_PREHASH_ParcelObjectOwnersRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel->getLocalID());

	msg->sendReliable(region->getHost());
}

// static
void LLPanelLandObjects::processParcelObjectOwnersReply(LLMessageSystem *msg, void **)
{
	LLPanelLandObjects* self = LLFloaterLand::getCurrentPanelLandObjects();

	if (!self)
	{
		llwarns << "Received message for nonexistent LLPanelLandObject"
				<< llendl;
		return;
	}
	
	const LLFontGL* FONT = LLFontGL::sSansSerif;

	// Extract all of the owners.
	S32 rows = msg->getNumberOfBlocksFast(_PREHASH_Data);
	//uuid_list_t return_ids;
	LLUUID	owner_id;
	BOOL	is_group_owned;
	S32		object_count;
	U32		most_recent_time = 0;
	BOOL	is_online;
	std::string object_count_str;
	//BOOL b_need_refresh = FALSE;

	// If we were waiting for the first reply, clear the "Searching..." text.
	if (self->mFirstReply)
	{
		self->mOwnerList->deleteAllItems();
		self->mFirstReply = FALSE;
	}

	for(S32 i = 0; i < rows; ++i)
	{
		msg->getUUIDFast(_PREHASH_Data, _PREHASH_OwnerID,		owner_id,		i);
		msg->getBOOLFast(_PREHASH_Data, _PREHASH_IsGroupOwned,	is_group_owned,	i);
		msg->getS32Fast (_PREHASH_Data, _PREHASH_Count,			object_count,	i);
		msg->getBOOLFast(_PREHASH_Data, _PREHASH_OnlineStatus,	is_online,		i);
		if(msg->has("DataExtended"))
		{
			msg->getU32("DataExtended", "TimeStamp", most_recent_time, i);
		}
		if (owner_id.isNull())
		{
			continue;
		}

		LLScrollListItem *row = new LLScrollListItem( TRUE, NULL, owner_id);
		if (is_group_owned)
		{
			row->addColumn(self->mIconGroup);
			row->addColumn(OWNER_GROUP, FONT);
		}
		else if (is_online)
		{
			row->addColumn(self->mIconAvatarOnline);
			row->addColumn(OWNER_ONLINE, FONT);
		}
		else  // offline
		{
			row->addColumn(self->mIconAvatarOffline);
			row->addColumn(OWNER_OFFLINE, FONT);
		}
		// Placeholder for name.
		row->addColumn(LLStringUtil::null, FONT);

		object_count_str = llformat("%d", object_count);
		row->addColumn(object_count_str, FONT);
		
		row->addColumn(formatted_time((time_t)most_recent_time), FONT);


		if (is_group_owned)
		{
			self->mOwnerList->addGroupNameItem(row, ADD_BOTTOM);
		}
		else
		{
			self->mOwnerList->addNameItem(row, ADD_BOTTOM);
		}

		lldebugs << "object owner " << owner_id << " (" << (is_group_owned ? "group" : "agent")
				<< ") owns " << object_count << " objects." << llendl;
	}
	// check for no results
	if (0 == self->mOwnerList->getItemCount())
	{
		self->mOwnerList->addCommentText(std::string("None found.")); // *TODO: Translate
	}
	else
	{
		self->mOwnerList->setEnabled(TRUE);
	}
}

// static
void LLPanelLandObjects::onCommitList(LLUICtrl* ctrl, void* data)
{
	LLPanelLandObjects* self = (LLPanelLandObjects*)data;

	if (FALSE == self->mOwnerList->getCanSelect())
	{
		return;
	}
	LLScrollListItem *item = self->mOwnerList->getFirstSelected();
	if (item)
	{
		// Look up the selected name, for future dialog box use.
		const LLScrollListCell* cell;
		cell = item->getColumn(1);
		if (!cell)
		{
			return;
		}
		// Is this a group?
		self->mSelectedIsGroup = cell->getValue().asString() == OWNER_GROUP;
		cell = item->getColumn(2);
		self->mSelectedName = cell->getValue().asString();
		cell = item->getColumn(3);
		self->mSelectedCount = atoi(cell->getValue().asString().c_str());

		// Set the selection, and enable the return button.
		self->mSelectedOwners.clear();
		self->mSelectedOwners.insert(item->getUUID());
		self->mBtnReturnOwnerList->setEnabled(TRUE);

		// Highlight this user's objects
		clickShowCore(self, RT_LIST, &(self->mSelectedOwners));
	}
}

// static
void LLPanelLandObjects::clickShowCore(LLPanelLandObjects* self, S32 return_type, uuid_list_t* list)
{
	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	send_parcel_select_objects(parcel->getLocalID(), return_type, list);
}

// static
void LLPanelLandObjects::onClickShowOwnerObjects(void* userdata)
{
	clickShowCore((LLPanelLandObjects*)userdata, RT_OWNER);
}

// static
void LLPanelLandObjects::onClickShowGroupObjects(void* userdata)
{
	clickShowCore((LLPanelLandObjects*)userdata, (RT_GROUP));
}

// static
void LLPanelLandObjects::onClickShowOtherObjects(void* userdata)
{
	clickShowCore((LLPanelLandObjects*)userdata, RT_OTHER);
}

// static
void LLPanelLandObjects::onClickReturnOwnerObjects(void* userdata)
{
	S32 owned = 0;

	LLPanelLandObjects* panelp = (LLPanelLandObjects*)userdata;
	LLParcel* parcel = panelp->mParcel->getParcel();
	if (!parcel) return;

	owned = parcel->getOwnerPrimCount();

	send_parcel_select_objects(parcel->getLocalID(), RT_OWNER);

	LLUUID owner_id = parcel->getOwnerID();
	
	LLStringUtil::format_map_t args;
	args["[N]"] = llformat("%d",owned);

	if (owner_id == gAgent.getID())
	{
		gViewerWindow->alertXml("ReturnObjectsOwnedBySelf", args, callbackReturnOwnerObjects, userdata);
	}
	else
	{
		std::string name;
		gCacheName->getFullName(owner_id, name);
		args["[NAME]"] = name;
		gViewerWindow->alertXml("ReturnObjectsOwnedByUser", args, callbackReturnOwnerObjects, userdata);
	}
}

// static
void LLPanelLandObjects::onClickReturnGroupObjects(void* userdata)
{
	LLPanelLandObjects* panelp = (LLPanelLandObjects*)userdata;
	LLParcel* parcel = panelp->mParcel->getParcel();
	if (!parcel) return;

	send_parcel_select_objects(parcel->getLocalID(), RT_GROUP);

	std::string group_name;
	gCacheName->getGroupName(parcel->getGroupID(), group_name);
	
	LLStringUtil::format_map_t args;
	args["[NAME]"] = group_name;
	args["[N]"] = llformat("%d", parcel->getGroupPrimCount());

	// create and show confirmation textbox
	gViewerWindow->alertXml("ReturnObjectsDeededToGroup", args, callbackReturnGroupObjects, userdata);
}

// static
void LLPanelLandObjects::onClickReturnOtherObjects(void* userdata)
{
	S32 other = 0;

	LLPanelLandObjects* panelp = (LLPanelLandObjects*)userdata;
	LLParcel* parcel = panelp->mParcel->getParcel();
	if (!parcel) return;
	
	other = parcel->getOtherPrimCount();

	send_parcel_select_objects(parcel->getLocalID(), RT_OTHER);

	LLStringUtil::format_map_t args;
	args["[N]"] = llformat("%d", other);
	
	if (parcel->getIsGroupOwned())
	{
		std::string group_name;
		gCacheName->getGroupName(parcel->getGroupID(), group_name);
		args["[NAME]"] = group_name;

		gViewerWindow->alertXml("ReturnObjectsNotOwnedByGroup", args, callbackReturnOtherObjects, userdata);
	}
	else
	{
		LLUUID owner_id = parcel->getOwnerID();

		if (owner_id == gAgent.getID())
		{
			gViewerWindow->alertXml("ReturnObjectsNotOwnedBySelf", args, callbackReturnOtherObjects, userdata);
		}
		else
		{
			std::string name;
			gCacheName->getFullName(owner_id, name);
			args["[NAME]"] = name;

			gViewerWindow->alertXml("ReturnObjectsNotOwnedByUser", args, callbackReturnOtherObjects, userdata);
		}
	}
}

// static
void LLPanelLandObjects::onLostFocus(LLFocusableElement* caller, void* user_data)
{
	onCommitClean((LLUICtrl*)caller, user_data);
}

// static
void LLPanelLandObjects::onCommitClean(LLUICtrl *caller, void* user_data)
{
	LLPanelLandObjects	*lop = (LLPanelLandObjects *)user_data;
	LLParcel* parcel = lop->mParcel->getParcel();
	if (parcel)
	{
		lop->mOtherTime = atoi(lop->mCleanOtherObjectsTime->getText().c_str());

		parcel->setCleanOtherTime(lop->mOtherTime);
		send_other_clean_time_message(parcel->getLocalID(), lop->mOtherTime);
	}
}


//---------------------------------------------------------------------------
// LLPanelLandOptions
//---------------------------------------------------------------------------

LLPanelLandOptions::LLPanelLandOptions(LLParcelSelectionHandle& parcel)
:	LLPanel(std::string("land_options_panel")),
	mCheckEditObjects(NULL),
	mCheckEditGroupObjects(NULL),
	mCheckAllObjectEntry(NULL),
	mCheckGroupObjectEntry(NULL),
	mCheckEditLand(NULL),
	mCheckSafe(NULL),
	mCheckFly(NULL),
	mCheckGroupScripts(NULL),
	mCheckOtherScripts(NULL),
	mCheckLandmark(NULL),
	mCheckShowDirectory(NULL),
	mCategoryCombo(NULL),
	mLandingTypeCombo(NULL),
	mSnapshotCtrl(NULL),
	mLocationText(NULL),
	mSetBtn(NULL),
	mClearBtn(NULL),
	mMatureCtrl(NULL),
	mPushRestrictionCtrl(NULL),
	mPublishHelpButton(NULL),
	mParcel(parcel)
{
}


BOOL LLPanelLandOptions::postBuild()
{

	
	mCheckEditObjects = getChild<LLCheckBoxCtrl>( "edit objects check");
	childSetCommitCallback("edit objects check", onCommitAny, this);
	
	mCheckEditGroupObjects = getChild<LLCheckBoxCtrl>( "edit group objects check");
	childSetCommitCallback("edit group objects check", onCommitAny, this);

	mCheckAllObjectEntry = getChild<LLCheckBoxCtrl>( "all object entry check");
	childSetCommitCallback("all object entry check", onCommitAny, this);

	mCheckGroupObjectEntry = getChild<LLCheckBoxCtrl>( "group object entry check");
	childSetCommitCallback("group object entry check", onCommitAny, this);
	
	mCheckEditLand = getChild<LLCheckBoxCtrl>( "edit land check");
	childSetCommitCallback("edit land check", onCommitAny, this);

	
	mCheckLandmark = getChild<LLCheckBoxCtrl>( "check landmark");
	childSetCommitCallback("check landmark", onCommitAny, this);

	
	mCheckGroupScripts = getChild<LLCheckBoxCtrl>( "check group scripts");
	childSetCommitCallback("check group scripts", onCommitAny, this);

	
	mCheckFly = getChild<LLCheckBoxCtrl>( "check fly");
	childSetCommitCallback("check fly", onCommitAny, this);

	
	mCheckOtherScripts = getChild<LLCheckBoxCtrl>( "check other scripts");
	childSetCommitCallback("check other scripts", onCommitAny, this);

	
	mCheckSafe = getChild<LLCheckBoxCtrl>( "check safe");
	childSetCommitCallback("check safe", onCommitAny, this);

	
	mPushRestrictionCtrl = getChild<LLCheckBoxCtrl>( "PushRestrictCheck");
	childSetCommitCallback("PushRestrictCheck", onCommitAny, this);

	mCheckShowDirectory = getChild<LLCheckBoxCtrl>( "ShowDirectoryCheck");
	childSetCommitCallback("ShowDirectoryCheck", onCommitAny, this);

	
	mCategoryCombo = getChild<LLComboBox>( "land category");
	childSetCommitCallback("land category", onCommitAny, this);

	mMatureCtrl = getChild<LLCheckBoxCtrl>( "MatureCheck");
	childSetCommitCallback("MatureCheck", onCommitAny, this);
	
	mPublishHelpButton = getChild<LLButton>("?");
	mPublishHelpButton->setClickedCallback(onClickPublishHelp, this);


	if (gAgent.wantsPGOnly())
	{
		// Disable these buttons if they are PG (Teen) users
		mPublishHelpButton->setVisible(FALSE);
		mPublishHelpButton->setEnabled(FALSE);
		mMatureCtrl->setVisible(FALSE);
		mMatureCtrl->setEnabled(FALSE);
	}

		// Load up the category list
	//now in xml file
	/*
	S32 i;
	for (i = 0; i < LLParcel::C_COUNT; i++)
	{
		LLParcel::ECategory cat = (LLParcel::ECategory)i;

		// Selecting Linden Location when you're not a god
		// is also blocked on the server.
		BOOL enabled = TRUE;
		if (!gAgent.isGodlike()
			&& i == LLParcel::C_LINDEN) 
		{
			enabled = FALSE;
		}

		mCategoryCombo->add( LLParcel::getCategoryUIString(cat), ADD_BOTTOM, enabled );
	}*/

	
	mSnapshotCtrl = getChild<LLTextureCtrl>("snapshot_ctrl");
	if (mSnapshotCtrl)
	{
		mSnapshotCtrl->setCommitCallback( onCommitAny );
		mSnapshotCtrl->setCallbackUserData( this );
		mSnapshotCtrl->setAllowNoTexture ( TRUE );
		mSnapshotCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
		mSnapshotCtrl->setNonImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
	}
	else
	{
		llwarns << "LLUICtrlFactory::getTexturePickerByName() returned NULL for 'snapshot_ctrl'" << llendl;
	}


	mLocationText = getChild<LLTextBox>("landing_point");

	mSetBtn = getChild<LLButton>("Set");
	mSetBtn->setClickedCallback(onClickSet, this);

	
	mClearBtn = getChild<LLButton>("Clear");
	mClearBtn->setClickedCallback(onClickClear, this);


	mLandingTypeCombo = getChild<LLComboBox>( "landing type");
	childSetCommitCallback("landing type", onCommitAny, this);

	return TRUE;
}


// virtual
LLPanelLandOptions::~LLPanelLandOptions()
{ }


// public
void LLPanelLandOptions::refresh()
{
	LLParcel *parcel = mParcel->getParcel();
	
	if (!parcel)
	{
		mCheckEditObjects	->set(FALSE);
		mCheckEditObjects	->setEnabled(FALSE);

		mCheckEditGroupObjects	->set(FALSE);
		mCheckEditGroupObjects	->setEnabled(FALSE);

		mCheckAllObjectEntry	->set(FALSE);
		mCheckAllObjectEntry	->setEnabled(FALSE);

		mCheckGroupObjectEntry	->set(FALSE);
		mCheckGroupObjectEntry	->setEnabled(FALSE);

		mCheckEditLand		->set(FALSE);
		mCheckEditLand		->setEnabled(FALSE);

		mCheckSafe			->set(FALSE);
		mCheckSafe			->setEnabled(FALSE);

		mCheckFly			->set(FALSE);
		mCheckFly			->setEnabled(FALSE);

		mCheckLandmark		->set(FALSE);
		mCheckLandmark		->setEnabled(FALSE);

		mCheckGroupScripts	->set(FALSE);
		mCheckGroupScripts	->setEnabled(FALSE);

		mCheckOtherScripts	->set(FALSE);
		mCheckOtherScripts	->setEnabled(FALSE);

		mCheckShowDirectory	->set(FALSE);
		mCheckShowDirectory	->setEnabled(FALSE);

		mPushRestrictionCtrl->set(FALSE);
		mPushRestrictionCtrl->setEnabled(FALSE);

		// *TODO:Translate
		const std::string& none_string = LLParcel::getCategoryUIString(LLParcel::C_NONE);
		mCategoryCombo->setSimple(none_string);
		mCategoryCombo->setEnabled(FALSE);

		mLandingTypeCombo->setCurrentByIndex(0);
		mLandingTypeCombo->setEnabled(FALSE);

		mSnapshotCtrl->setImageAssetID(LLUUID::null);
		mSnapshotCtrl->setEnabled(FALSE);

		mLocationText->setTextArg("[LANDING]", getString("landing_point_none"));
		mSetBtn->setEnabled(FALSE);
		mClearBtn->setEnabled(FALSE);

		mMatureCtrl->setEnabled(FALSE);
		mPublishHelpButton->setEnabled(FALSE);
	}
	else
	{
		// something selected, hooray!

		// Display options
		BOOL can_change_options = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_OPTIONS);
		mCheckEditObjects	->set( parcel->getAllowModify() );
		mCheckEditObjects	->setEnabled( can_change_options );
		
		mCheckEditGroupObjects	->set( parcel->getAllowGroupModify() ||  parcel->getAllowModify());
		mCheckEditGroupObjects	->setEnabled( can_change_options && !parcel->getAllowModify() );		// If others edit is enabled, then this is explicitly enabled.

		mCheckAllObjectEntry	->set( parcel->getAllowAllObjectEntry() );
		mCheckAllObjectEntry	->setEnabled( can_change_options );

		mCheckGroupObjectEntry	->set( parcel->getAllowGroupObjectEntry() ||  parcel->getAllowAllObjectEntry());
		mCheckGroupObjectEntry	->setEnabled( can_change_options && !parcel->getAllowAllObjectEntry() );

		BOOL can_change_terraform = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_EDIT);
		mCheckEditLand		->set( parcel->getAllowTerraform() );
		mCheckEditLand		->setEnabled( can_change_terraform );

		mCheckSafe			->set( !parcel->getAllowDamage() );
		mCheckSafe			->setEnabled( can_change_options );

		mCheckFly			->set( parcel->getAllowFly() );
		mCheckFly			->setEnabled( can_change_options );

		mCheckLandmark		->set( parcel->getAllowLandmark() );
		mCheckLandmark		->setEnabled( can_change_options );

		mCheckGroupScripts	->set( parcel->getAllowGroupScripts() || parcel->getAllowOtherScripts());
		mCheckGroupScripts	->setEnabled( can_change_options && !parcel->getAllowOtherScripts());

		mCheckOtherScripts	->set( parcel->getAllowOtherScripts() );
		mCheckOtherScripts	->setEnabled( can_change_options );

		mPushRestrictionCtrl->set( parcel->getRestrictPushObject() );
		if(parcel->getRegionPushOverride())
		{
			mPushRestrictionCtrl->setLabel(getString("push_restrict_region_text"));
			mPushRestrictionCtrl->setEnabled(false);
			mPushRestrictionCtrl->set(TRUE);
		}
		else
		{
			mPushRestrictionCtrl->setLabel(getString("push_restrict_text"));
			mPushRestrictionCtrl->setEnabled(can_change_options);
		}

		BOOL can_change_identity = 
			LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_CHANGE_IDENTITY);
		// Set by string in case the order in UI doesn't match the order by index.
		// *TODO:Translate
		LLParcel::ECategory cat = parcel->getCategory();
		const std::string& category_string = LLParcel::getCategoryUIString(cat);
		mCategoryCombo->setSimple(category_string);
		mCategoryCombo->setEnabled( can_change_identity );

		BOOL can_change_landing_point = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, 
														GP_LAND_SET_LANDING_POINT);
		mLandingTypeCombo->setCurrentByIndex((S32)parcel->getLandingType());
		mLandingTypeCombo->setEnabled( can_change_landing_point );

		mSnapshotCtrl->setImageAssetID(parcel->getSnapshotID());
		mSnapshotCtrl->setEnabled( can_change_identity );

		LLVector3 pos = parcel->getUserLocation();
		if (pos.isExactlyZero())
		{
			mLocationText->setTextArg("[LANDING]", getString("landing_point_none"));
		}
		else
		{
			mLocationText->setTextArg("[LANDING]",llformat("%d, %d, %d",
														   llround(pos.mV[VX]),
														   llround(pos.mV[VY]),
														   llround(pos.mV[VZ])));
		}

		mSetBtn->setEnabled( can_change_landing_point );
		mClearBtn->setEnabled( can_change_landing_point );

		mMatureCtrl->set(parcel->getMaturePublish());
		mMatureCtrl->setEnabled( can_change_identity );
		mPublishHelpButton->setEnabled( can_change_identity );

		if (gAgent.wantsPGOnly())
		{
			// Disable these buttons if they are PG (Teen) users
			mPublishHelpButton->setVisible(FALSE);
			mPublishHelpButton->setEnabled(FALSE);
			mMatureCtrl->setVisible(FALSE);
			mMatureCtrl->setEnabled(FALSE);
		}
		else
		{
			// not teen so fill in the data for the maturity control
			mMatureCtrl->setVisible(TRUE);
			// they can see the checkbox, but its disposition depends on the 
			// state of the region
			LLViewerRegion* regionp = LLViewerParcelMgr::getInstance()->getSelectionRegion();
			if (regionp)
			{
				if (regionp->getSimAccess() == SIM_ACCESS_PG)
				{
					mMatureCtrl->setEnabled(FALSE);
					mMatureCtrl->set(FALSE);
				}
				else if (regionp->getSimAccess() == SIM_ACCESS_MATURE)
				{
					mMatureCtrl->setEnabled(can_change_identity);
					mMatureCtrl->set(parcel->getMaturePublish());
				}
				else if (regionp->getSimAccess() == SIM_ACCESS_ADULT)
				{
					mMatureCtrl->setEnabled(FALSE);
					mMatureCtrl->set(TRUE);
				}
			}
		}
	}
}

// virtual
void LLPanelLandOptions::draw()
{
	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection()->getParcel();
	
	if(parcel)
	{
		LLViewerRegion* region;
		region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
		llassert(region); // Region should never be null.

		BOOL can_change_identity = region ? 
			LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_CHANGE_IDENTITY) &&
			! (region->getRegionFlags() & REGION_FLAGS_BLOCK_PARCEL_SEARCH) : false;

		// There is a bug with this panel whereby the Show Directory bit can be 
		// slammed off by the Region based on an override.  Since this data is cached
		// locally the change will not reflect in the panel, which could cause confusion
		// A workaround for this is to flip the bit off in the locally cached version
		// when we detect a mismatch case.
		if(! can_change_identity && parcel->getParcelFlag(PF_SHOW_DIRECTORY))
		{
			parcel->setParcelFlag(PF_SHOW_DIRECTORY, FALSE);
		}
		mCheckShowDirectory	->set(parcel->getParcelFlag(PF_SHOW_DIRECTORY));
		mCheckShowDirectory	->setEnabled(can_change_identity);
		mCategoryCombo->setEnabled(can_change_identity);
	}

	LLPanel::draw();


}
// static
void LLPanelLandOptions::onCommitAny(LLUICtrl *ctrl, void *userdata)
{
	LLPanelLandOptions *self = (LLPanelLandOptions *)userdata;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	BOOL create_objects		= self->mCheckEditObjects->get();
	BOOL create_group_objects	= self->mCheckEditGroupObjects->get() || self->mCheckEditObjects->get();
	BOOL all_object_entry		= self->mCheckAllObjectEntry->get();
	BOOL group_object_entry	= self->mCheckGroupObjectEntry->get() || self->mCheckAllObjectEntry->get();
	BOOL allow_terraform	= self->mCheckEditLand->get();
	BOOL allow_damage		= !self->mCheckSafe->get();
	BOOL allow_fly			= self->mCheckFly->get();
	BOOL allow_landmark		= self->mCheckLandmark->get();
	BOOL allow_group_scripts	= self->mCheckGroupScripts->get() || self->mCheckOtherScripts->get();
	BOOL allow_other_scripts	= self->mCheckOtherScripts->get();
	BOOL allow_publish		= FALSE;
	BOOL mature_publish		= self->mMatureCtrl->get();
	BOOL push_restriction	= self->mPushRestrictionCtrl->get();
	BOOL show_directory		= self->mCheckShowDirectory->get();
	S32  category_index		= self->mCategoryCombo->getCurrentIndex();
	S32  landing_type_index	= self->mLandingTypeCombo->getCurrentIndex();
	LLUUID snapshot_id		= self->mSnapshotCtrl->getImageAssetID();
	LLViewerRegion* region;
	region = LLViewerParcelMgr::getInstance()->getSelectionRegion();

	if (!allow_other_scripts && region && region->getAllowDamage())
	{

		gViewerWindow->alertXml("UnableToDisableOutsideScripts");
		return;
	}

	// Push data into current parcel
	parcel->setParcelFlag(PF_CREATE_OBJECTS, create_objects);
	parcel->setParcelFlag(PF_CREATE_GROUP_OBJECTS, create_group_objects);
	parcel->setParcelFlag(PF_ALLOW_ALL_OBJECT_ENTRY, all_object_entry);
	parcel->setParcelFlag(PF_ALLOW_GROUP_OBJECT_ENTRY, group_object_entry);
	parcel->setParcelFlag(PF_ALLOW_TERRAFORM, allow_terraform);
	parcel->setParcelFlag(PF_ALLOW_DAMAGE, allow_damage);
	parcel->setParcelFlag(PF_ALLOW_FLY, allow_fly);
	parcel->setParcelFlag(PF_ALLOW_LANDMARK, allow_landmark);
	parcel->setParcelFlag(PF_ALLOW_GROUP_SCRIPTS, allow_group_scripts);
	parcel->setParcelFlag(PF_ALLOW_OTHER_SCRIPTS, allow_other_scripts);
	parcel->setParcelFlag(PF_SHOW_DIRECTORY, show_directory);
	parcel->setParcelFlag(PF_ALLOW_PUBLISH, allow_publish);
	parcel->setParcelFlag(PF_MATURE_PUBLISH, mature_publish);
	parcel->setParcelFlag(PF_RESTRICT_PUSHOBJECT, push_restriction);
	parcel->setCategory((LLParcel::ECategory)category_index);
	parcel->setLandingType((LLParcel::ELandingType)landing_type_index);
	parcel->setSnapshotID(snapshot_id);

	// Send current parcel data upstream to server
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

	// Might have changed properties, so let's redraw!
	self->refresh();
}


// static
void LLPanelLandOptions::onClickSet(void* userdata)
{
	LLPanelLandOptions* self = (LLPanelLandOptions*)userdata;

	LLParcel* selected_parcel = self->mParcel->getParcel();
	if (!selected_parcel) return;

	LLParcel* agent_parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!agent_parcel) return;

	if (agent_parcel->getLocalID() != selected_parcel->getLocalID())
	{
		gViewerWindow->alertXml("MustBeInParcel");
		return;
	}

	LLVector3 pos_region = gAgent.getPositionAgent();
	selected_parcel->setUserLocation(pos_region);
	selected_parcel->setUserLookAt(gAgent.getFrameAgent().getAtAxis());

	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(selected_parcel);

	self->refresh();
}

void LLPanelLandOptions::onClickClear(void* userdata)
{
	LLPanelLandOptions* self = (LLPanelLandOptions*)userdata;

	LLParcel* selected_parcel = self->mParcel->getParcel();
	if (!selected_parcel) return;

	// yes, this magic number of 0,0,0 means that it is clear
	LLVector3 zero_vec(0.f, 0.f, 0.f);
	selected_parcel->setUserLocation(zero_vec);
	selected_parcel->setUserLookAt(zero_vec);

	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate(selected_parcel);

	self->refresh();
}

// static
void LLPanelLandOptions::onClickPublishHelp(void*)
{
	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection()->getParcel();
	llassert(region); // Region should never be null.

	bool can_change_identity = region && parcel ? 
		LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_CHANGE_IDENTITY) &&
		! (region->getRegionFlags() & REGION_FLAGS_BLOCK_PARCEL_SEARCH) : false;

	if(! can_change_identity)
	{
		gViewerWindow->alertXml("ClickPublishHelpLandDisabled");
	}
	else
	{
		gViewerWindow->alertXml("ClickPublishHelpLand");
	}
}



//---------------------------------------------------------------------------
// LLPanelLandAccess
//---------------------------------------------------------------------------

LLPanelLandAccess::LLPanelLandAccess(LLParcelSelectionHandle& parcel)
:	LLPanel(std::string("land_access_panel")), mParcel(parcel)
{
}


BOOL LLPanelLandAccess::postBuild()
{
	childSetCommitCallback("public_access", onCommitPublicAccess, this);
	childSetCommitCallback("limit_payment", onCommitAny, this);
	childSetCommitCallback("limit_age_verified", onCommitAny, this);
	childSetCommitCallback("GroupCheck", onCommitAny, this);
	childSetCommitCallback("PassCheck", onCommitAny, this);
	childSetCommitCallback("pass_combo", onCommitAny, this);
	childSetCommitCallback("PriceSpin", onCommitAny, this);
	childSetCommitCallback("HoursSpin", onCommitAny, this);

	childSetAction("add_allowed", onClickAddAccess, this);
	childSetAction("remove_allowed", onClickRemoveAccess, this);
	childSetAction("add_banned", onClickAddBanned, this);
	childSetAction("remove_banned", onClickRemoveBanned, this);
	
	mListAccess = getChild<LLNameListCtrl>("AccessList");
	if (mListAccess)
		mListAccess->sortByColumnIndex(0, TRUE); // ascending

	mListBanned = getChild<LLNameListCtrl>("BannedList");
	if (mListBanned)
		mListBanned->sortByColumnIndex(0, TRUE); // ascending

	return TRUE;
}


LLPanelLandAccess::~LLPanelLandAccess()
{ 
}

void LLPanelLandAccess::refresh()
{
	if (mListAccess)
		mListAccess->deleteAllItems();
	if (mListBanned)
		mListBanned->deleteAllItems();
	
	LLParcel *parcel = mParcel->getParcel();
	
	// Display options
	if (parcel)
	{
		BOOL use_access_list = parcel->getParcelFlag(PF_USE_ACCESS_LIST);
		BOOL use_group = parcel->getParcelFlag(PF_USE_ACCESS_GROUP);
		BOOL public_access = !use_access_list && !use_group;
		
		childSetValue("public_access", public_access );
		childSetValue("GroupCheck", use_group );

		std::string group_name;
		gCacheName->getGroupName(parcel->getGroupID(), group_name);
		childSetLabelArg("GroupCheck", "[GROUP]", group_name );
		
		// Allow list
		{
			S32 count = parcel->mAccessList.size();
			childSetToolTipArg("AccessList", "[LISTED]", llformat("%d",count));
			childSetToolTipArg("AccessList", "[MAX]", llformat("%d",PARCEL_MAX_ACCESS_LIST));

			// *TODO: Translate
			for (access_map_const_iterator cit = parcel->mAccessList.begin();
				 cit != parcel->mAccessList.end(); ++cit)
			{
				const LLAccessEntry& entry = (*cit).second;
				std::string suffix;
				if (entry.mTime != 0)
				{
					S32 now = time(NULL);
					S32 seconds = entry.mTime - now;
					if (seconds < 0) seconds = 0;
					suffix.assign(" (");
					if (seconds >= 120)
					{
						std::string buf = llformat("%d minutes", (seconds/60));
						suffix.append(buf);
					}
					else if (seconds >= 60)
					{
						suffix.append("1 minute");
					}
					else
					{
						std::string buf = llformat("%d seconds", seconds);
						suffix.append(buf);
					}
					suffix.append(" remaining)");
				}
				if (mListAccess)
					mListAccess->addNameItem(entry.mID, ADD_SORTED, TRUE, suffix);
			}
		}
		
		// Ban List
		{
			S32 count = parcel->mBanList.size();

			childSetToolTipArg("BannedList", "[LISTED]", llformat("%d",count));
			childSetToolTipArg("BannedList", "[MAX]", llformat("%d",PARCEL_MAX_ACCESS_LIST));

			for (access_map_const_iterator cit = parcel->mBanList.begin();
				 cit != parcel->mBanList.end(); ++cit)
			{
				const LLAccessEntry& entry = (*cit).second;
				std::string suffix;
				if (entry.mTime != 0)
				{
					S32 now = time(NULL);
					S32 seconds = entry.mTime - now;
					if (seconds < 0) seconds = 0;
					suffix.assign(" (");
					if (seconds >= 120)
					{
						std::string buf = llformat("%d minutes", (seconds/60));
						suffix.append(buf);
					}
					else if (seconds >= 60)
					{
						suffix.append("1 minute");
					}
					else
					{
						std::string buf = llformat("%d seconds", seconds);
						suffix.append(buf);
					}
					suffix.append(" remaining)");
				}
				mListBanned->addNameItem(entry.mID, ADD_SORTED, TRUE, suffix);
			}
		}

		if(parcel->getRegionDenyAnonymousOverride())
		{
			childSetValue("limit_payment", TRUE);
		}
		else
		{
			childSetValue("limit_payment", (parcel->getParcelFlag(PF_DENY_ANONYMOUS)));
		}
		if(parcel->getRegionDenyAgeUnverifiedOverride())
		{
			childSetValue("limit_age_verified", TRUE);
		}
		else
		{
			childSetValue("limit_age_verified", (parcel->getParcelFlag(PF_DENY_AGEUNVERIFIED)));
		}
		
		BOOL use_pass = parcel->getParcelFlag(PF_USE_PASS_LIST);
		childSetValue("PassCheck",  use_pass );
		LLCtrlSelectionInterface* passcombo = childGetSelectionInterface("pass_combo");
		if (passcombo)
		{
			if (public_access || !use_pass || !use_group)
			{
				passcombo->selectByValue("anyone");
			}
		}
		
		S32 pass_price = parcel->getPassPrice();
		childSetValue( "PriceSpin", (F32)pass_price );

		F32 pass_hours = parcel->getPassHours();
		childSetValue( "HoursSpin", pass_hours );
	}
	else
	{
		childSetValue("public_access", FALSE);
		childSetValue("limit_payment", FALSE);
		childSetValue("limit_age_verified", FALSE);
		childSetValue("GroupCheck", FALSE);
		childSetLabelArg("GroupCheck", "[GROUP]", LLStringUtil::null );
		childSetValue("PassCheck", FALSE);
		childSetValue("PriceSpin", (F32)PARCEL_PASS_PRICE_DEFAULT);
		childSetValue( "HoursSpin", PARCEL_PASS_HOURS_DEFAULT );
		childSetToolTipArg("AccessList", "[LISTED]", llformat("%d",0));
		childSetToolTipArg("AccessList", "[MAX]", llformat("%d",0));
		childSetToolTipArg("BannedList", "[LISTED]", llformat("%d",0));
		childSetToolTipArg("BannedList", "[MAX]", llformat("%d",0));
	}	
}

void LLPanelLandAccess::refresh_ui()
{
	childSetEnabled("public_access", FALSE);
	childSetEnabled("limit_payment", FALSE);
	childSetEnabled("limit_age_verified", FALSE);
	childSetEnabled("GroupCheck", FALSE);
	childSetEnabled("PassCheck", FALSE);
	childSetEnabled("pass_combo", FALSE);
	childSetEnabled("PriceSpin", FALSE);
	childSetEnabled("HoursSpin", FALSE);
	childSetEnabled("AccessList", FALSE);
	childSetEnabled("BannedList", FALSE);
	
	LLParcel *parcel = mParcel->getParcel();
	if (parcel)
	{
		BOOL can_manage_allowed = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_MANAGE_ALLOWED);
		BOOL can_manage_banned = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_MANAGE_BANNED);
	
		childSetEnabled("public_access", can_manage_allowed);
		BOOL public_access = childGetValue("public_access").asBoolean();
		if (public_access)
		{
			bool override = false;
			if(parcel->getRegionDenyAnonymousOverride())
			{
				override = true;
				childSetEnabled("limit_payment", FALSE);
			}
			else
			{
				childSetEnabled("limit_payment", can_manage_allowed);
			}
			if(parcel->getRegionDenyAgeUnverifiedOverride())
			{
				override = true;
				childSetEnabled("limit_age_verified", FALSE);
			}
			else
			{
				childSetEnabled("limit_age_verified", can_manage_allowed);
			}
			if (override)
			{
				childSetToolTip("Only Allow", getString("estate_override"));
			}
			else
			{
				childSetToolTip("Only Allow", std::string());
			}
			childSetEnabled("GroupCheck", FALSE);
			childSetEnabled("PassCheck", FALSE);
			childSetEnabled("pass_combo", FALSE);
			childSetEnabled("AccessList", FALSE);
		}
		else
		{
			childSetEnabled("limit_payment", FALSE);
			childSetEnabled("limit_age_verified", FALSE);

			std::string group_name;
			if (gCacheName->getGroupName(parcel->getGroupID(), group_name))
			{			
				childSetEnabled("GroupCheck", can_manage_allowed);
			}
			BOOL group_access = childGetValue("GroupCheck").asBoolean();
			BOOL sell_passes = childGetValue("PassCheck").asBoolean();
			childSetEnabled("PassCheck", can_manage_allowed);
			if (sell_passes)
			{
				childSetEnabled("pass_combo", group_access && can_manage_allowed);
				childSetEnabled("PriceSpin", can_manage_allowed);
				childSetEnabled("HoursSpin", can_manage_allowed);
			}
		}
		childSetEnabled("AccessList", can_manage_allowed);
		S32 allowed_list_count = parcel->mAccessList.size();
		childSetEnabled("add_allowed", can_manage_allowed && allowed_list_count < PARCEL_MAX_ACCESS_LIST);
		BOOL has_selected = mListAccess->getSelectionInterface()->getFirstSelectedIndex() >= 0;
		childSetEnabled("remove_allowed", can_manage_allowed && has_selected);
		
		childSetEnabled("BannedList", can_manage_banned);
		S32 banned_list_count = parcel->mBanList.size();
		childSetEnabled("add_banned", can_manage_banned && banned_list_count < PARCEL_MAX_ACCESS_LIST);
		has_selected = mListBanned->getSelectionInterface()->getFirstSelectedIndex() >= 0;
		childSetEnabled("remove_banned", can_manage_banned && has_selected);
	}
}
		

// public
void LLPanelLandAccess::refreshNames()
{
	LLParcel* parcel = mParcel->getParcel();
	std::string group_name;
	if(parcel)
	{
		gCacheName->getGroupName(parcel->getGroupID(), group_name);
	}
	childSetLabelArg("GroupCheck", "[GROUP]", group_name);
}


// virtual
void LLPanelLandAccess::draw()
{
	refresh_ui();
	refreshNames();
	LLPanel::draw();
}

// static
void LLPanelLandAccess::onCommitPublicAccess(LLUICtrl *ctrl, void *userdata)
{
	LLPanelLandAccess *self = (LLPanelLandAccess *)userdata;
	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// If we disabled public access, enable group access by default (if applicable)
	BOOL public_access = self->childGetValue("public_access").asBoolean();
	if (public_access == FALSE)
	{
		std::string group_name;
		if (gCacheName->getGroupName(parcel->getGroupID(), group_name))
		{
			self->childSetValue("GroupCheck", public_access ? FALSE : TRUE);
		}
	}
	
	onCommitAny(ctrl, userdata);
}

// static
void LLPanelLandAccess::onCommitAny(LLUICtrl *ctrl, void *userdata)
{
	LLPanelLandAccess *self = (LLPanelLandAccess *)userdata;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	BOOL public_access = self->childGetValue("public_access").asBoolean();
	BOOL use_access_group = self->childGetValue("GroupCheck").asBoolean();
	if (use_access_group)
	{
		std::string group_name;
		if (!gCacheName->getGroupName(parcel->getGroupID(), group_name))
		{
			use_access_group = FALSE;
		}
	}
		
	BOOL limit_payment = FALSE, limit_age_verified = FALSE;
	BOOL use_access_list = FALSE;
	BOOL use_pass_list = FALSE;
	
	if (public_access)
	{
		use_access_list = FALSE;
		use_access_group = FALSE;
		limit_payment = self->childGetValue("limit_payment").asBoolean();
		limit_age_verified = self->childGetValue("limit_age_verified").asBoolean();
	}
	else
	{
		use_access_list = TRUE;
		use_pass_list = self->childGetValue("PassCheck").asBoolean();
		if (use_access_group && use_pass_list)
		{
			LLCtrlSelectionInterface* passcombo = self->childGetSelectionInterface("pass_combo");
			if (passcombo)
			{
				if (passcombo->getSelectedValue().asString() == "group")
				{
					use_access_list = FALSE;
				}
			}
		}
	}

	S32 pass_price = llfloor((F32)self->childGetValue("PriceSpin").asReal());
	F32 pass_hours = (F32)self->childGetValue("HoursSpin").asReal();

	// Push data into current parcel
	parcel->setParcelFlag(PF_USE_ACCESS_GROUP,	use_access_group);
	parcel->setParcelFlag(PF_USE_ACCESS_LIST,	use_access_list);
	parcel->setParcelFlag(PF_USE_PASS_LIST,		use_pass_list);
	parcel->setParcelFlag(PF_USE_BAN_LIST,		TRUE);
	parcel->setParcelFlag(PF_DENY_ANONYMOUS, 	limit_payment);
	parcel->setParcelFlag(PF_DENY_AGEUNVERIFIED, limit_age_verified);

	parcel->setPassPrice( pass_price );
	parcel->setPassHours( pass_hours );

	// Send current parcel data upstream to server
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

	// Might have changed properties, so let's redraw!
	self->refresh();
}

// static
void LLPanelLandAccess::onClickAddAccess(void* data)
{
	LLPanelLandAccess* panelp = (LLPanelLandAccess*)data;
	if (panelp)
	{
		gFloaterView->getParentFloater(panelp)->addDependentFloater(LLFloaterAvatarPicker::show(callbackAvatarCBAccess, data) );
	}
}

// static
void LLPanelLandAccess::callbackAvatarCBAccess(const std::vector<std::string>& names, const std::vector<LLUUID>& ids, void* userdata)
{
	LLPanelLandAccess* panelp = (LLPanelLandAccess*)userdata;
	if (!names.empty() && !ids.empty())
	{
		LLUUID id = ids[0];
		LLParcel* parcel = panelp->mParcel->getParcel();
		if (parcel)
		{
			parcel->addToAccessList(id, 0);
			LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(AL_ACCESS);
			panelp->refresh();
		}
	}
}

// static
void LLPanelLandAccess::onClickRemoveAccess(void* data)
{
	LLPanelLandAccess* panelp = (LLPanelLandAccess*)data;
	if (panelp && panelp->mListAccess)
	{
		LLParcel* parcel = panelp->mParcel->getParcel();
		if (parcel)
		{
			std::vector<LLScrollListItem*> names = panelp->mListAccess->getAllSelected();
			for (std::vector<LLScrollListItem*>::iterator iter = names.begin();
				 iter != names.end(); )
			{
				LLScrollListItem* item = *iter++;
				const LLUUID& agent_id = item->getUUID();
				parcel->removeFromAccessList(agent_id);
			}
			LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(AL_ACCESS);
			panelp->refresh();
		}
	}
}

// static
void LLPanelLandAccess::onClickAddBanned(void* data)
{
	LLPanelLandAccess* panelp = (LLPanelLandAccess*)data;
	gFloaterView->getParentFloater(panelp)->addDependentFloater(LLFloaterAvatarPicker::show(callbackAvatarCBBanned, data) );
}

// static
void LLPanelLandAccess::callbackAvatarCBBanned(const std::vector<std::string>& names, const std::vector<LLUUID>& ids, void* userdata)
{
	LLPanelLandAccess* panelp = (LLPanelLandAccess*)userdata;
	if (!names.empty() && !ids.empty())
	{
		LLUUID id = ids[0];
		LLParcel* parcel = panelp->mParcel->getParcel();
		if (parcel)
		{
			parcel->addToBanList(id, 0);
			LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(AL_BAN);
			panelp->refresh();
		}
	}
}

// static
void LLPanelLandAccess::onClickRemoveBanned(void* data)
{
	LLPanelLandAccess* panelp = (LLPanelLandAccess*)data;
	if (panelp && panelp->mListBanned)
	{
		LLParcel* parcel = panelp->mParcel->getParcel();
		if (parcel)
		{
			std::vector<LLScrollListItem*> names = panelp->mListBanned->getAllSelected();
			for (std::vector<LLScrollListItem*>::iterator iter = names.begin();
				 iter != names.end(); )
			{
				LLScrollListItem* item = *iter++;
				const LLUUID& agent_id = item->getUUID();
				parcel->removeFromBanList(agent_id);
			}
			LLViewerParcelMgr::getInstance()->sendParcelAccessListUpdate(AL_BAN);
			panelp->refresh();
		}
	}
}

//---------------------------------------------------------------------------
// LLPanelLandCovenant
//---------------------------------------------------------------------------
LLPanelLandCovenant::LLPanelLandCovenant(LLParcelSelectionHandle& parcel)
:	LLPanel(std::string("land_covenant_panel")), mParcel(parcel)
{	
}

LLPanelLandCovenant::~LLPanelLandCovenant()
{
}

BOOL LLPanelLandCovenant::postBuild()
{
	refresh();
	return TRUE;
}

// virtual
void LLPanelLandCovenant::refresh()
{
	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if(!region) return;
		
	LLTextBox* region_name = getChild<LLTextBox>("region_name_text");
	if (region_name)
	{
		region_name->setText(region->getName());
	}

	
	LLTextBox* region_maturity = getChild<LLTextBox>("region_maturity_text");
	if (region_maturity)
	{
		region_maturity->setText(region->getSimAccessString());
	}
	
	LLTextBox* resellable_clause = getChild<LLTextBox>("resellable_clause");
	if (resellable_clause)
	{
		if (region->getRegionFlags() & REGION_FLAGS_BLOCK_LAND_RESELL)
		{
			resellable_clause->setText(getString("can_not_resell"));
		}
		else
		{
			resellable_clause->setText(getString("can_resell"));
		}
	}
	
	LLTextBox* changeable_clause = getChild<LLTextBox>("changeable_clause");
	if (changeable_clause)
	{
		if (region->getRegionFlags() & REGION_FLAGS_ALLOW_PARCEL_CHANGES)
		{
			changeable_clause->setText(getString("can_change"));
		}
		else
		{
			changeable_clause->setText(getString("can_not_change"));
		}
	}
	
	// send EstateCovenantInfo message
	LLMessageSystem *msg = gMessageSystem;
	msg->newMessage("EstateCovenantRequest");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID,	gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
	msg->sendReliable(region->getHost());
}

// static
void LLPanelLandCovenant::updateCovenantText(const std::string &string)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		LLViewerTextEditor* editor = self->getChild<LLViewerTextEditor>("covenant_editor");
		if (editor)
		{
			editor->setHandleEditKeysDirectly(TRUE);
			editor->setText(string);
		}
	}
}

// static
void LLPanelLandCovenant::updateEstateName(const std::string& name)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		LLTextBox* editor = self->getChild<LLTextBox>("estate_name_text");
		if (editor) editor->setText(name);
	}
}

// static
void LLPanelLandCovenant::updateLastModified(const std::string& text)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		LLTextBox* editor = self->getChild<LLTextBox>("covenant_timestamp_text");
		if (editor) editor->setText(text);
	}
}

// static
void LLPanelLandCovenant::updateEstateOwnerName(const std::string& name)
{
	LLPanelLandCovenant* self = LLFloaterLand::getCurrentPanelLandCovenant();
	if (self)
	{
		LLTextBox* editor = self->getChild<LLTextBox>("estate_owner_text");
		if (editor) editor->setText(name);
	}
}
