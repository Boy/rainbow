/** 
 * @file lscript_execute.h
 * @brief Classes to execute bytecode
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
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#ifndef LL_LSCRIPT_EXECUTE_H
#define LL_LSCRIPT_EXECUTE_H

#include "lscript_byteconvert.h"
#include "linked_lists.h"
#include "lscript_library.h"

class LLTimer;

// Return values for run() methods
const U32 NO_DELETE_FLAG	= 0x0000;
const U32 DELETE_FLAG		= 0x0001;
const U32 CREDIT_MONEY_FLAG	= 0x0002;

// list of op code execute functions
BOOL run_noop(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pop(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pops(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_popl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_popv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_popq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_poparg(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_popip(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_popbp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_popsp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_popslr(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_dup(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_dups(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_dupl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_dupv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_dupq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_store(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_stores(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_storel(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_storev(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_storeq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_storeg(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_storegs(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_storegl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_storegv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_storegq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_loadp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_loadsp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_loadlp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_loadvp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_loadqp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_loadgp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_loadgsp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_loadglp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_loadgvp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_loadgqp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_push(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushs(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushg(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushgs(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushgl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushgv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushgq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_puship(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushbp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushsp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushargb(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushargi(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushargf(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushargs(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushargv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushargq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushe(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pushev(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pusheq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_pusharge(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_add(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_sub(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_mul(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_div(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_mod(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_eq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_neq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_leq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_geq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_less(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_greater(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_bitand(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_bitor(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_bitxor(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_booland(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_boolor(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_shl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_shr(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_neg(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_bitnot(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_boolnot(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_jump(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_jumpif(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_jumpnif(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_state(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_call(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_return(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_cast(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_stacktos(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_stacktol(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_print(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

BOOL run_calllib(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);
BOOL run_calllib_two_byte(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

void unknown_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void integer_integer_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void integer_float_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void integer_vector_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void float_integer_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void float_float_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void float_vector_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void string_string_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void string_key_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void key_string_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void key_key_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void vector_integer_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void vector_float_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void vector_vector_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void vector_quaternion_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void quaternion_quaternion_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);


void integer_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void float_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void string_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void key_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void vector_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void quaternion_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void list_integer_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void list_float_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void list_string_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void list_key_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void list_vector_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void list_quaternion_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void list_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);

void integer_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void float_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void vector_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void quaternion_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode);

class LLScriptDataCollection
{
public:
	LLScriptDataCollection(LSCRIPTStateEventType type, LLScriptLibData *data)
		: mType(type), mData(data)
	{
	}
	LLScriptDataCollection(U8 *src, S32 &offset)
	{
		S32 i, number;
		mType = (LSCRIPTStateEventType)bytestream2integer(src, offset);
		number = bytestream2integer(src,  offset);

		mData = new LLScriptLibData[number];

		for (i = 0; i < number; i++)
		{
			mData[i].set(src, offset);
		}

	}

	~LLScriptDataCollection()
	{
		delete [] mData;
		mData = NULL;
	}

	S32  getSavedSize()
	{
		S32 size = 0;
		// mTyoe
		size += 4;
		// number of entries
		size += 4;

		S32 i = 0;
		do
		{
			size += mData[i].getSavedSize();;
		}
		while (mData[i++].mType != LST_NULL);
		return size;
	}

	S32	 write2bytestream(U8 *dest)
	{
		S32 offset = 0;
		// mTyoe
		integer2bytestream(dest, offset, mType);
		// count number of entries
		S32 number = 0;
		while (mData[number++].mType != LST_NULL)
			;
		integer2bytestream(dest, offset, number);

		// now the entries themselves
		number = 0;
		do
		{
			offset += mData[number].write2bytestream(dest + offset);
		}
		while (mData[number++].mType != LST_NULL);
		return offset;
	}


	LSCRIPTStateEventType	mType;
	LLScriptLibData			*mData;
};
const S32 MAX_EVENTS_IN_QUEUE = 64;

class LLScriptEventData
{
public:
	LLScriptEventData()		{}
	LLScriptEventData(U8 *src, S32 &offset)
	{
		S32 i, number = bytestream2integer(src, offset);
		for (i = 0; i < number; i++)
		{
			mEventDataList.addData(new LLScriptDataCollection(src, offset));
		}
	}

	void set(U8 *src, S32 &offset)
	{
		S32 i, number = bytestream2integer(src, offset);
		for (i = 0; i < number; i++)
		{
			mEventDataList.addData(new LLScriptDataCollection(src, offset));
		}
	}

	~LLScriptEventData()	
	{
		mEventDataList.deleteAllData();
	}

	void addEventData(LLScriptDataCollection *data)
	{
		if (mEventDataList.getLength() < MAX_EVENTS_IN_QUEUE)
			mEventDataList.addDataAtEnd(data);
		else
			delete data;
	}
	LLScriptDataCollection *getNextEvent(LSCRIPTStateEventType type)
	{
		LLScriptDataCollection *temp;
		for (temp = mEventDataList.getFirstData();
			 temp;
			 temp = mEventDataList.getNextData())
		{
			if (temp->mType == type)
			{
				mEventDataList.removeCurrentData();
				return temp;
			}
		}
		return NULL;
	}
	LLScriptDataCollection *getNextEvent()
	{
		LLScriptDataCollection *temp;
		temp = mEventDataList.getFirstData();
		if (temp)
		{
			mEventDataList.removeCurrentData();
			return temp;
		}
		return NULL;
	}
	void removeEventType(LSCRIPTStateEventType type)
	{
		LLScriptDataCollection *temp;
		for (temp = mEventDataList.getFirstData();
			 temp;
			 temp = mEventDataList.getNextData())
		{
			if (temp->mType == type)
			{
				mEventDataList.deleteCurrentData();
			}
		}
	}

	S32  getSavedSize()
	{
		S32 size = 0;
		// number in linked list
		size += 4;
		LLScriptDataCollection *temp;
		for (temp = mEventDataList.getFirstData();
			 temp;
			 temp = mEventDataList.getNextData())
		{
			size += temp->getSavedSize();
		}
		return size;
	}

	S32	 write2bytestream(U8 *dest)
	{
		S32 offset = 0;
		// number in linked list
		S32 number = mEventDataList.getLength();
		integer2bytestream(dest, offset, number);
		LLScriptDataCollection *temp;
		for (temp = mEventDataList.getFirstData();
			 temp;
			 temp = mEventDataList.getNextData())
		{
			offset += temp->write2bytestream(dest + offset);
		}
		return offset;
	}

	LLLinkedList<LLScriptDataCollection>	mEventDataList;
};

class LLScriptExecute
{
public:
	LLScriptExecute();
	virtual ~LLScriptExecute()  = 0;
	virtual S32 getVersion() const = 0;
	virtual void deleteAllEvents() = 0;
	virtual void addEvent(LLScriptDataCollection* event) = 0;
	virtual U32 getEventCount() = 0;
	virtual void removeEventType(LSCRIPTStateEventType event_type) = 0;
	virtual S32 getFaults() = 0;
	virtual void setFault(LSCRIPTRunTimeFaults fault) = 0;
	virtual	U32 getFreeMemory() = 0;
	virtual S32 getParameter() = 0;
	virtual void setParameter(S32 value) = 0;
	virtual F32 getSleep() const = 0;
	virtual void setSleep(F32 value) = 0;
	virtual F32 getEnergy() const = 0;
	virtual void setEnergy(F32 value) = 0;
	virtual U64 getCurrentEvents() = 0;
	virtual void setCurrentEvents(U64 value) = 0;
	virtual U64 getEventHandlers() = 0;
	virtual void setEventHandlers(U64 value) = 0;
	virtual U64 getCurrentHandler() = 0;
	virtual void setCurrentHandler(U64 value) = 0;
	virtual BOOL isFinished() const = 0;
	virtual BOOL isStateChangePending() const = 0;
	virtual S32 writeState(U8 **dest, U32 header_size, U32 footer_size) = 0; // Allocate memory for header, state and footer return size of state.
	virtual U32 getEventsSavedSize() = 0; // Returns 0 if events are written with state.
	virtual S32 writeEvents(U8 *dest) = 0; // Must write and return exactly the number of bytes returned by getEventsSavedSize.
	virtual void readEvents(U8* src, S32& offset) = 0;
	virtual S32 readState(U8 *src) = 0; // Returns number of bytes read.
	virtual void reset();
	virtual const U8* getBytecode() const = 0;
	virtual U32 getBytecodeSize() const = 0;
	virtual bool isMono() const = 0;
	virtual void error() {;} // Processing that must be performed when error flag is set and so run is not called.

	virtual U32 getUsedMemory() = 0;
	
	// Run current event handler for a maximum of time_slice seconds.
	// Updates current handler and current events registers.
	virtual void resumeEventHandler(BOOL b_print, const LLUUID &id, F32 time_slice) = 0;

	// Run handler for event for a maximum of time_slice seconds.
	// Updates current handler and current events registers.
	virtual void callEventHandler(LSCRIPTStateEventType event, const LLUUID &id, F32 time_slice) = 0;;

	// Run handler for next queued event for maximum of time_slice seconds. 
	// Updates current handler and current events registers.
	// Removes processed event from queue.
	virtual void callNextQueuedEventHandler(U64 event_register, const LLUUID &id, F32 time_slice) = 0;

	// Run handler for event for a maximum of time_slice seconds.
	// Updates current handler and current events registers.
	// Removes processed event from queue.
	virtual void callQueuedEventHandler(LSCRIPTStateEventType event, const LLUUID &id, F32 time_slice) = 0;

	// Switch to next state.
	// Returns new set of handled events.
	virtual U64 nextState() = 0; 

	// Returns time taken.
	virtual F32 runQuanta(BOOL b_print, const LLUUID &id,
						  const char **errorstr, 
						  F32 quanta,
						  U32& events_processed, LLTimer& timer);

	// NOTE: babbage: this must be used on occasions where another script may already be executing. Only 2 levels of nesting are allowed.
	// Provided to support bizarre detach behaviour only. Do not use.
	virtual F32 runNested(BOOL b_print, const LLUUID &id,
						  const char **errorstr, 
						  F32 quanta,
						  U32& events_processed, LLTimer& timer);

	// Run smallest possible amount of code: an instruction for LSL2, a segment
	// between save tests for Mono
	void runInstructions(BOOL b_print, const LLUUID &id,
						 const char **errorstr, 
						 U32& events_processed,
						 F32 quanta);

	bool isYieldDue() const;

	void setReset(BOOL b) {mReset = b;}
	BOOL getReset() const { return mReset; }

	// Called when the script is scheduled to be run from newsim/LLScriptData
	virtual void startRunning() = 0;

	// Called when the script is scheduled to be stopped from newsim/LLScriptData
	virtual void stopRunning() = 0;
	
	// A timer is regularly checked to see if script takes too long, but we
	// don't do it every opcode due to performance hits.
	static void		setTimerCheckSkip( S32 value )			{ sTimerCheckSkip = value;		}
	static S32		getTimerCheckSkip()						{ return sTimerCheckSkip;		}

private:

	BOOL mReset;

	static	S32		sTimerCheckSkip;		// Number of times to skip the timer check for performance reasons
};

class LLScriptExecuteLSL2 : public LLScriptExecute
{
public:
	LLScriptExecuteLSL2(LLFILE *fp);
	LLScriptExecuteLSL2(const U8* bytecode, U32 bytecode_size);
	virtual ~LLScriptExecuteLSL2();

	virtual S32 getVersion() const {return get_register(mBuffer, LREG_VN);}
	virtual void deleteAllEvents() {mEventData.mEventDataList.deleteAllData();}
	virtual void addEvent(LLScriptDataCollection* event);
	virtual U32 getEventCount() {return mEventData.mEventDataList.getLength();}
	virtual void removeEventType(LSCRIPTStateEventType event_type);
	virtual S32 getFaults() {return get_register(mBuffer, LREG_FR);}
	virtual void setFault(LSCRIPTRunTimeFaults fault) {set_fault(mBuffer, fault);}
	virtual U32 getFreeMemory();
	virtual S32 getParameter();
	virtual void setParameter(S32 value);
	virtual F32 getSleep() const;
	virtual void setSleep(F32 value);
	virtual F32 getEnergy() const;
	virtual void setEnergy(F32 value);
	virtual U64 getCurrentEvents() {return get_event_register(mBuffer, LREG_CE, getMajorVersion());}
	virtual void setCurrentEvents(U64 value) {return set_event_register(mBuffer, LREG_CE, value, getMajorVersion());}
	virtual U64 getEventHandlers() {return get_event_register(mBuffer, LREG_ER, getMajorVersion());}
	virtual void setEventHandlers(U64 value) {set_event_register(mBuffer, LREG_ER, value, getMajorVersion());}
	virtual U64 getCurrentHandler();
	virtual void setCurrentHandler(U64 value) {return set_event_register(mBuffer, LREG_IE, value, getMajorVersion());}	
	virtual BOOL isFinished() const {return get_register(mBuffer, LREG_IP) == 0;}
	virtual BOOL isStateChangePending() const {return get_register(mBuffer, LREG_CS) != get_register(mBuffer, LREG_NS);}
	virtual S32 writeState(U8 **dest, U32 header_size, U32 footer_size); // Not including Events.
	virtual U32 getEventsSavedSize() {return mEventData.getSavedSize();}
	virtual S32 writeEvents(U8 *dest) {return mEventData.write2bytestream(dest);}
	virtual void readEvents(U8* src, S32& offset) {mEventData.set(src, offset);}
	virtual S32 writeBytecode(U8 **dest);
	virtual S32 readState(U8 *src);
	virtual void reset();
	virtual const U8* getBytecode() const {return mBytecode;}
	virtual U32 getBytecodeSize() const {return mBytecodeSize;}
	virtual bool isMono() const {return false;}
	virtual U32 getUsedMemory();
	// Run current event handler for a maximum of time_slice seconds.
	// Updates current handler and current events registers.
	virtual void resumeEventHandler(BOOL b_print, const LLUUID &id, F32 time_slice);

	// Run handler for event for a maximum of time_slice seconds.
	// Updates current handler and current events registers.
	virtual void callEventHandler(LSCRIPTStateEventType event, const LLUUID &id, F32 time_slice);

	// Run handler for next queued event for maximum of time_slice seconds. 
	// Updates current handler and current events registers.
	// Removes processed event from queue.
	virtual void callNextQueuedEventHandler(U64 event_register, const LLUUID &id, F32 time_slice);

	// Run handler for event for a maximum of time_slice seconds.
	// Updates current handler and current events registers.
	// Removes processed event from queue.
	virtual void callQueuedEventHandler(LSCRIPTStateEventType event, const LLUUID &id, F32 time_slice);

	// Switch to next state.
	// Returns new set of handled events.
	virtual U64 nextState(); 

	void init();

	BOOL (*mExecuteFuncs[0x100])(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id);

	U32						mInstructionCount;
	U8						*mBuffer;
	LLScriptEventData		mEventData;
	U8*						mBytecode; // Initial state and bytecode.
	U32						mBytecodeSize;

private:
	S32 getMajorVersion() const;
	void		recordBoundaryError( const LLUUID &id );
	void		setStateEventOpcoodeStartSafely( S32 state, LSCRIPTStateEventType event, const LLUUID &id );

	// Called when the script is scheduled to be run from newsim/LLScriptData
	virtual void startRunning();

	// Called when the script is scheduled to be stopped from newsim/LLScriptData
	virtual void stopRunning();
};

#endif
