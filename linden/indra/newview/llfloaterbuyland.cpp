/** 
 * @file llfloaterbuyland.cpp
 * @brief LLFloaterBuyLand class implementation
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfloaterbuyland.h"

// viewer includes
#include "llxmlrpctransaction.h"
#include "llagent.h"
#include "llbutton.h"
#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llconfirmationmanager.h"
#include "llcurrencyuimanager.h"
#include "llfloater.h"
#include "llfloatertools.h"
#include "llframetimer.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llnotify.h"
#include "llparcel.h"
#include "llstatusbar.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "llviewchildren.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
#include "llweb.h"
#include "llwindow.h"
#include "llworld.h"
#include "llviewernetwork.h"
#include "roles_constants.h"

// NOTE: This is duplicated in lldatamoney.cpp ...
const F32 GROUP_LAND_BONUS_FACTOR = 1.1f;
const F64 CURRENCY_ESTIMATE_FREQUENCY = 0.5;
	// how long of a pause in typing a currency buy amount before an
	// esimate is fetched from the server

class LLFloaterBuyLandUI
:	public LLFloater
{
private:
	LLFloaterBuyLandUI();
	virtual ~LLFloaterBuyLandUI();

	LLViewerRegion*	mRegion;
	LLParcelSelectionHandle mParcel;
	bool			mIsClaim;
	bool			mIsForGroup;

	bool			mCanBuy;
	bool			mCannotBuyIsError;
	std::string		mCannotBuyReason;
	std::string		mCannotBuyURI;
	
	bool			mBought;
	
	// information about the agent
	S32				mAgentCommittedTier;
	S32				mAgentCashBalance;
	bool			mAgentHasNeverOwnedLand;
	
	// information about the parcel
	bool			mParcelValid;
	bool			mParcelIsForSale;
	bool			mParcelIsGroupLand;
	S32				mParcelGroupContribution;
	S32				mParcelPrice;
	S32				mParcelActualArea;
	S32				mParcelBillableArea;
	S32				mParcelSupportedObjects;
	bool			mParcelSoldWithObjects;
	std::string		mParcelLocation;
	LLUUID			mParcelSnapshot;
	std::string		mParcelSellerName;
	
	// user's choices
	S32				mUserPlanChoice;
	
	// from website
	bool			mSiteValid;
	bool			mSiteMembershipUpgrade;
	std::string		mSiteMembershipAction;
	std::vector<std::string>
					mSiteMembershipPlanIDs;
	std::vector<std::string>
					mSiteMembershipPlanNames;
	bool			mSiteLandUseUpgrade;
	std::string		mSiteLandUseAction;
	std::string		mSiteConfirm;
	
	// values in current Preflight transaction... used to avoid extra
	// preflights when the parcel manager goes update crazy
	S32				mPreflightAskBillableArea;
	S32				mPreflightAskCurrencyBuy;

	LLViewChildren		mChildren;
	LLCurrencyUIManager	mCurrency;
	
	enum TransactionType
	{
		TransactionPreflight,
		TransactionCurrency,
		TransactionBuy
	};
	LLXMLRPCTransaction* mTransaction;
	TransactionType		 mTransactionType;
	
	LLViewerParcelMgr::ParcelBuyInfo*	mParcelBuyInfo;
	
	static LLFloaterBuyLandUI* sInstance;
	
public:
	static LLFloaterBuyLandUI* soleInstance(bool createIfNeeded);

	void setForGroup(bool is_for_group);
	void setParcel(LLViewerRegion* region, LLParcelSelectionHandle parcel);
		
	void updateAgentInfo();
	void updateParcelInfo();
	void updateCovenantInfo();
	static void onChangeAgreeCovenant(LLUICtrl* ctrl, void* user_data);
	void updateCovenantText(const std::string& string, const LLUUID &asset_id);
	void updateEstateName(const std::string& name);
	void updateLastModified(const std::string& text);
	void updateEstateOwnerName(const std::string& name);
	void updateWebSiteInfo();
	void finishWebSiteInfo();
	
	void runWebSitePrep(const std::string& password);
	void finishWebSitePrep();
	void sendBuyLand();

	void updateNames();
	
	void refreshUI();
	
	void startTransaction(TransactionType type, LLXMLRPCValue params);
	bool checkTransaction();
	
	void tellUserError(const std::string& message, const std::string& uri);

	virtual BOOL postBuild();
	
	void startBuyPreConfirm();
	void startBuyPostConfirm(const std::string& password);

	static void onClickBuy(void* data);
	static void onClickCancel(void* data);
	static void onClickErrorWeb(void* data);
	
	virtual void draw();
	virtual BOOL canClose();
	virtual void onClose(bool app_quitting);
	/*virtual*/ void setMinimized(BOOL b);
	
