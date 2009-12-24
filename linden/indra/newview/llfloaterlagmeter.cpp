/** 
 * @file llfloaterlagmeter.cpp
 * @brief The "Lag-o-Meter" floater used to tell users what is causing lag.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llfloaterlagmeter.h"

#include "lluictrlfactory.h"
#include "llviewerstats.h"
#include "llviewerimage.h"
#include "llviewercontrol.h"
#include "llappviewer.h"

#include "lltexturefetch.h"

#include "llbutton.h"
#include "llfocusmgr.h"
#include "lltextbox.h"

const std::string LAG_CRITICAL_IMAGE_NAME = "lag_status_critical.tga";
const std::string LAG_WARNING_IMAGE_NAME  = "lag_status_warning.tga";
const std::string LAG_GOOD_IMAGE_NAME     = "lag_status_good.tga";

LLFloaterLagMeter::LLFloaterLagMeter(const LLSD& key)
	:	LLFloater(std::string("floater_lagmeter"))
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_lagmeter.xml");

	// Don't let this window take keyboard focus -- it's confusing to
	// lose arrow-key driving when testing lag.
	setIsChrome(TRUE);

	mClientButton = getChild<LLButton>("client_lagmeter");
	mClientText = getChild<LLTextBox>("client_text");
	mClientCause = getChild<LLTextBox>("client_lag_cause");

	mNetworkButton = getChild<LLButton>("network_lagmeter");
	mNetworkText = getChild<LLTextBox>("network_text");
	mNetworkCause = getChild<LLTextBox>("network_lag_cause");

	mServerButton = getChild<LLButton>("server_lagmeter");
	mServerText = getChild<LLTextBox>("server_text");
	mServerCause = getChild<LLTextBox>("server_lag_cause");

	std::string config_string = getString("client_frame_rate_critical_fps", mStringArgs);
	mClientFrameTimeCritical = 1.0f / (float)atof( config_string.c_str() );
	config_string = getString("client_frame_rate_warning_fps", mStringArgs);
	mClientFrameTimeWarning = 1.0f / (float)atof( config_string.c_str() );

	config_string = getString("network_packet_loss_critical_pct", mStringArgs);
	mNetworkPacketLossCritical = (float)atof( config_string.c_str() );
	config_string = getString("network_packet_loss_warning_pct", mStringArgs);
	mNetworkPacketLossWarning = (float)atof( config_string.c_str() );

	config_string = getString("network_ping_critical_ms", mStringArgs);
	mNetworkPingCritical = (float)atof( config_string.c_str() );
	config_string = getString("network_ping_warning_ms", mStringArgs);
	mNetworkPingWarning = (float)atof( config_string.c_str() );
	config_string = getString("server_frame_rate_critical_fps", mStringArgs);

	mServerFrameTimeCritical = 1000.0f / (float)atof( config_string.c_str() );
	config_string = getString("server_frame_rate_warning_fps", mStringArgs);
	mServerFrameTimeWarning = 1000.0f / (float)atof( config_string.c_str() );
	config_string = getString("server_single_process_max_time_ms", mStringArgs);
	mServerSingleProcessMaxTime = (float)atof( config_string.c_str() );

	mShrunk = false;
	config_string = getString("max_width_px", mStringArgs);
	mMaxWidth = atoi( config_string.c_str() );
	config_string = getString("min_width_px", mStringArgs);
	mMinWidth = atoi( config_string.c_str() );

	mStringArgs["[CLIENT_FRAME_RATE_CRITICAL]"] = getString("client_frame_rate_critical_fps");
	mStringArgs["[CLIENT_FRAME_RATE_CRITICAL]"] = getString("client_frame_rate_critical_fps");
	mStringArgs["[CLIENT_FRAME_RATE_WARNING]"] = getString("client_frame_rate_warning_fps");

	mStringArgs["[NETWORK_PACKET_LOSS_CRITICAL]"] = getString("network_packet_loss_critical_pct");
	mStringArgs["[NETWORK_PACKET_LOSS_CRITICAL]"] = getString("network_packet_loss_critical_pct");
	mStringArgs["[NETWORK_PACKET_LOSS_WARNING]"] = getString("network_packet_loss_warning_pct");

	mStringArgs["[NETWORK_PING_CRITICAL]"] = getString("network_ping_critical_ms");
	mStringArgs["[NETWORK_PING_CRITICAL]"] = getString("network_ping_critical_ms");
	mStringArgs["[NETWORK_PING_WARNING]"] = getString("network_ping_warning_ms");

	mStringArgs["[SERVER_FRAME_RATE_CRITICAL]"] = getString("server_frame_rate_critical_fps");
	mStringArgs["[SERVER_FRAME_RATE_CRITICAL]"] = getString("server_frame_rate_critical_fps");
	mStringArgs["[SERVER_FRAME_RATE_WARNING]"] = getString("server_frame_rate_warning_fps");

	childSetAction("minimize", onClickShrink, this);

	// were we shrunk last time?
	if (gSavedSettings.getBOOL("LagMeterShrunk"))
	{
		onClickShrink(this);
	}
}

LLFloaterLagMeter::~LLFloaterLagMeter()
{
	// save shrunk status for next time
	gSavedSettings.setBOOL("LagMeterShrunk", mShrunk);
	// expand so we save the large window rectangle
	if (mShrunk)
	{
		onClickShrink(this);
	}
}

void LLFloaterLagMeter::draw()
{
	determineClient();
	determineNetwork();
	determineServer();

	LLFloater::draw();
}

void LLFloaterLagMeter::determineClient()
{
	F32 client_frame_time = LLViewerStats::getInstance()->mFPSStat.getMeanDuration();
	bool find_cause = false;

	if (!gFocusMgr.getAppHasFocus())
	{
		mClientButton->setImageUnselected(LAG_GOOD_IMAGE_NAME);
		mClientText->setText( getString("client_frame_time_window_bg_msg", mStringArgs) );
		mClientCause->setText( LLStringUtil::null );
	}
	else if(client_frame_time >= mClientFrameTimeCritical)
	{
		mClientButton->setImageUnselected(LAG_CRITICAL_IMAGE_NAME);
		mClientText->setText( getString("client_frame_time_critical_msg", mStringArgs) );
		find_cause = true;
	}
	else if(client_frame_time >= mClientFrameTimeWarning)
	{
		mClientButton->setImageUnselected(LAG_WARNING_IMAGE_NAME);
		mClientText->setText( getString("client_frame_time_warning_msg", mStringArgs) );
		find_cause = true;
	}
	else
	{
		mClientButton->setImageUnselected(LAG_GOOD_IMAGE_NAME);
		mClientText->setText( getString("client_frame_time_normal_msg", mStringArgs) );
		mClientCause->setText( LLStringUtil::null );
	}	

	if(find_cause)
	{
		if(gSavedSettings.getF32("RenderFarClip") > 128)
		{
			mClientCause->setText( getString("client_draw_distance_cause_msg", mStringArgs) );
		}
		else if(LLAppViewer::instance()->getTextureFetch()->getNumRequests() > 2)
		{
			mClientCause->setText( getString("client_texture_loading_cause_msg", mStringArgs) );
		}
		else if((LLViewerImage::sBoundTextureMemory >> 20) > LLViewerImage::sMaxBoundTextureMem)
		{
			mClientCause->setText( getString("client_texture_memory_cause_msg", mStringArgs) );
		}
		else 
		{
			mClientCause->setText( getString("client_complex_objects_cause_msg", mStringArgs) );
		}
	}
}

void LLFloaterLagMeter::determineNetwork()
{
	F32 packet_loss = LLViewerStats::getInstance()->mPacketsLostPercentStat.getMean();
	F32 ping_time = LLViewerStats::getInstance()->mSimPingStat.getMean();
	bool find_cause_loss = false;
	bool find_cause_ping = false;

	if(packet_loss >= mNetworkPacketLossCritical)
	{
		mNetworkButton->setImageUnselected(LAG_CRITICAL_IMAGE_NAME);
		mNetworkText->setText( getString("network_packet_loss_critical_msg", mStringArgs) );
		find_cause_loss = true;
	}
	else if(ping_time >= mNetworkPingCritical)
	{
		mNetworkButton->setImageUnselected(LAG_CRITICAL_IMAGE_NAME);
		mNetworkText->setText( getString("network_ping_critical_msg", mStringArgs) );
		find_cause_ping = true;
	}
	else if(packet_loss >= mNetworkPacketLossWarning)
	{
		mNetworkButton->setImageUnselected(LAG_WARNING_IMAGE_NAME);
		mNetworkText->setText( getString("network_packet_loss_warning_msg", mStringArgs) );
		find_cause_loss = true;
	}
	else if(ping_time >= mNetworkPingWarning)
	{
		mNetworkButton->setImageUnselected(LAG_WARNING_IMAGE_NAME);
		mNetworkText->setText( getString("network_ping_warning_msg", mStringArgs) );
		find_cause_ping = true;
	}
	else
	{
		mNetworkButton->setImageUnselected(LAG_GOOD_IMAGE_NAME);
		mNetworkText->setText( getString("network_performance_normal_msg", mStringArgs) );
	}

	if(find_cause_loss)
 	{
		mNetworkCause->setText( getString("network_packet_loss_cause_msg", mStringArgs) );
 	}
	else if(find_cause_ping)
	{
		mNetworkCause->setText( getString("network_ping_cause_msg", mStringArgs) );
	}
	else
	{
		mNetworkCause->setText( LLStringUtil::null );
	}
}

void LLFloaterLagMeter::determineServer()
{
	F32 sim_frame_time = LLViewerStats::getInstance()->mSimFrameMsec.getCurrent();
	bool find_cause = false;

	if(sim_frame_time >= mServerFrameTimeCritical)
	{
		mServerButton->setImageUnselected(LAG_CRITICAL_IMAGE_NAME);
		mServerText->setText( getString("server_frame_time_critical_msg", mStringArgs) );
		find_cause = true;
	}
	else if(sim_frame_time >= mServerFrameTimeWarning)
	{
		mServerButton->setImageUnselected(LAG_WARNING_IMAGE_NAME);
		mServerText->setText( getString("server_frame_time_warning_msg", mStringArgs) );
		find_cause = true;
	}
	else
	{
		mServerButton->setImageUnselected(LAG_GOOD_IMAGE_NAME);
		mServerText->setText( getString("server_frame_time_normal_msg", mStringArgs) );
		mServerCause->setText( LLStringUtil::null );
	}	

	if(find_cause)
	{
		if(LLViewerStats::getInstance()->mSimSimPhysicsMsec.getCurrent() > mServerSingleProcessMaxTime)
		{
			mServerCause->setText( getString("server_physics_cause_msg", mStringArgs) );
		}
		else if(LLViewerStats::getInstance()->mSimScriptMsec.getCurrent() > mServerSingleProcessMaxTime)
		{
			mServerCause->setText( getString("server_scripts_cause_msg", mStringArgs) );
		}
		else if(LLViewerStats::getInstance()->mSimNetMsec.getCurrent() > mServerSingleProcessMaxTime)
		{
			mServerCause->setText( getString("server_net_cause_msg", mStringArgs) );
		}
		else if(LLViewerStats::getInstance()->mSimAgentMsec.getCurrent() > mServerSingleProcessMaxTime)
		{
			mServerCause->setText( getString("server_agent_cause_msg", mStringArgs) );
		}
		else if(LLViewerStats::getInstance()->mSimImagesMsec.getCurrent() > mServerSingleProcessMaxTime)
		{
			mServerCause->setText( getString("server_images_cause_msg", mStringArgs) );
		}
		else
		{
			mServerCause->setText( getString("server_generic_cause_msg", mStringArgs) );
		}
	}
}

//static
void LLFloaterLagMeter::onClickShrink(void * data)
{
	LLFloaterLagMeter * self = (LLFloaterLagMeter*)data;

	LLButton * button = self->getChild<LLButton>("minimize");
	S32 delta_width = self->mMaxWidth - self->mMinWidth;
	LLRect r = self->getRect();
	if(self->mShrunk)
	{
		self->setTitle( self->getString("max_title_msg", self->mStringArgs) );
		// make left edge appear to expand
		r.translate(-delta_width, 0);
		self->setRect(r);
		self->reshape(self->mMaxWidth, self->getRect().getHeight());
		
		self->childSetText("client", self->getString("client_text_msg", self->mStringArgs) + ":");
		self->childSetText("network", self->getString("network_text_msg", self->mStringArgs) + ":");
		self->childSetText("server", self->getString("server_text_msg", self->mStringArgs) + ":");

		// usually "<<"
		button->setLabel( self->getString("smaller_label", self->mStringArgs) );
	}
	else
	{
		self->setTitle( self->getString("min_title_msg", self->mStringArgs) );
		// make left edge appear to collapse
		r.translate(delta_width, 0);
		self->setRect(r);
		self->reshape(self->mMinWidth, self->getRect().getHeight());
		
		self->childSetText("client", self->getString("client_text_msg", self->mStringArgs) );
		self->childSetText("network", self->getString("network_text_msg", self->mStringArgs) );
		self->childSetText("server", self->getString("server_text_msg", self->mStringArgs) );

		// usually ">>"
		button->setLabel( self->getString("bigger_label", self->mStringArgs) );
	}
	// Don't put keyboard focus on the button
	button->setFocus(FALSE);

	self->mClientText->setVisible(self->mShrunk);
	self->mClientCause->setVisible(self->mShrunk);
	self->childSetVisible("client_help", self->mShrunk);

	self->mNetworkText->setVisible(self->mShrunk);
	self->mNetworkCause->setVisible(self->mShrunk);
	self->childSetVisible("network_help", self->mShrunk);

	self->mServerText->setVisible(self->mShrunk);
	self->mServerCause->setVisible(self->mShrunk);
	self->childSetVisible("server_help", self->mShrunk);

	self->mShrunk = !self->mShrunk;
}
