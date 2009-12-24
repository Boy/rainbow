/** 
 * @file lldarray.h
 * @brief Wrapped std::vector for backward compatibility.
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

#ifndef LL_LLDARRAY_H
#define LL_LLDARRAY_H

#include "llerror.h"

#include <vector>
#include <map>

// class LLDynamicArray<>; // = std::vector + reserves <BlockSize> elements
// class LLDynamicArrayIndexed<>; // = std::vector + std::map if indices, only supports operator[] and begin(),end()

//--------------------------------------------------------
// LLDynamicArray declaration
//--------------------------------------------------------
// NOTE: BlockSize is used to reserve a minimal initial amount
template <typename Type, int BlockSize = 32> 
class LLDynamicArray : public std::vector<Type>
{
public:
	enum
	{
		OKAY = 0,
		FAIL = -1
	};
	
	LLDynamicArray(S32 size=0) : std::vector<Type>(size) { if (size < BlockSize) std::vector<Type>::reserve(BlockSize); }

	void reset() { std::vector<Type>::resize(0); }

	// ACCESSORS
	const Type& get(S32 index) const	 			{ return std::vector<Type>::operator[](index); }
	Type&       get(S32 index)						{ return std::vector<Type>::operator[](index); }
	S32			find(const Type &obj) const;

	S32			count() const						{ return std::vector<Type>::size(); }
	S32			getLength() const					{ return std::vector<Type>::size(); }
	S32			getMax() const						{ return std::vector<Type>::capacity(); }

	// MANIPULATE
	S32         put(const Type &obj);					// add to end of array, returns index
// 	Type*		reserve(S32 num);					// reserve a block of indices in advance
	Type*		reserve_block(U32 num);			// reserve a block of indices in advance

	S32			remove(S32 index);				// remove by index, no bounds checking
	S32			removeObj(const Type &obj);				// remove by object
	S32			removeLast();

	void		operator+=(const LLDynamicArray<Type,BlockSize> &other);
};

//--------------------------------------------------------
// LLDynamicArray implementation
//--------------------------------------------------------

template <typename Type,int BlockSize>
inline S32 LLDynamicArray<Type,BlockSize>::find(const Type &obj) const
{
	typename std::vector<Type>::const_iterator iter = std::find(this->begin(), this->end(), obj);
	if (iter != this->end())
	{
		return iter - this->begin();
	}
	return FAIL;
}


template <typename Type,int BlockSize>
inline S32 LLDynamicArray<Type,BlockSize>::remove(S32 i)
{
	// This is a fast removal by swapping with the last element
	S32 sz = this->size();
	if (i < 0 || i >= sz)
	{
		return FAIL;
	}
	if (i < sz-1)
	{
		this->operator[](i) = this->back();
	}
	this->pop_back();
	return i;
}

template <typename Type,int BlockSize>
inline S32 LLDynamicArray<Type,BlockSize>::removeObj(const Type& obj)
{
	typename std::vector<Type>::iterator iter = std::find(this->begin(), this->end(), obj);
	if (iter != this->end())
	{
		S32 res = iter - this->begin();
		typename std::vector<Type>::iterator last = this->end(); 
		--last;
		*iter = *last;
		this->pop_back();
		return res;
	}
	return FAIL;
}

template <typename Type,int BlockSize>
inline S32	LLDynamicArray<Type,BlockSize>::removeLast()
{
	if (!this->empty())
	{
		this->pop_back();
		return OKAY;
	}
	return FAIL;
}

template <typename Type,int BlockSize>
inline Type* LLDynamicArray<Type,BlockSize>::reserve_block(U32 num)
{
	U32 sz = this->size();
	this->resize(sz+num);
	return &(this->operator[](sz));
}

template <typename Type,int BlockSize>
inline S32	LLDynamicArray<Type,BlockSize>::put(const Type &obj) 
{
	this->push_back(obj);
	return this->size() - 1;
}

template <typename Type,int BlockSize>
inline void LLDynamicArray<Type,BlockSize>::operator+=(const LLDynamicArray<Type,BlockSize> &other)
{
	insert(this->end(), other.begin(), other.end());
}

//--------------------------------------------------------
// LLDynamicArrayIndexed declaration
//--------------------------------------------------------

template <typename Type, typename Key, int BlockSize = 32> 
class LLDynamicArrayIndexed
{
public:
	typedef typename std::vector<Type>::iterator iterator;
	typedef typename std::vector<Type>::const_iterator const_iterator;
	typedef typename std::vector<Type>::reverse_iterator reverse_iterator;
	typedef typename std::vector<Type>::const_reverse_iterator const_reverse_iterator;
	typedef typename std::vector<Type>::size_type size_type;
protected:
	std::vector<Type> mVector;
	std::map<Key, U32> mIndexMap;
	
public:
	LLDynamicArrayIndexed() { mVector.reserve(BlockSize); }
	
	iterator begin() { return mVector.begin(); }
	const_iterator begin() const { return mVector.begin(); }
	iterator end() { return mVector.end(); }
	const_iterator end() const { return mVector.end(); }

	reverse_iterator rbegin() { return mVector.rbegin(); }
	const_reverse_iterator rbegin() const { return mVector.rbegin(); }
	reverse_iterator rend() { return mVector.rend(); }
	const_reverse_iterator rend() const { return mVector.rend(); }

	void reset() { mVector.resize(0); mIndexMap.resize(0); }
	bool empty() const { return mVector.empty(); }
	size_type size() const { return mVector.size(); }
	
	Type& operator[](const Key& k)
	{
		typename std::map<Key, U32>::const_iterator iter = mIndexMap.find(k);
		if (iter == mIndexMap.end())
		{
			U32 n = mVector.size();
			mIndexMap[k] = n;
			mVector.resize(n+1);
			llassert(mVector.size() == mIndexMap.size());
			return mVector[n];
		}
		else
		{
			return mVector[iter->second];
		}
	}

	const_iterator find(const Key& k) const
	{
		typename std::map<Key, U32>::const_iterator iter = mIndexMap.find(k);
		if(iter == mIndexMap.end())
		{
			return mVector.end();
		}
		else
		{
			return mVector.begin() + iter->second;
		}
	}
};

#endif
