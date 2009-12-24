/** 
 * @file llstat.h
 * @brief Runtime statistics accumulation.
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

#ifndef LL_LLSTAT_H
#define LL_LLSTAT_H

#include <deque>
#include <map>

#include "lltimer.h"
#include "llframetimer.h"
#include "llfile.h"

class	LLSD;

// Set this if longer stats are needed
#define ENABLE_LONG_TIME_STATS	0

//
// Accumulates statistics for an arbitrary length of time.
// Does this by maintaining a chain of accumulators, each one
// accumulation the results of the parent.  Can scale to arbitrary
// amounts of time with very low memory cost.
//

class LLStatAccum
{
protected:
	LLStatAccum(bool use_frame_timer);
	virtual ~LLStatAccum();

public:
	enum TimeScale {
		SCALE_100MS,
		SCALE_SECOND,
		SCALE_MINUTE,
#if ENABLE_LONG_TIME_STATS
		SCALE_HOUR,
		SCALE_DAY,
		SCALE_WEEK,
#endif
		NUM_SCALES,			// Use to size storage arrays
		SCALE_PER_FRAME		// For latest frame information - should be after NUM_SCALES since this doesn't go into the time buckets
	};

	static U64 sScaleTimes[NUM_SCALES];

	virtual F32 meanValue(TimeScale scale) const;
		// see the subclasses for the specific meaning of value

	F32 meanValueOverLast100ms()  const { return meanValue(SCALE_100MS);  }
	F32 meanValueOverLastSecond() const	{ return meanValue(SCALE_SECOND); }
	F32 meanValueOverLastMinute() const	{ return meanValue(SCALE_MINUTE); }

	void reset(U64 when);

	void sum(F64 value);
	void sum(F64 value, U64 when);

	U64 getCurrentUsecs() const;
		// Get current microseconds based on timer type

	BOOL	mUseFrameTimer;
	BOOL	mRunning;

	U64		mLastTime;
	
	struct Bucket
	{
		F64		accum;
		U64		endTime;

		BOOL	lastValid;
		F64		lastAccum;
	};

	Bucket	mBuckets[NUM_SCALES];

	BOOL 	mLastSampleValid;
	F64 	mLastSampleValue;
};

class LLStatMeasure : public LLStatAccum
	// gathers statistics about things that are measured
	// ex.: tempature, time dilation
{
public:
	LLStatMeasure(bool use_frame_timer = true);

	void sample(F64);
	void sample(S32 v) { sample((F64)v); }
	void sample(U32 v) { sample((F64)v); }
	void sample(S64 v) { sample((F64)v); }
	void sample(U64 v) { sample((F64)v); }
};


class LLStatRate : public LLStatAccum
	// gathers statistics about things that can be counted over time
	// ex.: LSL instructions executed, messages sent, simulator frames completed
	// renders it in terms of rate of thing per second
{
public:
	LLStatRate(bool use_frame_timer = true);

	void count(U32);
		// used to note that n items have occured
	
	void mark();
		// used for counting the rate thorugh a point in the code
};


class LLStatTime : public LLStatAccum
	// gathers statistics about time spent in a block of code
	// measure average duration per second in the block
{
public:
	LLStatTime( const std::string & key = "undefined" );

	U32		mFrameNumber;		// Current frame number
	U64		mTotalTimeInFrame;	// Total time (microseconds) accumulated during the last frame

	void	setKey( const std::string & key )		{ mKey = key;	};

	virtual F32 meanValue(TimeScale scale) const;

private:
	void start();				// Start and stop measuring time block
	void stop();

	std::string		mKey;		// Tag representing this time block

#if LL_DEBUG
	BOOL			mRunning;	// TRUE if start() has been called
#endif

	friend class LLPerfBlock;
};

// ----------------------------------------------------------------------------


// Use this class on the stack to record statistics about an area of code
class LLPerfBlock
{
public:
    struct StatEntry
    {
            StatEntry(const std::string& key) : mStat(LLStatTime(key)), mCount(0) {}
            LLStatTime  mStat;
            U32         mCount;
    };
    typedef std::map<std::string, StatEntry*>		stat_map_t;

	// Use this constructor for pre-defined LLStatTime objects
	LLPerfBlock(LLStatTime* stat);

	// Use this constructor for dynamically created LLStatTime objects (not pre-defined) with a multi-part key
	LLPerfBlock( const char* key1, const char* key2 = NULL);


	~LLPerfBlock();

	static void setStatsEnabled( BOOL enable )		{ sStatsEnabled = enable;	};
	static S32  getStatsEnabled()					{ return sStatsEnabled;		};

	static void clearDynamicStats();		// Reset maps to clear out dynamic objects
	static void addStatsToLLSDandReset( LLSD & stats,		// Get current information and clear time bin
										LLStatAccum::TimeScale scale );

private:
	// Initialize dynamically created LLStatTime objects
    void initDynamicStat(const std::string& key);

	std::string				mLastPath;				// Save sCurrentStatPath when this is called
	LLStatTime * 			mPredefinedStat;		// LLStatTime object to get data
	StatEntry *				mDynamicStat;   		// StatEntryobject to get data

	static BOOL				sStatsEnabled;			// Normally FALSE
    static stat_map_t		sStatMap;				// Map full path string to LLStatTime objects
	static std::string		sCurrentStatPath;		// Something like "frame/physics/physics step"
};

// ----------------------------------------------------------------------------

class LLPerfStats
{
public:
    LLPerfStats(const std::string& process_name = "unknown", S32 process_pid = 0);
    virtual ~LLPerfStats();

    virtual void init();    // Reset and start all stat timers
    virtual void updatePerFrameStats();
    // Override these function to add process-specific information to the performance log header and per-frame logging.
    virtual void addProcessHeaderInfo(LLSD& info) { /* not implemented */ }
    virtual void addProcessFrameInfo(LLSD& info, LLStatAccum::TimeScale scale) { /* not implemented */ }

    // High-resolution frame stats
    BOOL    frameStatsIsRunning()                                { return (mReportPerformanceStatEnd > 0.);        };
    F32     getReportPerformanceInterval() const                { return mReportPerformanceStatInterval;        };
    void    setReportPerformanceInterval( F32 interval )        { mReportPerformanceStatInterval = interval;    };
    void    setReportPerformanceDuration( F32 seconds );
    void    setProcessName(const std::string& process_name) { mProcessName = process_name; }
    void    setProcessPID(S32 process_pid) { mProcessPID = process_pid; }

