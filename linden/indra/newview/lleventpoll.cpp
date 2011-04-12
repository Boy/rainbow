/**
 * @file lleventpoll.cpp
 * @brief Implementation of the LLEventPoll class.
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

#include "lleventpoll.h"
#include "llappviewer.h"
#include "llagent.h"

#include "llhttpclient.h"
#include "llhttpstatuscodes.h"
#include "llsdserialize.h"
#include "lltimer.h"
#include "llviewerregion.h"
#include "message.h"
#include "lltrans.h"

namespace
{
	// We will wait RETRY_SECONDS + (errorCount * RETRY_SECONDS_INC) before retrying after an error.
	// This means we attempt to recover relatively quickly but back off giving more time to recover
	// until we finally give up after MAX_EVENT_POLL_HTTP_ERRORS attempts.
	const F32 EVENT_POLL_ERROR_RETRY_SECONDS = 15.f; // ~ half of a normal timeout.
	const F32 EVENT_POLL_ERROR_RETRY_SECONDS_INC = 5.f; // ~ half of a normal timeout.
	const S32 MAX_EVENT_POLL_HTTP_ERRORS = 10; // ~5 minutes, by the above rules.

	class LLEventPollResponder : public LLHTTPClient::Responder
	{
	public:
		
		static LLHTTPClient::ResponderPtr start(const std::string& pollURL, const LLHost& sender);
		void stop();
		
		void makeRequest();

	private:
		LLEventPollResponder(const std::string&	pollURL, const LLHost& sender);
		~LLEventPollResponder();

		
		void handleMessage(const LLSD& content);
		virtual	void error(U32 status, const std::string& reason);
		virtual	void result(const LLSD&	content);

		virtual void completedRaw(U32 status,
									const std::string& reason,
									const LLChannelDescriptors& channels,
									const LLIOPipe::buffer_ptr_t& buffer);
	private:

		bool	mDone;

		std::string			mPollURL;
		std::string			mSender;
		
		LLSD	mAcknowledge;
		
		// these are only here for debugging so	we can see which poller	is which
		static int sCount;
		int	mCount;
		S32 mErrorCount;
	};

	class LLEventPollEventTimer : public LLEventTimer
	{
		typedef boost::intrusive_ptr<LLEventPollResponder> EventPollResponderPtr;

	public:
		LLEventPollEventTimer(F32 period, EventPollResponderPtr responder)
			: LLEventTimer(period), mResponder(responder)
		{ }

		virtual BOOL tick()
		{
			mResponder->makeRequest();
			return TRUE;	// Causes this instance to be deleted.
		}

	private:
		
		EventPollResponderPtr mResponder;
	};

	//static
	LLHTTPClient::ResponderPtr LLEventPollResponder::start(
		const std::string& pollURL, const LLHost& sender)
	{
		LLHTTPClient::ResponderPtr result = new LLEventPollResponder(pollURL, sender);
		llinfos	<< "LLEventPollResponder::start <" << sCount << "> "
				<< pollURL << llendl;
		return result;
	}

	void LLEventPollResponder::stop()
	{
		llinfos	<< "LLEventPollResponder::stop	<" << mCount <<	"> "
				<< mPollURL	<< llendl;
		// there should	be a way to	stop a LLHTTPClient	request	in progress
		mDone =	true;
	}

	int	LLEventPollResponder::sCount =	0;

	LLEventPollResponder::LLEventPollResponder(const std::string& pollURL, const LLHost& sender)
		: mDone(false),
		  mPollURL(pollURL),
		  mCount(++sCount),
		  mErrorCount(0)
	{
		//extract host and port of simulator to set as sender
		LLViewerRegion *regionp = gAgent.getRegion();
		if (!regionp)
		{
			// OGPX : Changed from llerrs to llwarns.
			// No longer an llerrs error, because we might be starting an event queue before we have a region. 
			
			llwarns << "LLEventPoll initialized before region is added." << llendl;
		}
		mSender = sender.getIPandPort();
		llinfos << "LLEventPoll initialized with sender " << mSender << llendl;
		makeRequest();
	}

	LLEventPollResponder::~LLEventPollResponder()
	{
		stop();
		lldebugs <<	"LLEventPollResponder::~Impl <" <<	mCount << "> "
				 <<	mPollURL <<	llendl;
	}

	// virtual 
	void LLEventPollResponder::completedRaw(U32 status,
									const std::string& reason,
									const LLChannelDescriptors& channels,
									const LLIOPipe::buffer_ptr_t& buffer)
	{
		if (status == HTTP_BAD_GATEWAY)
		{
			// These errors are not parsable as LLSD, 
			// which LLHTTPClient::Responder::completedRaw will try to do.
			completed(status, reason, LLSD());
		}
		else
		{
			LLHTTPClient::Responder::completedRaw(status,reason,channels,buffer);
		}
	}

	void LLEventPollResponder::makeRequest()
	{
		LLSD request;
		request["ack"] = mAcknowledge;
		request["done"]	= mDone;
		
		lldebugs <<	"LLEventPollResponder::makeRequest	<" << mCount <<	"> ack = "
				 <<	LLSDXMLStreamer(mAcknowledge) << llendl;
		LLHTTPClient::post(mPollURL, request, this);
	}

	void LLEventPollResponder::handleMessage(const	LLSD& content)
	{
		std::string	msg_name	= content["message"];
		LLSD message;
		message["sender"] = mSender;
		message["body"] = content["body"];
		LLMessageSystem::dispatch(msg_name, message);
	}

	//virtual
	void LLEventPollResponder::error(U32 status, const	std::string& reason)
	{
		if (mDone) return;

		// A HTTP_BAD_GATEWAY (502) error is our standard timeout response
		// we get this when there are no events.
		if ( status == HTTP_BAD_GATEWAY )	
		{
			mErrorCount = 0;
			makeRequest();
		}
		else if (mErrorCount < MAX_EVENT_POLL_HTTP_ERRORS)
		{
			++mErrorCount;
			
			// The 'tick' will return TRUE causing the timer to delete this.
			new LLEventPollEventTimer(EVENT_POLL_ERROR_RETRY_SECONDS
										+ mErrorCount * EVENT_POLL_ERROR_RETRY_SECONDS_INC
									, this);

			llwarns << "Unexpected HTTP error.  status: " << status << ", reason: " << reason << llendl;
		}
		else
		{
			llwarns <<	"LLEventPollResponder::error: <" << mCount << "> got "
					<<	status << ": " << reason
					<<	(mDone ? " -- done"	: "") << llendl;
			stop();

			// At this point we have given up and the viewer will not receive HTTP messages from the simulator.
			// IMs, teleports, about land, selecing land, region crossing and more will all fail.
			// They are essentially disconnected from the region even though some things may still work.
			// Since things won't get better until they relog we force a disconnect now.

			// *NOTE:Mani - The following condition check to see if this failing event poll
			// is attached to the Agent's main region. If so we disconnect the viewer.
			// Else... its a child region and we just leave the dead event poll stopped and 
			// continue running.
			if(gAgent.getRegion() && gAgent.getRegion()->getHost().getIPandPort() == mSender)
			{
				llwarns << "Forcing disconnect due to stalled main region event poll."  << llendl;
				// OGPX - Is this valid in the OGPX case? forceDisconnect() pops up a dialog.
				//   in the OGPX case, if we lose the connection to the region, we might still
				//   have one to the Agent Domain.
				//   Since the viewer doesn't currently implement any functionality that involves
				//   being connected to the Agent Domain without also being connected to a region, leave the
				//   call to forceDisconnect() here.
				LLAppViewer::instance()->forceDisconnect(LLTrans::getString("AgentLostConnection"));
			}
		}
	}

	//virtual
	void LLEventPollResponder::result(const LLSD& content)
	{
		lldebugs <<	"LLEventPollResponder::result <" << mCount	<< ">"
				 <<	(mDone ? " -- done"	: "") << llendl;
		
		if (mDone) return;

		mErrorCount = 0;

		if (!content.get("events") ||
			!content.get("id"))
		{
			llwarns << "received event poll with no events or id key" << llendl;
			makeRequest();
			return;
		}
		
		mAcknowledge = content["id"];
		LLSD events	= content["events"];

		if(mAcknowledge.isUndefined())
		{
			llwarns << "LLEventPollResponder: id undefined" << llendl;
		}
		
		// was llinfos but now that CoarseRegionUpdate is TCP @ 1/second, it'd be too verbose for viewer logs. -MG
		lldebugs  << "LLEventPollResponder::completed <" <<	mCount << "> " << events.size() << "events (id "
				 <<	LLSDXMLStreamer(mAcknowledge) << ")" << llendl;
		
		LLSD::array_const_iterator i = events.beginArray();
		LLSD::array_const_iterator end = events.endArray();
		for	(; i !=	end; ++i)
		{
			if (i->has("message"))
			{
				handleMessage(*i);
			}
		}
		
		makeRequest();
	}	

	// OGPX : So this area of the code is an acknowledged mess... but... 
	// it is also an area that will be changing a lot as OGPX changes and grows.
	// Leave the event queue code completely separate (agent vs region eq) so that
	// normal legacy region eq connections aren't subject to the thrashing 
	// that agent eq code will have.
	//
	// Similar to the way a sim needs to invoke a request on the client without doing
	// an actual inbound http request, this is the similar mechanism for 
	// the Agent Domain. One area of current investigation is how the viewer
	// might accomodate requests from multiple services (i.e., we shouldn't make
	// assumptions about how a particular OGPX grid or agent domain has carved up 
	// the handling of different pieces of functionality). 
	

	class LLAgentEventPollResponder : public LLHTTPClient::Responder
	{
	public:
		
		static LLHTTPClient::ResponderPtr start(const std::string& pollURL);
		void stop();
		virtual void makeRequest();
		virtual void makeRequest(const LLSD&);
		
	private:
		LLAgentEventPollResponder(const std::string&	pollURL);
		~LLAgentEventPollResponder();

		//void handleMessage(const LLSD& content);
		virtual	void error(U32 status, const std::string& reason);
		virtual	void result(const LLSD&	content);

	private:

		bool	mDone;
		int		mAcknowledge ;  // OGPX : id of request to send back along with the response to Agent Domain.
								// We will probably change the specifics of returning the result of
								// a resource request. 

		std::string			mPollURL;
		
		// these are only here for debugging so	we can see which poller	is which
		static int sCount;
		int	mCount;
	};


	//static
	LLHTTPClient::ResponderPtr LLAgentEventPollResponder::start(
		const std::string& pollURL)
	{
		LLHTTPClient::ResponderPtr result = new LLAgentEventPollResponder(pollURL);
		llinfos	<< "LLAgentEventPollResponder::start <" << sCount << "> "
				<< pollURL << llendl;
		return result;
	}

	void LLAgentEventPollResponder::stop()
	{
		llinfos	<< "LLAgentEventPollResponder::stop	<" << mCount <<	"> "
				<< mPollURL	<< llendl;
		// there should	be a way to	stop a LLHTTPClient	request	in progress
		mDone =	true;
	}

	int	LLAgentEventPollResponder::sCount =	0;

	LLAgentEventPollResponder::LLAgentEventPollResponder(const std::string& pollURL)
		: mDone(false),
		  mPollURL(pollURL),
		  mAcknowledge(0),
		  mCount(++sCount)
	{
		
		makeRequest();
	}

	LLAgentEventPollResponder::~LLAgentEventPollResponder()
	{
		stop();
		lldebugs <<	"LLAgentEventPollResponder::~Impl <" <<	mCount << "> "
				 <<	mPollURL <<	llendl;
	}

	// OGPX : Should LLAgentEventPollResponder inherits from normal EventPollResponder,
	// or will the two classes diverge as we figure out how AgentEventPoll will
	// actually work? Me thinks that event queue code will be changed so much that a clear
	// sharp division between the classes might not be such a bad thing right now. 
	// It's especially important to minimize code changes to the XML-RPC legacy path while
	// adding OGP code, because that minimizes risk to breaking something in the legacy path.
	void LLAgentEventPollResponder::makeRequest()
	{		
		lldebugs <<	"LLAgentEventPollResponder::makeRequest	<" << mCount << "> "<< llendl;
		LLSD request;
		request["ack"] = mAcknowledge;
		request["done"]	= mDone;
		LLHTTPClient::post(mPollURL, request, this);
	}
	
	// OGPX only function passes response back to agent domain.
	// This is our backward little way of responding to a request
	// for client side resources, and passing something back to the requestor
	// in the HTTP stream. The original makeRequest() is in place to provide a
	// regular way to 'tap' the agent domain and ask if there were any client side 
	// resources it needed to request. This function adds the ability to also 
	// pass back LLSD along with the 'tap'
	//
	// WARNING:
	// This area of the code will change a lot as OGPX is developed. When 
	// client side resources are invoked, we need some way to pass back 
	// a response on the event queue. This is one possible way.
	void LLAgentEventPollResponder::makeRequest(const LLSD& result_for_agentd)
	{		
		LLSD args;
		args["ack"] = mAcknowledge;
		args["done"] = mDone;
		args["result"] = result_for_agentd;
		lldebugs <<	"LLAgentEventPollResponder::makeRequest	<" << mCount << "> " << llendl;
		LLHTTPClient::post(mPollURL, args, this);
	}

	// virtual
	void LLAgentEventPollResponder::error(U32 status, const	std::string& reason)
	{
		if (mDone) return;

		if (status != 499)
		{
			llwarns <<	"LLAgentEventPollResponder::error: <" << mCount << "> got "
					<<	status << " : " << reason
					<<	(mDone ? " -- done"	: "") << llendl;
			stop();
			return;
		}

		makeRequest();
	}


	//virtual
	void LLAgentEventPollResponder::result(const LLSD& content)
	{
		LLSD result_for_agentd;
		lldebugs <<	"LLAgentEventPollResponder::result <" << mCount	<< ">"
				 <<	(mDone ? " -- done"	: "") << llendl;
		
		if (mDone) return;
		
		// was llinfos but now that CoarseRegionUpdate is TCP @ 1/second, it'd be too verbose for viewer logs. -MG
		lldebugs << "LLAgentEventPollResponder::completed <" <<	mCount << "> "  << llendl;


		if (!content.get("events") ||
			!content.get("id"))
		{
			llinfos << "Received event poll with no events or id key" << llendl; // was llwarns, but too frequent
			makeRequest();
			return;
		}
		mAcknowledge = 0;
		mAcknowledge = content["id"];
		LLSD events	= content["events"];

		if (mAcknowledge!=0)
		{
			llwarns << " : id undefined" << llendl;
		}
		
		// was llinfos but now that CoarseRegionUpdate is TCP @ 1/second, it'd be too verbose for viewer logs. -MG
		lldebugs << "LLEventPollResponder::completed <" <<	mCount << "> " << events.size() << "events (id "
				 <<	LLSDXMLStreamer(mAcknowledge) << ")" << llendl;

	// twiddling with making the messaging system gobble up event queue requests
#if OGPXEVENTHACK
		// OGPXEVENTHACK : An attempt at using the message_template.msg as a way to add HTTP messages that 
		// are handled by the event queue (instead of coming across UDP, and being handled via UDP decoding).
		// I found that I was able to do this in a limited way (have a message decoded into LLSD, 
		// and a handler called for it) for inbound EQ messages. Unsure it gives us the level of control
		// we need in OGPX to implement policy and trust between the viewer and other entities. The legacy
		// model was "trust everything from the region", and it won't be that way in OGPX.
		// Feels weird adding dead code to the patch, but I wanted a record of the experimentation
		// OGPX TODO: figure out proper building of services. 

		// iterate over the requests sent by agent domain
		LLSD::array_const_iterator i = events.beginArray();
		LLSD::array_const_iterator end = events.endArray();
		for	(; i !=	end; ++i)
		{
			if (i->has("message"))
			{
				std::string message = i->get("message"); 
				std::string	path = "/message/" + message; // OGPX : mmmm....seems like we are gluing it together so traverse can tear it apart
				LLSD context;
				// so this tries to traverse over the things registered
				// in llhttpnode.cpp, but i had trouble getting anything other than message to work
				const LLHTTPNode* handler =	messageRootNode().traverse(path, context);
				if (!handler)
				{
					llwarns << " no handler for "<< path << llendl;
					return;
				}

			    //lldebugs << "data: " << LLSDNotationStreamer(message) << llendl;	   

				// We've found a handler for the request, call its post() and get its LLSD response
				// so, changing from the post that was fussing with response pointer to simpler
				result_for_agentd = handler->post(*i);
				lldebugs << "after handling "<< ll_pretty_print_sd(*i) << " sending AD result: " << ll_pretty_print_sd(result_for_agentd) << llendl;
			}
		}
		// OGPXEVENTHACK end
			
		// OGPX : result_for_agentd is LLSD and passed back to agentd with the next 'tap'
		makeRequest(result_for_agentd);
#else
		// OGPX :until eventqueue code is redesigned, just send back the regular 'tap' 
		makeRequest();
#endif
	}	
	
}

LLEventPoll::LLEventPoll(const std::string&	poll_url, const LLHost& sender)
	: mImpl(LLEventPollResponder::start(poll_url, sender))
	{ }

LLEventPoll::~LLEventPoll()
{
	LLHTTPClient::Responder* responderp = mImpl.get();
	LLEventPollResponder* event_poll_responder = dynamic_cast<LLEventPollResponder*>(responderp);
	if (event_poll_responder) event_poll_responder->stop();
}

// OGPX : We maintain an event poll with the Agent Domain and with the region
// while OGP9 svn branch tried to use a funky ReverseHTTP thing for the Responder, 
// for now we will revert to simpler code. This poll should remain active for the
// life of the viewer session.
LLAgentEventPoll::LLAgentEventPoll(const std::string&	pollURL)
	: mImpl(LLAgentEventPollResponder::start(pollURL))
{
}

LLAgentEventPoll::~LLAgentEventPoll()
{
}