private:
	class SelectionObserver : public LLParcelObserver
	{
	public:
		virtual void changed();
	};
};

static void cacheNameUpdateRefreshesBuyLand(const LLUUID&,
	const std::string&, const std::string&, BOOL, void* data)
{
	LLFloaterBuyLandUI* ui = LLFloaterBuyLandUI::soleInstance(false);
	if (ui)
	{
		ui->updateNames();
	}
}

// static
void LLFloaterBuyLand::buyLand(
	LLViewerRegion* region, LLParcelSelectionHandle parcel, bool is_for_group)
{
	if(is_for_group && !gAgent.hasPowerInActiveGroup(GP_LAND_DEED))
	{
		gViewerWindow->alertXml("OnlyOfficerCanBuyLand");
		return;
	}

	LLFloaterBuyLandUI* ui = LLFloaterBuyLandUI::soleInstance(true);
	ui->setForGroup(is_for_group);
	ui->setParcel(region, parcel);
	ui->open();	/*Flawfinder: ignore*/
}

// static
void LLFloaterBuyLand::updateCovenantText(const std::string& string, const LLUUID &asset_id)
{
	LLFloaterBuyLandUI* floater = LLFloaterBuyLandUI::soleInstance(FALSE);
	if (floater)
	{
		floater->updateCovenantText(string, asset_id);
	}
}

// static
void LLFloaterBuyLand::updateEstateName(const std::string& name)
{
	LLFloaterBuyLandUI* floater = LLFloaterBuyLandUI::soleInstance(FALSE);
	if (floater)
	{
		floater->updateEstateName(name);
	}
}

// static
void LLFloaterBuyLand::updateLastModified(const std::string& text)
{
	LLFloaterBuyLandUI* floater = LLFloaterBuyLandUI::soleInstance(FALSE);
	if (floater)
	{
		floater->updateLastModified(text);
	}
}

// static
void LLFloaterBuyLand::updateEstateOwnerName(const std::string& name)
{
	LLFloaterBuyLandUI* floater = LLFloaterBuyLandUI::soleInstance(FALSE);
	if (floater)
	{
		floater->updateEstateOwnerName(name);
	}
}

// static
BOOL LLFloaterBuyLand::isOpen()
{
	LLFloaterBuyLandUI* floater = LLFloaterBuyLandUI::soleInstance(FALSE);
	if (floater)
	{
		return floater->getVisible();
	}
	return FALSE;
}

// static
LLFloaterBuyLandUI* LLFloaterBuyLandUI::sInstance = NULL;

// static
LLFloaterBuyLandUI* LLFloaterBuyLandUI::soleInstance(bool createIfNeeded)
{
#if !LL_RELEASE_FOR_DOWNLOAD
	if (createIfNeeded)
	{
		delete sInstance;
		sInstance = NULL;
	}
#endif
	if (!sInstance  &&  createIfNeeded)
	{
		sInstance = new LLFloaterBuyLandUI();

		LLUICtrlFactory::getInstance()->buildFloater(sInstance, "floater_buy_land.xml");
		sInstance->center();

		static bool observingCacheName = false;
		if (!observingCacheName)
		{
			gCacheName->addObserver(cacheNameUpdateRefreshesBuyLand);
			observingCacheName = true;
		}

		static SelectionObserver* parcelSelectionObserver = NULL;
		if (!parcelSelectionObserver)
		{
			parcelSelectionObserver = new SelectionObserver;
			LLViewerParcelMgr::getInstance()->addObserver(parcelSelectionObserver);
		}
	}
	
	return sInstance;
}


#if LL_WINDOWS
// passing 'this' during construction generates a warning. The callee
// only uses the pointer to hold a reference to 'this' which is
// already valid, so this call does the correct thing. Disable the
// warning so that we can compile without generating a warning.
#pragma warning(disable : 4355)
#endif 
LLFloaterBuyLandUI::LLFloaterBuyLandUI()
:	LLFloater(std::string("Buy Land")),
	mParcel(0),
	mBought(false),
	mParcelValid(false), mSiteValid(false),
	mChildren(*this), mCurrency(*this), mTransaction(0),
	mParcelBuyInfo(0)
{
}

LLFloaterBuyLandUI::~LLFloaterBuyLandUI()
{
	delete mTransaction;

	LLViewerParcelMgr::getInstance()->deleteParcelBuy(mParcelBuyInfo);
	
	if (sInstance == this)
	{
		sInstance = NULL;
	}
}

