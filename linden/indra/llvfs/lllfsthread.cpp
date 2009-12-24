/** 
 * @file lllfsthread.cpp
 * @brief LLLFSThread base class
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "linden_common.h"
#include "lllfsthread.h"
#include "llstl.h"
#include "llapr.h"

//============================================================================

/*static*/ LLLFSThread* LLLFSThread::sLocal = NULL;

//============================================================================
// Run on MAIN thread
//static
void LLLFSThread::initClass(bool local_is_threaded)
{
	llassert(sLocal == NULL);
	sLocal = new LLLFSThread(local_is_threaded);
}

//static
S32 LLLFSThread::updateClass(U32 ms_elapsed)
{
	sLocal->update(ms_elapsed);
	return sLocal->getPending();
}

//static
void LLLFSThread::cleanupClass()
{
	sLocal->setQuitting();
	while (sLocal->getPending())
	{
		sLocal->update(0);
	}
	delete sLocal;
	sLocal = 0;
}

//----------------------------------------------------------------------------

LLLFSThread::LLLFSThread(bool threaded) :
	LLQueuedThread("LFS", threaded),
	mPriorityCounter(PRIORITY_LOWBITS)
{
}

LLLFSThread::~LLLFSThread()
{
	// ~LLQueuedThread() will be called here
}

//----------------------------------------------------------------------------

LLLFSThread::handle_t LLLFSThread::read(const std::string& filename,	/* Flawfinder: ignore */ 
										U8* buffer, S32 offset, S32 numbytes,
										Responder* responder, U32 priority)
{
	handle_t handle = generateHandle();

	if (priority == 0) priority = PRIORITY_NORMAL | priorityCounter();
	else if (priority < PRIORITY_LOW) priority |= PRIORITY_LOW; // All reads are at least PRIORITY_LOW

	Request* req = new Request(this, handle, priority,
							   FILE_READ, filename,
							   buffer, offset, numbytes,
							   responder);

	bool res = addRequest(req);
	if (!res)
	{
		llerrs << "LLLFSThread::read called after LLLFSThread::cleanupClass()" << llendl;
	}

	return handle;
}

LLLFSThread::handle_t LLLFSThread::write(const std::string& filename,
										 U8* buffer, S32 offset, S32 numbytes,
										 Responder* responder, U32 priority)
{
	handle_t handle = generateHandle();

	if (priority == 0) priority = PRIORITY_LOW | priorityCounter();
	
	Request* req = new Request(this, handle, priority,
							   FILE_WRITE, filename,
							   buffer, offset, numbytes,
							   responder);

	bool res = addRequest(req);
	if (!res)
	{
		llerrs << "LLLFSThread::read called after LLLFSThread::cleanupClass()" << llendl;
	}
	
	return handle;
}

//============================================================================

LLLFSThread::Request::Request(LLLFSThread* thread,
							  handle_t handle, U32 priority,
							  operation_t op, const std::string& filename,
							  U8* buffer, S32 offset, S32 numbytes,
							  Responder* responder) :
	QueuedRequest(handle, priority, FLAG_AUTO_COMPLETE),
	mThread(thread),
	mOperation(op),
	mFileName(filename),
	mBuffer(buffer),
	mOffset(offset),
	mBytes(numbytes),
	mBytesRead(0),
	mResponder(responder)
{
	if (numbytes <= 0)
	{
		llwarns << "LLLFSThread: Request with numbytes = " << numbytes << llendl;
	}
}

LLLFSThread::Request::~Request()
{
}

// virtual, called from own thread
void LLLFSThread::Request::finishRequest(bool completed)
{
	if (mResponder.notNull())
	{
		mResponder->completed(completed ? mBytesRead : 0);
		mResponder = NULL;
	}
}

void LLLFSThread::Request::deleteRequest()
{
	if (getStatus() == STATUS_QUEUED)
	{
		llerrs << "Attempt to delete a queued LLLFSThread::Request!" << llendl;
	}	
	if (mResponder.notNull())
	{
		mResponder->completed(0);
		mResponder = NULL;
	}
	LLQueuedThread::QueuedRequest::deleteRequest();
}

bool LLLFSThread::Request::processRequest()
{
	bool complete = false;
	if (mOperation ==  FILE_READ)
	{
		llassert(mOffset >= 0);
		apr_file_t* filep = ll_apr_file_open(mFileName, LL_APR_RB, mThread->mAPRPoolp);
		if (!filep)
		{
			llwarns << "LLLFS: Unable to read file: " << mFileName << llendl;
			mBytesRead = 0; // fail
			return true;
		}
		S32 off;
		if (mOffset < 0)
			off = ll_apr_file_seek(filep, APR_END, 0);
		else
			off = ll_apr_file_seek(filep, APR_SET, mOffset);
		llassert_always(off >= 0);
		mBytesRead = ll_apr_file_read(filep, mBuffer, mBytes );
		apr_file_close(filep);
		complete = true;
// 		llinfos << "LLLFSThread::READ:" << mFileName << " Bytes: " << mBytesRead << llendl;
	}
	else if (mOperation ==  FILE_WRITE)
	{
		apr_int32_t flags = APR_CREATE|APR_WRITE|APR_BINARY;
		if (mOffset < 0)
			flags |= APR_APPEND;
		apr_file_t* filep = ll_apr_file_open(mFileName, flags, mThread->mAPRPoolp);
		if (!filep)
		{
			llwarns << "LLLFS: Unable to write file: " << mFileName << llendl;
			mBytesRead = 0; // fail
			return true;
		}
		if (mOffset >= 0)
		{
			S32 seek = ll_apr_file_seek(filep, APR_SET, mOffset);
			if (seek < 0)
			{
				apr_file_close(filep);
				llwarns << "LLLFS: Unable to write file (seek failed): " << mFileName << llendl;
				mBytesRead = 0; // fail
				return true;
			}
		}
		mBytesRead = ll_apr_file_write(filep, mBuffer, mBytes );
		complete = true;
		apr_file_close(filep);
// 		llinfos << "LLLFSThread::WRITE:" << mFileName << " Bytes: " << mBytesRead << "/" << mBytes << " Offset:" << mOffset << llendl;
	}
	else
	{
		llerrs << "LLLFSThread::unknown operation: " << (S32)mOperation << llendl;
	}
	return complete;
}

//============================================================================

LLLFSThread::Responder::~Responder()
{
}

//============================================================================
