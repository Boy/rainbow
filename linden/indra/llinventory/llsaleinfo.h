/** 
 * @file llsaleinfo.h
 * @brief LLSaleInfo class header file.
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

#ifndef LL_LLSALEINFO_H
#define LL_LLSALEINFO_H

#include "llpermissionsflags.h"
#include "llsd.h"
#include "llxmlnode.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLSaleInfo
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// L$ default price for objects
const S32 DEFAULT_PRICE = 10;

class LLMessageSystem;

class LLSaleInfo
{
public:
	// use this to avoid temporary object creation
	static const LLSaleInfo DEFAULT;

	enum EForSale
	{
		// item is not to be considered for transactions
		FS_NOT = 0,

		// the origional is on sale
		FS_ORIGINAL = 1,

		// A copy is for sale
		FS_COPY = 2,

		// Valid only for tasks, the inventory is for sale
		// at the price in this structure.
		FS_CONTENTS = 3,

		FS_COUNT
	};

protected:
	EForSale mSaleType;
	S32 mSalePrice;

public:
	// default constructor is fine usually
	LLSaleInfo();
	LLSaleInfo(EForSale sale_type, S32 sale_price);

	// accessors
	BOOL isForSale() const;
	EForSale getSaleType() const { return mSaleType; }
	S32 getSalePrice() const { return mSalePrice; }
	U32 getCRC32() const;

	// mutators
	void setSaleType(EForSale type)		{ mSaleType = type; }
	void setSalePrice(S32 price);
	//void setNextOwnerPermMask(U32 mask)	{ mNextOwnerPermMask = mask; }


	// file serialization
	BOOL exportFile(LLFILE* fp) const;
	BOOL importFile(LLFILE* fp, BOOL& has_perm_mask, U32& perm_mask);

	BOOL exportLegacyStream(std::ostream& output_stream) const;
	LLSD asLLSD() const;
	operator LLSD() const { return asLLSD(); }
	bool fromLLSD(LLSD& sd, BOOL& has_perm_mask, U32& perm_mask);
	BOOL importLegacyStream(std::istream& input_stream, BOOL& has_perm_mask, U32& perm_mask);

	LLXMLNode *exportFileXML() const;
	BOOL importXML(LLXMLNode* node);

	LLSD packMessage() const;
	void unpackMessage(LLSD sales);

	// message serialization
	void packMessage(LLMessageSystem* msg) const;
	void unpackMessage(LLMessageSystem* msg, const char* block);
	void unpackMultiMessage(LLMessageSystem* msg, const char* block,
							S32 block_num);

	// static functionality for determine for sale status.
	static EForSale lookup(const char* name);
	static const char* lookup(EForSale type);

	// Allow accumulation of sale info. The price of each is added,
	// conflict in sale type results in FS_NOT, and the permissions
	// are tightened.
	void accumulate(const LLSaleInfo& sale_info);

	bool operator==(const LLSaleInfo &rhs) const;
	bool operator!=(const LLSaleInfo &rhs) const;
};

// These functions convert between structured data and sale info as
// appropriate for serialization.
LLSD ll_create_sd_from_sale_info(const LLSaleInfo& sale);
LLSaleInfo ll_sale_info_from_sd(const LLSD& sd);

#endif // LL_LLSALEINFO_H
