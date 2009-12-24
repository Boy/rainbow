/** 
 * @file lluseroperation.h
 * @brief LLUserOperation class header file - used for message based
 * transaction. For example, L$ transactions.
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

#ifndef LL_LLUSEROPERATION_H
#define LL_LLUSEROPERATION_H

#include "lluuid.h"
#include "llframetimer.h"

#include <map>

class LLUserOperation
{
public:
	LLUserOperation(const LLUUID& agent_id);
	LLUserOperation(const LLUUID& agent_id, const LLUUID& transaction_id);
	virtual ~LLUserOperation();

	const LLUUID& getTransactionID() const { return mTransactionID; }
	const LLUUID& getAgentID() const { return mAgentID; }

	// Operation never got necessary data, so expired	
	virtual BOOL isExpired();

	// ability to mark this operation as never expiring.
	void SetNoExpireFlag(const BOOL flag);

	// Send request to the dataserver
	virtual void sendRequest() = 0;

	// Run the operation. This will only be called in the case of an
	// actual success or failure of the operation.
	virtual BOOL execute(BOOL transaction_success) = 0;	

	// This method is called when the user op has expired, and is
	// about to be deleted by the manager. This gives the user op the
	// ability to nack someone when the user op is never evaluated
	virtual void expire();

protected:
	LLUserOperation();
	
protected:
	LLUUID mAgentID;
	LLUUID mTransactionID;
	LLFrameTimer mTimer;
	BOOL   mNoExpire;			// this is used for operations that expect an answer and will wait till it gets one. 
};


class LLUserOperationMgr
{
public:
	LLUserOperationMgr();
	~LLUserOperationMgr();

	void addOperation(LLUserOperation* op);
	LLUserOperation* findOperation(const LLUUID& transaction_id);
	BOOL deleteOperation(LLUserOperation* op);

	// Call this method every once in a while to clean up old
	// transactions.
	void deleteExpiredOperations();
	
private:
	typedef std::map<LLUUID, LLUserOperation*> user_operation_list_t;
	user_operation_list_t mUserOperationList;
	LLUUID mLastOperationConsidered;
};

extern LLUserOperationMgr* gUserOperationMgr;

#endif // LL_LLUSEROPERATION_H