protected:
    void    openPerfStatsFile();                    // Open file for high resolution metrics logging
    void    dumpIntervalPerformanceStats();

    llofstream      mFrameStatsFile;            // File for per-frame stats
    BOOL            mFrameStatsFileFailure;        // Flag to prevent repeat opening attempts
    BOOL            mSkipFirstFrameStats;        // Flag to skip one (partial) frame report
    std::string     mProcessName;
    S32             mProcessPID;

private:
    F32 mReportPerformanceStatInterval;    // Seconds between performance stats
    F64 mReportPerformanceStatEnd;        // End time (seconds) for performance stats
};

// ----------------------------------------------------------------------------
class LLStat
{
public:
	LLStat(const U32 num_bins = 32, BOOL use_frame_timer = FALSE);
	~LLStat();

	void reset();

	void start();	// Start the timer for the current "frame", otherwise uses the time tracked from
					// the last addValue
	void addValue(const F32 value = 1.f); // Adds the current value being tracked, and tracks the DT.
	void addValue(const S32 value) { addValue((F32)value); }
	void addValue(const U32 value) { addValue((F32)value); }

	void setBeginTime(const F64 time);
	void addValueTime(const F64 time, const F32 value = 1.f);
	
	S32 getCurBin() const;
	S32 getNextBin() const;
	
	F32 getCurrent() const;
	F32 getCurrentPerSec() const;
	F64 getCurrentBeginTime() const;
	F64 getCurrentTime() const;
	F32 getCurrentDuration() const;
	
	F32 getPrev(S32 age) const;				// Age is how many "addValues" previously - zero is current
	F32 getPrevPerSec(S32 age) const;		// Age is how many "addValues" previously - zero is current
	F64 getPrevBeginTime(S32 age) const;
	F64 getPrevTime(S32 age) const;
	
	F32 getBin(S32 bin) const;
	F32 getBinPerSec(S32 bin) const;
	F64 getBinBeginTime(S32 bin) const;
	F64 getBinTime(S32 bin) const;

	F32 getMax() const;
	F32 getMaxPerSec() const;
	
	F32 getMean() const;
	F32 getMeanPerSec() const;
	F32 getMeanDuration() const;

	F32 getMin() const;
	F32 getMinPerSec() const;
	F32 getMinDuration() const;

	F32 getSum() const;
	F32 getSumDuration() const;

	U32 getNumValues() const;
	S32 getNumBins() const;

	F64 getLastTime() const;
private:
	BOOL mUseFrameTimer;
	U32 mNumValues;
	U32 mNumBins;
	F32 mLastValue;
	F64 mLastTime;
	F32 *mBins;
	F64 *mBeginTime;
	F64 *mTime;
	F32 *mDT;
	S32 mCurBin;
	S32 mNextBin;
	static LLTimer sTimer;
	static LLFrameTimer sFrameTimer;
};

#endif // LL_STAT_
