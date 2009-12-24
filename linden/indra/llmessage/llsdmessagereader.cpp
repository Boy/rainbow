/** 
 * @file llsdmessagereader.cpp
 * @brief LLSDMessageReader class implementation.
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

#include "linden_common.h"

#include "llsdmessagereader.h"

#include "llmessagebuilder.h"
#include "llsdmessagebuilder.h"
#include "llsdutil.h"

#include "v3math.h"
#include "v4math.h"
#include "v3dmath.h"
#include "v2math.h"
#include "llquaternion.h"
#include "v4color.h"

LLSDMessageReader::LLSDMessageReader() :
	mMessageName(NULL)
{
}

//virtual 
LLSDMessageReader::~LLSDMessageReader()
{
}


LLSD getLLSD(const LLSD& input, const char* block, const char* var, S32 blocknum)
{
	// babbage: log error to llerrs if variable not found to mimic
	// LLTemplateMessageReader::getData behaviour
	if(NULL == block)
	{
		llerrs << "NULL block name" << llendl;
		return LLSD();
	}
	if(NULL == var)
	{
		llerrs << "NULL var name" << llendl;
		return LLSD();
	}
	if(! input[block].isArray())
	{
		// NOTE: babbage: need to return default for missing blocks to allow
		// backwards/forwards compatibility - handlers must cope with default
		// values.
		llwarns << "block " << block << " not found" << llendl;
		return LLSD();
	}

	LLSD result = input[block][blocknum][var]; 
	if(result.isUndefined())
	{
		// NOTE: babbage: need to return default for missing vars to allow
		// backwards/forwards compatibility - handlers must cope with default
		// values.
		llwarns << "var " << var << " not found" << llendl;
	}
	return result;
}

//virtual 
void LLSDMessageReader::getBinaryData(const char *block, const char *var, 
									  void *datap, S32 size, S32 blocknum, 
									  S32 max_size)
{
	std::vector<U8> data = getLLSD(mMessage, block, var, blocknum);
	S32 data_size = (S32)data.size();

	if (size && data_size != size)
	{
		return;
	}
	
	if (max_size < data_size)
	{
		data_size = max_size;
	}
	
	memcpy(datap, &(data[0]), data_size);
}

//virtual 
void LLSDMessageReader::getBOOL(const char *block, const char *var, 
								BOOL &data, 
								S32 blocknum)
{
	data = getLLSD(mMessage, block, var, blocknum);
}

//virtual 
void LLSDMessageReader::getS8(const char *block, const char *var, S8 &data, 
					   S32 blocknum)
{
	data = getLLSD(mMessage, block, var, blocknum).asInteger();
}

//virtual 
void LLSDMessageReader::getU8(const char *block, const char *var, U8 &data, 
					   S32 blocknum)
{
	data = getLLSD(mMessage, block, var, blocknum).asInteger();
}

//virtual 
void LLSDMessageReader::getS16(const char *block, const char *var, S16 &data, 
						S32 blocknum)
{
	data = getLLSD(mMessage, block, var, blocknum).asInteger();
}

//virtual 
void LLSDMessageReader::getU16(const char *block, const char *var, U16 &data, 
						S32 blocknum)
{
	data = getLLSD(mMessage, block, var, blocknum).asInteger();
}

//virtual 
void LLSDMessageReader::getS32(const char *block, const char *var, S32 &data, 
						S32 blocknum)
{
	data = getLLSD(mMessage, block, var, blocknum);
}

//virtual 
void LLSDMessageReader::getF32(const char *block, const char *var, F32 &data, 
						S32 blocknum)
{
	data = (F32)getLLSD(mMessage, block, var, blocknum).asReal();
}

//virtual 
void LLSDMessageReader::getU32(const char *block, const char *var, U32 &data, 
						S32 blocknum)
{
	data = ll_U32_from_sd(getLLSD(mMessage, block, var, blocknum));
}

//virtual 
void LLSDMessageReader::getU64(const char *block, const char *var,
								U64 &data, S32 blocknum)
{
	data = ll_U64_from_sd(getLLSD(mMessage, block, var, blocknum));
}

//virtual 
void LLSDMessageReader::getF64(const char *block, const char *var,
								F64 &data, S32 blocknum)
{
	data = getLLSD(mMessage, block, var, blocknum);
}

//virtual 
void LLSDMessageReader::getVector3(const char *block, const char *var, 
							LLVector3 &vec, S32 blocknum)
{
	vec = ll_vector3_from_sd(getLLSD(mMessage, block, var, blocknum));
}

//virtual 
void LLSDMessageReader::getVector4(const char *block, const char *var, 
							LLVector4 &vec, S32 blocknum)
{
	vec = ll_vector4_from_sd(getLLSD(mMessage, block, var, blocknum));
}

//virtual 
void LLSDMessageReader::getVector3d(const char *block, const char *var, 
							 LLVector3d &vec, S32 blocknum)
{
	vec = ll_vector3d_from_sd(getLLSD(mMessage, block, var, blocknum));
}

//virtual 
void LLSDMessageReader::getQuat(const char *block, const char *var,
								 LLQuaternion &q, S32 blocknum)
{
	q = ll_quaternion_from_sd(getLLSD(mMessage, block, var, blocknum));
}

//virtual 
void LLSDMessageReader::getUUID(const char *block, const char *var,
								 LLUUID &uuid, S32 blocknum)
{
	uuid = getLLSD(mMessage, block, var, blocknum);
}

//virtual 
void LLSDMessageReader::getIPAddr(const char *block, const char *var,
								   U32 &ip, S32 blocknum)
{
	ip = ll_ipaddr_from_sd(getLLSD(mMessage, block, var, blocknum));
}

//virtual 
void LLSDMessageReader::getIPPort(const char *block, const char *var,
								   U16 &port, S32 blocknum)
{
	port = getLLSD(mMessage, block, var, blocknum).asInteger();
}

//virtual 
void LLSDMessageReader::getString(const char *block, const char *var, 
						   S32 buffer_size, char *buffer, S32 blocknum)
{
	if(buffer_size <= 0)
	{
		llwarns << "buffer_size <= 0" << llendl;
		return;
	}
	std::string data = getLLSD(mMessage, block, var, blocknum);
	S32 data_size = data.size();
	if (data_size >= buffer_size)
	{
		data_size = buffer_size - 1;
	}
	memcpy(buffer, data.data(), data_size);
	buffer[data_size] = '\0';
}

//virtual 
void LLSDMessageReader::getString(const char *block, const char *var, 
						   std::string& outstr, S32 blocknum)
{
	outstr = getLLSD(mMessage, block, var, blocknum).asString();
}

//virtual 
S32	LLSDMessageReader::getNumberOfBlocks(const char *blockname)
{
	return mMessage[blockname].size();
}

S32 getElementSize(const LLSD& llsd)
{
	LLSD::Type type = llsd.type();
	switch(type)
	{
	case LLSD::TypeBoolean:
		return sizeof(bool);
	case LLSD::TypeInteger:
		return sizeof(S32);
	case LLSD::TypeReal:
		return sizeof(F64);
	case LLSD::TypeString:
		return llsd.asString().size();
	case LLSD::TypeUUID:
		return sizeof(LLUUID);
	case LLSD::TypeDate:
		return sizeof(LLDate);
	case LLSD::TypeURI:
		return sizeof(LLURI);
	case LLSD::TypeBinary:
	{
		std::vector<U8> data = llsd;
		return data.size() * sizeof(U8);
	}
	case LLSD::TypeMap:
	case LLSD::TypeArray:
	case LLSD::TypeUndefined:
		return 0;
	}
	return 0;
}

//virtual 
//Mainly used to find size of binary block of data
S32	LLSDMessageReader::getSize(const char *blockname, const char *varname)
{
	return getElementSize(mMessage[blockname][0][varname]);
}


//virtual 
S32	LLSDMessageReader::getSize(const char *blockname, S32 blocknum, 
							   const char *varname)
{
	return getElementSize(mMessage[blockname][blocknum][varname]);
}

//virtual 
void LLSDMessageReader::clearMessage()
{
	mMessage = LLSD();
}

//virtual 
const char* LLSDMessageReader::getMessageName() const
{
	return mMessageName;
}

// virtual 
S32 LLSDMessageReader::getMessageSize() const
{
	return 0;
}

//virtual 
void LLSDMessageReader::copyToBuilder(LLMessageBuilder& builder) const
{
	builder.copyFromLLSD(mMessage);
}

void LLSDMessageReader::setMessage(const char* name, const LLSD& message)
{
	mMessageName = name;
	// TODO: Validate
	mMessage = message;
}