void LLFloaterBuyLandUI::SelectionObserver::changed()
{
	LLFloaterBuyLandUI* ui = LLFloaterBuyLandUI::soleInstance(false);
	if (ui)
	{
		if (LLViewerParcelMgr::getInstance()->selectionEmpty())
		{
			ui->close();
		}
		else {
			ui->setParcel(
				LLViewerParcelMgr::getInstance()->getSelectionRegion(),
				LLViewerParcelMgr::getInstance()->getParcelSelection());
		}
	}
}


void LLFloaterBuyLandUI::updateAgentInfo()
{
	mAgentCommittedTier = gStatusBar->getSquareMetersCommitted();
	mAgentCashBalance = gStatusBar->getBalance();

	// *TODO: This is an approximation, we should send this value down
	// to the viewer. See SL-10728 for details.
	mAgentHasNeverOwnedLand = mAgentCommittedTier == 0;
}

void LLFloaterBuyLandUI::updateParcelInfo()
{
	LLParcel* parcel = mParcel->getParcel();
	mParcelValid = parcel && mRegion;
	mParcelIsForSale = false;
	mParcelIsGroupLand = false;
	mParcelGroupContribution = 0;
	mParcelPrice = 0;
	mParcelActualArea = 0;
	mParcelBillableArea = 0;
	mParcelSupportedObjects = 0;
	mParcelSoldWithObjects = false;
	mParcelLocation = "";
	mParcelSnapshot.setNull();
	mParcelSellerName = "";
	
	mCanBuy = false;
	mCannotBuyIsError = false;
	
	if (!mParcelValid)
	{
		mCannotBuyReason = getString("no_land_selected");
		return;
	}
	
	if (mParcel->getMultipleOwners())
	{
		mCannotBuyReason = getString("multiple_parcels_selected");
		return;
	}

	const LLUUID& parcelOwner = parcel->getOwnerID();
	
	mIsClaim = parcel->isPublic();
	if (!mIsClaim)
	{
		mParcelActualArea = parcel->getArea();
		mParcelIsForSale = parcel->getForSale();
		mParcelIsGroupLand = parcel->getIsGroupOwned();
		mParcelPrice = mParcelIsForSale ? parcel->getSalePrice() : 0;
		
		if (mParcelIsGroupLand)
		{
			LLUUID group_id = parcel->getGroupID();
			mParcelGroupContribution = gAgent.getGroupContribution(group_id);
		}
	}
	else
	{
		mParcelActualArea = mParcel->getClaimableArea();
		mParcelIsForSale = true;
		mParcelPrice = mParcelActualArea * parcel->getClaimPricePerMeter();
	}

	mParcelBillableArea =
		llround(mRegion->getBillableFactor() * mParcelActualArea);

 	mParcelSupportedObjects = llround(
		parcel->getMaxPrimCapacity() * parcel->getParcelPrimBonus()); 
 	// Can't have more than region max tasks, regardless of parcel 
 	// object bonus factor. 
 	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion(); 
 	if(region) 
 	{ 
		S32 max_tasks_per_region = (S32)region->getMaxTasks(); 
		mParcelSupportedObjects = llmin(
			mParcelSupportedObjects, max_tasks_per_region); 
 	} 

	mParcelSoldWithObjects = parcel->getSellWithObjects();

	
	LLVector3 center = parcel->getCenterpoint();
	mParcelLocation = llformat("%s %d,%d",
				mRegion->getName().c_str(),
				(int)center[VX], (int)center[VY]
				);
	
	mParcelSnapshot = parcel->getSnapshotID();
	
	updateNames();
	
	bool haveEnoughCash = mParcelPrice <= mAgentCashBalance;
	S32 cashBuy = haveEnoughCash ? 0 : (mParcelPrice - mAgentCashBalance);
	mCurrency.setAmount(cashBuy, true);
	mCurrency.setZeroMessage(haveEnoughCash ? getString("none_needed") : LLStringUtil::null);

	// checks that we can buy the land

	if(mIsForGroup && !gAgent.hasPowerInActiveGroup(GP_LAND_DEED))
	{
		mCannotBuyReason = getString("cant_buy_for_group");
		return;
	}

	if (!mIsClaim)
	{
		const LLUUID& authorizedBuyer = parcel->getAuthorizedBuyerID();
		const LLUUID buyer = gAgent.getID();
		const LLUUID newOwner = mIsForGroup ? gAgent.getGroupID() : buyer;

		if (!mParcelIsForSale
			|| (mParcelPrice == 0  &&  authorizedBuyer.isNull()))
		{
			
			mCannotBuyReason = getString("parcel_not_for_sale");
			return;
		}

		if (parcelOwner == newOwner)
		{
			if (mIsForGroup)
			{
				mCannotBuyReason = getString("group_already_owns");
			}
			else
			{
				mCannotBuyReason = getString("you_already_own");
			}
			return;
		}

		if (!authorizedBuyer.isNull()  &&  buyer != authorizedBuyer)
		{
			mCannotBuyReason = getString("set_to_sell_to_other");
			return;
		}
	}
	else
	{
		if (mParcelActualArea == 0)
		{
			mCannotBuyReason = getString("no_public_land");
			return;
		}

		if (mParcel->hasOthersSelected())
		{
			// Policy: Must not have someone else's land selected
			mCannotBuyReason = getString("not_owned_by_you");
			return;
		}
	}

	mCanBuy = true;
}

