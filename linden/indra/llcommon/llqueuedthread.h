/** 
 * @file llqueuedthread.h
 * @brief
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLQUEUEDTHREAD_H
#define LL_LLQUEUEDTHREAD_H

#include <queue>
#include <string>
#include <map>
#include <set>

#include "llapr.h"

#include "llthread.h"
#include "llsimplehash.h"

//============================================================================
// Note: ~LLQueuedThread is O(N) N=# of queued threads, assumed to be small
//   It is assumed that LLQueuedThreads are rarely created/destroyed.

class LLQueuedThread : public LLThread
{
	//------------------------------------------------------------------------
public:
	enum priority_t {
		PRIORITY_IMMEDIATE = 0x7FFFFFFF,
		PRIORITY_URGENT =    0x40000000,
		PRIORITY_HIGH =      0x30000000,
		PRIORITY_NORMAL =    0x20000000,
		PRIORITY_LOW =       0x10000000,
		PRIORITY_LOWBITS =   0x0FFFFFFF,
		PRIORITY_HIGHBITS =  0x70000000
	};
	enum status_t {
		STATUS_EXPIRED = -1,
		STATUS_UNKNOWN = 0,
		STATUS_QUEUED = 1,
		STATUS_INPROGRESS = 2,
		STATUS_COMPLETE = 3,
		STATUS_ABORTED = 4,
		STATUS_DELETE = 5
	};
	enum flags_t {
		FLAG_AUTO_COMPLETE = 1,
		FLAG_AUTO_DELETE = 2, // child-class dependent
		FLAG_ABORT = 4
	};

	typedef U32 handle_t;
	
	//------------------------------------------------------------------------
public:

	class QueuedRequest : public LLSimpleHashEntry<handle_t>
	{
		friend class LLQueuedThread;
		
	protected:
		virtual ~QueuedRequest(); // use deleteRequest()
		
	public:
		QueuedRequest(handle_t handle, U32 priority, U32 flags = 0);

		status_t getStatus()
		{
			return mStatus;
		}
		U32 getPriority() const
		{
			return mPriority;
		}
		U32 getFlags() const
		{
			return mFlags;
		}
		bool higherPriority(const QueuedRequest& second) const
		{
			if ( mPriority == second.mPriority)
				return mHashKey < second.mHashKey;
			else
				return mPriority > second.mPriority;
		}

	protected:
		status_t setStatus(status_t newstatus)
		{
			status_t oldstatus = mStatus;
			mStatus = newstatus;
			return oldstatus;
		}
		void setFlags(U32 flags)
		{
			// NOTE: flags are |'d
			mFlags |= flags;
		}
		
		virtual bool processRequest() = 0; // Return true when request has completed
		virtual void finishRequest(bool completed); // Always called from thread after request has completed or aborted
		virtual void deleteRequest(); // Only method to delete a request

		void setPriority(U32 pri)
		{
			// Only do this on a request that is not in a queued list!
			mPriority = pri;
		};
		
	protected:
		LLAtomic32<status_t> mStatus;
		U32 mPriority;
		U32 mFlags;
	};

protected:
	struct queued_request_less
	{
		bool operator()(const QueuedRequest* lhs, const QueuedRequest* rhs) const
		{
			return lhs->higherPriority(*rhs); // higher priority in front of queue (set)
		}
	};

	//------------------------------------------------------------------------
	
public:
	static handle_t nullHandle() { return handle_t(0); }
	
public:
	LLQueuedThread(const std::string& name, bool threaded = true);
	virtual ~LLQueuedThread();	
	virtual void shutdown();
	
private:
	// No copy constructor or copy assignment
	LLQueuedThread(const LLQueuedThread&);
	LLQueuedThread& operator=(const LLQueuedThread&);

	virtual bool runCondition(void);
	virtual void run(void);

protected:
	handle_t generateHandle();
	bool addRequest(QueuedRequest* req);
	S32  processNextRequest(void);
	void incQueue();

public:
	bool waitForResult(handle_t handle, bool auto_complete = true);

	virtual S32 update(U32 max_time_ms);
	S32 updateQueue(U32 max_time_ms);
	
	void waitOnPending();
	void printQueueStats();

	S32 getPending();
	bool getThreaded() { return mThreaded ? true : false; }

	// Request accessors
	status_t getRequestStatus(handle_t handle);
	void abortRequest(handle_t handle, bool autocomplete);
	void setFlags(handle_t handle, U32 flags);
	void setPriority(handle_t handle, U32 priority);
	bool completeRequest(handle_t handle);
	// This is public for support classes like LLWorkerThread,
	// but generally the methods above should be used.
	QueuedRequest* getRequest(handle_t handle);

	// debug (see source)
	bool check();
	
protected:
	BOOL mThreaded;  // if false, run on main thread and do updates during update()
	LLAtomic32<BOOL> mIdleThread; // request queue is empty (or we are quitting) and the thread is idle
	
	typedef std::set<QueuedRequest*, queued_request_less> request_queue_t;
	request_queue_t mRequestQueue;

	enum { REQUEST_HASH_SIZE = 512 }; // must be power of 2
	typedef LLSimpleHash<handle_t, REQUEST_HASH_SIZE> request_hash_t;
	request_hash_t mRequestHash;

	handle_t mNextHandle;
};

#endif // LL_LLQUEUEDTHREAD_H