void LLFloaterBuyLandUI::updateCovenantInfo()
{
	LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
	if(!region) return;

	LLTextBox* region_name = getChild<LLTextBox>("region_name_text");
	if (region_name)
	{
		region_name->setText(region->getName());
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

	LLCheckBoxCtrl* check = getChild<LLCheckBoxCtrl>("agree_covenant");
	if(check)
	{
		check->set(false);
		check->setEnabled(true);
		check->setCallbackUserData(this);
		check->setCommitCallback(onChangeAgreeCovenant);
	}

	LLTextBox* box = getChild<LLTextBox>("covenant_text");
	if(box)
	{
		box->setVisible(FALSE);
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
void LLFloaterBuyLandUI::onChangeAgreeCovenant(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterBuyLandUI* self = (LLFloaterBuyLandUI*)user_data;
	if(self)
	{
		self->refreshUI();
	}
}

void LLFloaterBuyLandUI::updateCovenantText(const std::string &string, const LLUUID& asset_id)
{
	LLViewerTextEditor* editor = getChild<LLViewerTextEditor>("covenant_editor");
	if (editor)
	{
		editor->setHandleEditKeysDirectly(FALSE);
		editor->setText(string);

		LLCheckBoxCtrl* check = getChild<LLCheckBoxCtrl>("agree_covenant");
		LLTextBox* box = getChild<LLTextBox>("covenant_text");
		if(check && box)
		{
			if (asset_id.isNull())
			{
				check->set(true);
				check->setEnabled(false);
				refreshUI();

				// remove the line stating that you must agree
				box->setVisible(FALSE);
			}
			else
			{
				check->setEnabled(true);

				// remove the line stating that you must agree
				box->setVisible(TRUE);
			}
		}
	}
}

void LLFloaterBuyLandUI::updateEstateName(const std::string& name)
{
	LLTextBox* box = getChild<LLTextBox>("estate_name_text");
	if (box) box->setText(name);
}

void LLFloaterBuyLandUI::updateLastModified(const std::string& text)
{
	LLTextBox* editor = getChild<LLTextBox>("covenant_timestamp_text");
	if (editor) editor->setText(text);
}

void LLFloaterBuyLandUI::updateEstateOwnerName(const std::string& name)
{
	LLTextBox* box = getChild<LLTextBox>("estate_owner_text");
	if (box) box->setText(name);
}

void LLFloaterBuyLandUI::updateWebSiteInfo()
{
	S32 askBillableArea = mIsForGroup ? 0 : mParcelBillableArea;
	S32 askCurrencyBuy = mCurrency.getAmount();
	
	if (mTransaction && mTransactionType == TransactionPreflight
	&&  mPreflightAskBillableArea == askBillableArea
	&&  mPreflightAskCurrencyBuy == askCurrencyBuy)
	{
		return;
	}
	
	mPreflightAskBillableArea = askBillableArea;
	mPreflightAskCurrencyBuy = askCurrencyBuy;
	
#if 0
	// enable this code if you want the details to blank while we're talking
	// to the web site... it's kind of jarring
	mSiteValid = false;
	mSiteMembershipUpgrade = false;
	mSiteMembershipAction = "(waiting)";
	mSiteMembershipPlanIDs.clear();
	mSiteMembershipPlanNames.clear();
	mSiteLandUseUpgrade = false;
	mSiteLandUseAction = "(waiting)";
	mSiteCurrencyEstimated = false;
	mSiteCurrencyEstimatedCost = 0;
#endif
	
	LLXMLRPCValue keywordArgs = LLXMLRPCValue::createStruct();
	keywordArgs.appendString("agentId", gAgent.getID().asString());
	keywordArgs.appendString(
		"secureSessionId",
		gAgent.getSecureSessionID().asString());
	keywordArgs.appendInt("billableArea", mPreflightAskBillableArea);
	keywordArgs.appendInt("currencyBuy", mPreflightAskCurrencyBuy);
	
	LLXMLRPCValue params = LLXMLRPCValue::createArray();
	params.append(keywordArgs);
	
	startTransaction(TransactionPreflight, params);
}

void LLFloaterBuyLandUI::finishWebSiteInfo()
{
	
	LLXMLRPCValue result = mTransaction->responseValue();

	mSiteValid = result["success"].asBool();
	if (!mSiteValid)
	{
		tellUserError(
			result["errorMessage"].asString(),
			result["errorURI"].asString()
		);
		return;
	}
	
	LLXMLRPCValue membership = result["membership"];
	mSiteMembershipUpgrade = membership["upgrade"].asBool();
	mSiteMembershipAction = membership["action"].asString();
	mSiteMembershipPlanIDs.clear();
	mSiteMembershipPlanNames.clear();
	LLXMLRPCValue levels = membership["levels"];
	for (LLXMLRPCValue level = levels.rewind();
		level.isValid();
		level = levels.next())
	{
		mSiteMembershipPlanIDs.push_back(level["id"].asString());
		mSiteMembershipPlanNames.push_back(level["description"].asString());
	}
	mUserPlanChoice = 0;

	LLXMLRPCValue landUse = result["landUse"];
	mSiteLandUseUpgrade = landUse["upgrade"].asBool();
	mSiteLandUseAction = landUse["action"].asString();

	LLXMLRPCValue currency = result["currency"];
	mCurrency.setEstimate(currency["estimatedCost"].asInt());

	mSiteConfirm = result["confirm"].asString();
}

void LLFloaterBuyLandUI::runWebSitePrep(const std::string& password)
{
	if (!mCanBuy)
	{
		return;
	}
	
	BOOL remove_contribution = childGetValue("remove_contribution").asBoolean();
	mParcelBuyInfo = LLViewerParcelMgr::getInstance()->setupParcelBuy(gAgent.getID(), gAgent.getSessionID(),
						gAgent.getGroupID(), mIsForGroup, mIsClaim, remove_contribution);

	if (mParcelBuyInfo
		&& !mSiteMembershipUpgrade
		&& !mSiteLandUseUpgrade
		&& mCurrency.getAmount() == 0
		&& mSiteConfirm != "password")
	{
		sendBuyLand();
		return;
	}


	std::string newLevel = "noChange";
	
	if (mSiteMembershipUpgrade)
	{
		LLComboBox* levels = getChild<LLComboBox>( "account_level");
		if (levels)
		{
			mUserPlanChoice = levels->getCurrentIndex();
			newLevel = mSiteMembershipPlanIDs[mUserPlanChoice];
		}
	}
	
	LLXMLRPCValue keywordArgs = LLXMLRPCValue::createStruct();
	keywordArgs.appendString("agentId", gAgent.getID().asString());
	keywordArgs.appendString(
		"secureSessionId",
		gAgent.getSecureSessionID().asString());
	keywordArgs.appendString("levelId", newLevel);
	keywordArgs.appendInt("billableArea",
		mIsForGroup ? 0 : mParcelBillableArea);
	keywordArgs.appendInt("currencyBuy", mCurrency.getAmount());
	keywordArgs.appendInt("estimatedCost", mCurrency.getEstimate());
	keywordArgs.appendString("confirm", mSiteConfirm);
	if (!password.empty())
	{
		keywordArgs.appendString("password", password);
	}
	
	LLXMLRPCValue params = LLXMLRPCValue::createArray();
	params.append(keywordArgs);
	
	startTransaction(TransactionBuy, params);
}

void LLFloaterBuyLandUI::finishWebSitePrep()
{
	LLXMLRPCValue result = mTransaction->responseValue();

	bool success = result["success"].asBool();
	if (!success)
	{
		tellUserError(
			result["errorMessage"].asString(),
			result["errorURI"].asString()
		);
		return;
	}
	
	sendBuyLand();
}

void LLFloaterBuyLandUI::sendBuyLand()
{
	if (mParcelBuyInfo)
	{
		LLViewerParcelMgr::getInstance()->sendParcelBuy(mParcelBuyInfo);
		LLViewerParcelMgr::getInstance()->deleteParcelBuy(mParcelBuyInfo);
		mBought = true;
	}
}

void LLFloaterBuyLandUI::updateNames()
{
	LLParcel* parcelp = mParcel->getParcel();

	if (!parcelp)
	{
		mParcelSellerName = LLStringUtil::null;
		return;
	}
	
	if (mIsClaim)
	{
		mParcelSellerName = "Linden Lab";
	}
	else if (parcelp->getIsGroupOwned())
	{
		gCacheName->getGroupName(parcelp->getGroupID(), mParcelSellerName);
	}
	else
	{
		gCacheName->getFullName(parcelp->getOwnerID(), mParcelSellerName);
	}
//MK
	if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
	{
		mParcelSellerName = gAgent.mRRInterface.getDummyName (mParcelSellerName);
	}
//mk
}


void LLFloaterBuyLandUI::startTransaction(TransactionType type,
										  LLXMLRPCValue params)
{
	delete mTransaction;
	mTransaction = NULL;

	mTransactionType = type;

	// Select a URI and method appropriate for the transaction type.
	static std::string transaction_uri;
	if (transaction_uri.empty())
	{
		transaction_uri = LLViewerLogin::getInstance()->getHelperURI() + "landtool.php";
	}
	
	const char* method;
	switch (mTransactionType)
	{
		case TransactionPreflight:
			method = "preflightBuyLandPrep";
			break;
		case TransactionBuy:
			method = "buyLandPrep";
			break;
		default:
			llwarns << "LLFloaterBuyLandUI: Unknown transaction type!" << llendl;
			return;
	}

	mTransaction = new LLXMLRPCTransaction(
		transaction_uri,
		method,
		params,
		false /* don't use gzip */
		);
}

bool LLFloaterBuyLandUI::checkTransaction()
{
	if (!mTransaction)
	{
		return false;
	}
	
	if (!mTransaction->process())
	{
		return false;
	}

	if (mTransaction->status(NULL) != LLXMLRPCTransaction::StatusComplete)
	{
		tellUserError(mTransaction->statusMessage(), mTransaction->statusURI());
	}
	else {
		switch (mTransactionType)
		{	
			case TransactionPreflight:	finishWebSiteInfo();	break;
			case TransactionBuy:		finishWebSitePrep();	break;
			default: ;
		}
	}
	
	delete mTransaction;
	mTransaction = NULL;
	
	return true;
}

void LLFloaterBuyLandUI::tellUserError(
	const std::string& message, const std::string& uri)
{
	mCanBuy = false;
	mCannotBuyIsError = true;
	mCannotBuyReason = getString("fetching_error");
	mCannotBuyReason += message;
	mCannotBuyURI = uri;
}


// virtual
BOOL LLFloaterBuyLandUI::postBuild()
{
	mCurrency.prepare();
	
	childSetAction("buy_btn", onClickBuy, this);
	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("error_web", onClickErrorWeb, this);
	
	return TRUE;
}

void LLFloaterBuyLandUI::setParcel(LLViewerRegion* region, LLParcelSelectionHandle parcel)
{
	if (mTransaction &&  mTransactionType == TransactionBuy)
	{
		// the user is buying, don't change the selection
		return;
	}
	
	mRegion = region;
	mParcel = parcel;

	updateAgentInfo();
	updateParcelInfo();
	updateCovenantInfo();
	if (mCanBuy)
	{
		updateWebSiteInfo();
	}
	refreshUI();
}

void LLFloaterBuyLandUI::setForGroup(bool forGroup)
{
	mIsForGroup = forGroup;
}

void LLFloaterBuyLandUI::draw()
{
	LLFloater::draw();
	
	bool needsUpdate = false;
	needsUpdate |= checkTransaction();
	needsUpdate |= mCurrency.process();
	
	if (mBought)
	{
		close();
	}
	else if (needsUpdate)
	{
		if (mCanBuy && mCurrency.hasError())
		{
			tellUserError(mCurrency.errorMessage(), mCurrency.errorURI());
		}
		
		refreshUI();
	}
}

// virtual
BOOL LLFloaterBuyLandUI::canClose()
{
	bool can_close = (mTransaction ? FALSE : TRUE) && mCurrency.canCancel();
	if (!can_close)
	{
		// explain to user why they can't do this, see DEV-9605
		gViewerWindow->alertXml("CannotCloseFloaterBuyLand");
	}
	return can_close;
}

// virtual
void LLFloaterBuyLandUI::setMinimized(BOOL minimize)
{
	bool restored = (isMinimized() && !minimize);
	LLFloater::setMinimized(minimize);
	if (restored)
	{
		refreshUI();
	}
}

void LLFloaterBuyLandUI::onClose(bool app_quitting)
{
	LLFloater::onClose(app_quitting);
	destroy();
}


void LLFloaterBuyLandUI::refreshUI()
{
	// section zero: title area
	{
		LLTextureCtrl* snapshot = getChild<LLTextureCtrl>("info_image");
		if (snapshot)
		{
			snapshot->setImageAssetID(
				mParcelValid ? mParcelSnapshot : LLUUID::null);
		}
		
		if (mParcelValid)
		{
			childSetText("info_parcel", mParcelLocation);

			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d", mParcelActualArea);
			string_args["[AMOUNT2]"] = llformat("%d", mParcelSupportedObjects);
		
			childSetText("info_size", getString("meters_supports_object", string_args));


			childSetText("info_price",
				llformat(
					"L$ %d%s",
					mParcelPrice,
					mParcelSoldWithObjects
						? "\nsold with objects"
						: ""));
			childSetVisible("info_price", mParcelIsForSale);
		}
		else
		{
			childSetText("info_parcel", getString("no_parcel_selected"));
			childSetText("info_size", LLStringUtil::null);
			childSetText("info_price", LLStringUtil::null);
		}
		
		childSetText("info_action",
			mCanBuy
				?
					mIsForGroup
						? getString("buying_for_group")//"Buying land for group:"
						: getString("buying_will")//"Buying this land will:"
				: 
					mCannotBuyIsError
						? getString("cannot_buy_now")//"Cannot buy now:"
						: getString("not_for_sale")//"Not for sale:"

			);
	}
	
	bool showingError = !mCanBuy || !mSiteValid;
	
	// error section
	if (showingError)
	{
		mChildren.setBadge(std::string("step_error"),
			mCannotBuyIsError
				? LLViewChildren::BADGE_ERROR
				: LLViewChildren::BADGE_WARN);
		
		LLTextBox* message = getChild<LLTextBox>("error_message");
		if (message)
		{
			message->setVisible(true);
			message->setWrappedText(
				!mCanBuy ? mCannotBuyReason : "(waiting for data)"
				);
		}

		childSetVisible("error_web", 
				mCannotBuyIsError && !mCannotBuyURI.empty());
	}
	else
	{
		childHide("step_error");
		childHide("error_message");
		childHide("error_web");
	}
	
	
	// section one: account
	if (!showingError)
	{
		mChildren.setBadge(std::string("step_1"),
			mSiteMembershipUpgrade
				? LLViewChildren::BADGE_NOTE
				: LLViewChildren::BADGE_OK);
		childSetText("account_action", mSiteMembershipAction);
		childSetText("account_reason", 
			mSiteMembershipUpgrade
				?	getString("must_upgrade")
				:	getString("cant_own_land")
			);
		
		LLComboBox* levels = getChild<LLComboBox>( "account_level");
		if (levels)
		{
			levels->setVisible(mSiteMembershipUpgrade);
			
			levels->removeall();
			for(std::vector<std::string>::const_iterator i
					= mSiteMembershipPlanNames.begin();
				i != mSiteMembershipPlanNames.end();
				++i)
			{
				levels->add(*i);
			}
			
			levels->setCurrentByIndex(mUserPlanChoice);
		}

		childShow("step_1");
		childShow("account_action");
		childShow("account_reason");
	}
	else
	{
		childHide("step_1");
		childHide("account_action");
		childHide("account_reason");
		childHide("account_level");
	}
	
	// section two: land use fees
	if (!showingError)
	{
		mChildren.setBadge(std::string("step_2"),
			mSiteLandUseUpgrade
				? LLViewChildren::BADGE_NOTE
				: LLViewChildren::BADGE_OK);
		childSetText("land_use_action", mSiteLandUseAction);
		
		std::string message;
		
		if (mIsForGroup)
		{
			LLStringUtil::format_map_t string_args;
			string_args["[GROUP]"] = std::string(gAgent.mGroupName);

			message += getString("insufficient_land_credits", string_args);
				
		}
		else
		{
			LLStringUtil::format_map_t string_args;
			string_args["[BUYER]"] = llformat("%d", mAgentCommittedTier);
			message += getString("land_holdings", string_args);
		}
		
		if (!mParcelValid)
		{
			message += "(no parcel selected)";
		}
		else if (mParcelBillableArea == mParcelActualArea)
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d", mParcelActualArea);
			message += getString("parcel_meters", string_args);
		}
		else
		{

			if (mParcelBillableArea > mParcelActualArea)
			{	
				LLStringUtil::format_map_t string_args;
				string_args["[AMOUNT]"] = llformat("%d", mParcelBillableArea);
				message += getString("premium_land", string_args);
			}
			else
			{
				LLStringUtil::format_map_t string_args;
				string_args["[AMOUNT]"] = llformat("%d", mParcelBillableArea);
				message += getString("discounted_land", string_args);
			}
		}

		childSetWrappedText("land_use_reason", message);

		childShow("step_2");
		childShow("land_use_action");
		childShow("land_use_reason");
	}
	else
	{
		childHide("step_2");
		childHide("land_use_action");
		childHide("land_use_reason");
	}
	
	// section three: purchase & currency
	S32 finalBalance = mAgentCashBalance + mCurrency.getAmount() - mParcelPrice;
	bool willHaveEnough = finalBalance >= 0;
	bool haveEnough = mAgentCashBalance >= mParcelPrice;
	S32 minContribution = llceil((F32)mParcelBillableArea / GROUP_LAND_BONUS_FACTOR);
	bool groupContributionEnough = mParcelGroupContribution >= minContribution;
	
	mCurrency.updateUI(!showingError  &&  !haveEnough);

	if (!showingError)
	{
		mChildren.setBadge(std::string("step_3"),
			!willHaveEnough
				? LLViewChildren::BADGE_WARN
				: mCurrency.getAmount() > 0
					? LLViewChildren::BADGE_NOTE
					: LLViewChildren::BADGE_OK);
			
		childSetText("purchase_action",
			llformat(
				"Pay L$ %d to %s for this land",
				mParcelPrice,
				mParcelSellerName.c_str()
				));
		childSetVisible("purchase_action", mParcelValid);
		
		std::string reasonString;

		if (haveEnough)
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d", mAgentCashBalance);

			childSetText("currency_reason", getString("have_enough_lindens", string_args));
		}
		else
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d", mAgentCashBalance);
			string_args["[AMOUNT2]"] = llformat("%d", mParcelPrice - mAgentCashBalance);
			
			childSetText("currency_reason", getString("not_enough_lindens", string_args));

			childSetTextArg("currency_est", "[AMOUNT2]", llformat("%#.2f", mCurrency.getEstimate() / 100.0));
		}
		
		if (willHaveEnough)
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d", finalBalance);

			childSetText("currency_balance", getString("balance_left", string_args));

		}
		else
		{
			LLStringUtil::format_map_t string_args;
			string_args["[AMOUNT]"] = llformat("%d", mParcelPrice - mAgentCashBalance);
	
			childSetText("currency_balance", getString("balance_needed", string_args));
			
		}

		childSetValue("remove_contribution", LLSD(groupContributionEnough));
		childSetEnabled("remove_contribution", groupContributionEnough);
		bool showRemoveContribution = mParcelIsGroupLand
							&& (mParcelGroupContribution > 0);
		childSetLabelArg("remove_contribution", "[AMOUNT]",
							llformat("%d", minContribution));
		childSetVisible("remove_contribution", showRemoveContribution);

		childShow("step_3");
		childShow("purchase_action");
		childShow("currency_reason");
		childShow("currency_balance");
	}
	else
	{
		childHide("step_3");
		childHide("purchase_action");
		childHide("currency_reason");
		childHide("currency_balance");
		childHide("remove_group_donation");
	}


	bool agrees_to_covenant = false;
	LLCheckBoxCtrl* check = getChild<LLCheckBoxCtrl>("agree_covenant");
	if (check)
	{
	    agrees_to_covenant = check->get();
	}

	childSetEnabled("buy_btn",
		mCanBuy  &&  mSiteValid  &&  willHaveEnough  &&  !mTransaction && agrees_to_covenant);
}

void LLFloaterBuyLandUI::startBuyPreConfirm()
{
	std::string action;
	
	if (mSiteMembershipUpgrade)
	{
		action += mSiteMembershipAction;
		action += "\n";

		LLComboBox* levels = getChild<LLComboBox>( "account_level");
		if (levels)
		{
			action += " * ";
			action += mSiteMembershipPlanNames[levels->getCurrentIndex()];
			action += "\n";
		}
	}
	if (mSiteLandUseUpgrade)
	{
		action += mSiteLandUseAction;
		action += "\n";
	}
	if (mCurrency.getAmount() > 0)
	{
		LLStringUtil::format_map_t string_args;
		string_args["[AMOUNT]"] = llformat("%d", mCurrency.getAmount());
		string_args["[AMOUNT2]"] = llformat("%#.2f", mCurrency.getEstimate() / 100.0);
		
		action += getString("buy_for_US", string_args);
	}

	LLStringUtil::format_map_t string_args;
	string_args["[AMOUNT]"] = llformat("%d", mParcelPrice);
	string_args["[SELLER]"] = mParcelSellerName;
	action += getString("pay_to_for_land", string_args);
		
	
	LLConfirmationManager::confirm(mSiteConfirm,
		action,
		*this,
		&LLFloaterBuyLandUI::startBuyPostConfirm);
}

void LLFloaterBuyLandUI::startBuyPostConfirm(const std::string& password)
{
	runWebSitePrep(password);
	
	mCanBuy = false;
	mCannotBuyReason = getString("processing");
	refreshUI();
}


// static
void LLFloaterBuyLandUI::onClickBuy(void* data)
{
	LLFloaterBuyLandUI* self = (LLFloaterBuyLandUI*)data;
	self->startBuyPreConfirm();
}

// static
void LLFloaterBuyLandUI::onClickCancel(void* data)
{
	LLFloaterBuyLandUI* self = (LLFloaterBuyLandUI*)data;
	self->close();
}

// static
void LLFloaterBuyLandUI::onClickErrorWeb(void* data)
{
	LLFloaterBuyLandUI* self = (LLFloaterBuyLandUI*)data;
	LLWeb::loadURLExternal(self->mCannotBuyURI);
	self->close();
}



