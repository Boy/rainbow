/** 
 * @file lluri.cpp
 * @author Phoenix
 * @date 2006-02-08
 * @brief Implementation of the LLURI class.
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

#include "linden_common.h"

#include "llapp.h"
#include "lluri.h"
#include "llsd.h"
#include <iomanip>
  
#include "lluuid.h"

// system includes
#include <boost/tokenizer.hpp>

void encode_character(std::ostream& ostr, std::string::value_type val)
{
	ostr << "%" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') 
	     // VWR-4010 Cannot cast to U32 because sign-extension on 
	     // chars > 128 will result in FFFFFFC3 instead of F3.
	     << static_cast<S32>(static_cast<U8>(val));
}

// static
std::string LLURI::escape(
	const std::string& str,
	const std::string& allowed,
	bool is_allowed_sorted)
{
	// *NOTE: This size determination feels like a good value to
	// me. If someone wante to come up with a more precise heuristic
	// with some data to back up the assertion that 'sort is good'
	// then feel free to change this test a bit.
	if(!is_allowed_sorted && (str.size() > 2 * allowed.size()))
	{
		// if it's already sorted, or if the url is quite long, we
		// want to optimize this process.
		std::string sorted_allowed(allowed);
		std::sort(sorted_allowed.begin(), sorted_allowed.end());
		return escape(str, sorted_allowed, true);
	}

	std::ostringstream ostr;
	std::string::const_iterator it = str.begin();
	std::string::const_iterator end = str.end();
	std::string::value_type c;
	if(is_allowed_sorted)
	{
		std::string::const_iterator allowed_begin(allowed.begin());
		std::string::const_iterator allowed_end(allowed.end());
		for(; it != end; ++it)
		{
			c = *it;
			if(std::binary_search(allowed_begin, allowed_end, c))
			{
				ostr << c;
			}
			else
			{
				encode_character(ostr, c);
			}
		}
	}
	else
	{
		for(; it != end; ++it)
		{
			c = *it;
			if(allowed.find(c) == std::string::npos)
			{
				encode_character(ostr, c);
			}
			else
			{
				ostr << c;
			}
		}
	}
	return ostr.str();
}

// static
std::string LLURI::unescape(const std::string& str)
{
	std::ostringstream ostr;
	std::string::const_iterator it = str.begin();
	std::string::const_iterator end = str.end();
	for(; it != end; ++it)
	{
		if((*it) == '%')
		{
			++it;
			if(it == end) break;
			U8 c = hex_as_nybble(*it++);
			c = c << 4;
			if (it == end) break;
			c |= hex_as_nybble(*it);
			ostr.put((char)c);
		}
		else
		{
			ostr.put(*it);
		}
	}
	return ostr.str();
}

namespace
{
	const std::string unreserved()
	{
		static const std::string s =   
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
			"0123456789"
			"-._~";
		return s;
	}
	const std::string sub_delims()
	{
		static const std::string s = "!$&'()*+,;=";
		return s;
	}

	std::string escapeHostAndPort(const std::string& s)
		{ return LLURI::escape(s, unreserved() + sub_delims() +":"); }
	std::string escapePathComponent(const std::string& s)
		{ return LLURI::escape(s, unreserved() + sub_delims() + ":@"); }
	std::string escapeQueryVariable(const std::string& s)
		{ return LLURI::escape(s, unreserved() + ":@!$'()*+,"); }	 // sub_delims - "&;=" + ":@"
	std::string escapeQueryValue(const std::string& s)
		{ return LLURI::escape(s, unreserved() + ":@!$'()*+,="); }	// sub_delims - "&;" + ":@"
}

// *TODO: Consider using curl. After http textures gets merged everywhere.
// static
std::string LLURI::escape(const std::string& str)
{
	static std::string default_allowed(unreserved() + ":@!$'()*+,=/?&#;");
	static bool initialized = false;
	if(!initialized)
	{
		std::sort(default_allowed.begin(), default_allowed.end());
		initialized = true;
	}
	return escape(str, default_allowed, true);
}

LLURI::LLURI()
{
}

LLURI::LLURI(const std::string& escaped_str)
{
	std::string::size_type delim_pos;
	delim_pos = escaped_str.find(':');
	std::string temp;
	if (delim_pos == std::string::npos)
	{
		mScheme = "";
		mEscapedOpaque = escaped_str;
	}
	else
	{
		mScheme = escaped_str.substr(0, delim_pos);
		mEscapedOpaque = escaped_str.substr(delim_pos+1);
	}

	parseAuthorityAndPathUsingOpaque();

	delim_pos = mEscapedPath.find('?');
	if (delim_pos != std::string::npos)
	{
		mEscapedQuery = mEscapedPath.substr(delim_pos+1);
		mEscapedPath = mEscapedPath.substr(0,delim_pos);
	}
}

static BOOL isDefault(const std::string& scheme, U16 port)
{
	if (scheme == "http")
		return port == 80;
	if (scheme == "https")
		return port == 443;
	if (scheme == "ftp")
		return port == 21;

	return FALSE;
}

void LLURI::parseAuthorityAndPathUsingOpaque()
{
	if (mScheme == "http" || mScheme == "https" ||
		mScheme == "ftp" || mScheme == "secondlife" )
	{
		if (mEscapedOpaque.substr(0,2) != "//")
		{
			return;
		}

		std::string::size_type delim_pos, delim_pos2;
		delim_pos = mEscapedOpaque.find('/', 2);
		delim_pos2 = mEscapedOpaque.find('?', 2);
		// no path, no query
		if (delim_pos == std::string::npos &&
			delim_pos2 == std::string::npos)
		{
			mEscapedAuthority = mEscapedOpaque.substr(2);
			mEscapedPath = "";
		}
		// path exist, no query
		else if (delim_pos2 == std::string::npos)
		{
			mEscapedAuthority = mEscapedOpaque.substr(2,delim_pos-2);
			mEscapedPath = mEscapedOpaque.substr(delim_pos);
		}
		// no path, only query
		else if (delim_pos == std::string::npos ||
				 delim_pos2 < delim_pos)
		{
			mEscapedAuthority = mEscapedOpaque.substr(2,delim_pos2-2);
			// query part will be broken out later
			mEscapedPath = mEscapedOpaque.substr(delim_pos2);
		}
		// path and query
		else
		{
			mEscapedAuthority = mEscapedOpaque.substr(2,delim_pos-2);
			// query part will be broken out later
			mEscapedPath = mEscapedOpaque.substr(delim_pos);
		}
	}
	else if (mScheme == "about")
	{
		mEscapedPath = mEscapedOpaque;
	}
}

LLURI::LLURI(const std::string& scheme,
			 const std::string& userName,
			 const std::string& password,
			 const std::string& hostName,
			 U16 port,
			 const std::string& escapedPath,
			 const std::string& escapedQuery)
	: mScheme(scheme),
	  mEscapedPath(escapedPath),
	  mEscapedQuery(escapedQuery)
{
	std::ostringstream auth;
	std::ostringstream opaque;

	opaque << "//";
	
	if (!userName.empty())
	{
		auth << escape(userName);
		if (!password.empty())
		{
			auth << ':' << escape(password);
		}
		auth << '@';
	}
	auth << hostName;
	if (!isDefault(scheme, port))
	{
		auth << ':' << port;
	}
	mEscapedAuthority = auth.str();

	opaque << mEscapedAuthority << escapedPath << escapedQuery;

	mEscapedOpaque = opaque.str();
}

LLURI::~LLURI()
{
}

// static
LLURI LLURI::buildHTTP(const std::string& prefix,
					   const LLSD& path)
{
	LLURI result;
	
	// TODO: deal with '/' '?' '#' in host_port
	if (prefix.find("://") != prefix.npos)
	{
		// it is a prefix
		result = LLURI(prefix);
	}
	else
	{
		// it is just a host and optional port
		result.mScheme = "http";
		result.mEscapedAuthority = escapeHostAndPort(prefix);
	}

	if (path.isArray())
	{
		// break out and escape each path component
		for (LLSD::array_const_iterator it = path.beginArray();
			 it != path.endArray();
			 ++it)
		{
			lldebugs << "PATH: inserting " << it->asString() << llendl;
			result.mEscapedPath += "/" + escapePathComponent(it->asString());
		}
	}
	else if(path.isString())
	{
		result.mEscapedPath += "/" + escapePathComponent(path.asString());
	} 
	else if(path.isUndefined())
	{
	  // do nothing
	}
    else
	{
	  llwarns << "Valid path arguments to buildHTTP are array, string, or undef, you passed type" 
			  << path.type() << llendl;
	}
	result.mEscapedOpaque = "//" + result.mEscapedAuthority +
		result.mEscapedPath;
	return result;
}

// static
LLURI LLURI::buildHTTP(const std::string& prefix,
					   const LLSD& path,
					   const LLSD& query)
{
	LLURI uri = buildHTTP(prefix, path);
	// break out and escape each query component
	uri.mEscapedQuery = mapToQueryString(query);
	uri.mEscapedOpaque += uri.mEscapedQuery ;
	uri.mEscapedQuery.erase(0,1); // trim the leading '?'
	return uri;
}

// static
LLURI LLURI::buildHTTP(const std::string& host,
					   const U32& port,
					   const LLSD& path)
{
	return LLURI::buildHTTP(llformat("%s:%u", host.c_str(), port), path);
}

// static
LLURI LLURI::buildHTTP(const std::string& host,
					   const U32& port,
					   const LLSD& path,
					   const LLSD& query)
{
	return LLURI::buildHTTP(llformat("%s:%u", host.c_str(), port), path, query);
}

std::string LLURI::asString() const
{
	if (mScheme.empty())
	{
		return mEscapedOpaque;
	}
	else
	{
		return mScheme + ":" + mEscapedOpaque;
	}
}

std::string LLURI::scheme() const
{
	return mScheme;
}

std::string LLURI::opaque() const
{
	return unescape(mEscapedOpaque);
}

std::string LLURI::authority() const
{
	return unescape(mEscapedAuthority);
}


namespace {
	void findAuthorityParts(const std::string& authority,
							std::string& user,
							std::string& host,
							std::string& port)
	{
		std::string::size_type start_pos = authority.find('@');
		if (start_pos == std::string::npos)
		{
			user = "";
			start_pos = 0;
		}
		else
		{
			user = authority.substr(0, start_pos);
			start_pos += 1;
		}

		std::string::size_type end_pos = authority.find(':', start_pos);
		if (end_pos == std::string::npos)
		{
			host = authority.substr(start_pos);
			port = "";
		}
		else
		{
			host = authority.substr(start_pos, end_pos - start_pos);
			port = authority.substr(end_pos + 1);
		}
	}
}
	
std::string LLURI::hostName() const
{
	std::string user, host, port;
	findAuthorityParts(mEscapedAuthority, user, host, port);
	return unescape(host);
}

std::string LLURI::userName() const
{
	std::string user, userPass, host, port;
	findAuthorityParts(mEscapedAuthority, userPass, host, port);
	std::string::size_type pos = userPass.find(':');
	if (pos != std::string::npos)
	{
		user = userPass.substr(0, pos);
	}
	return unescape(user);
}

std::string LLURI::password() const
{
	std::string pass, userPass, host, port;
	findAuthorityParts(mEscapedAuthority, userPass, host, port);
	std::string::size_type pos = userPass.find(':');
	if (pos != std::string::npos)
	{
		pass = userPass.substr(pos + 1);
	}
	return unescape(pass);
}

BOOL LLURI::defaultPort() const
{
	return isDefault(mScheme, hostPort());
}

U16 LLURI::hostPort() const
{
	std::string user, host, port;
	findAuthorityParts(mEscapedAuthority, user, host, port);
	if (port.empty())
	{
		if (mScheme == "http")
			return 80;
		if (mScheme == "https")
			return 443;
		if (mScheme == "ftp")
			return 21;		
		return 0;
	}
	return atoi(port.c_str());
}	

std::string LLURI::path() const
{
	return unescape(mEscapedPath);
}

LLSD LLURI::pathArray() const
{
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("/", "", boost::drop_empty_tokens);
	tokenizer tokens(mEscapedPath, sep);
	tokenizer::iterator it = tokens.begin();
	tokenizer::iterator end = tokens.end();

	LLSD params;
	for ( ; it != end; ++it)
	{
		params.append(*it);
	}
	return params;
}

std::string LLURI::query() const
{
	return unescape(mEscapedQuery);
}

LLSD LLURI::queryMap() const
{
	return queryMap(mEscapedQuery);
}

// static
LLSD LLURI::queryMap(std::string escaped_query_string)
{
	lldebugs << "LLURI::queryMap query params: " << escaped_query_string << llendl;

	LLSD result = LLSD::emptyArray();
	while(!escaped_query_string.empty())
	{
		// get tuple first
		std::string tuple;
		std::string::size_type tuple_begin = escaped_query_string.find('&');
		if (tuple_begin != std::string::npos)
		{
			tuple = escaped_query_string.substr(0, tuple_begin);
			escaped_query_string = escaped_query_string.substr(tuple_begin+1);
		}
		else
		{
			tuple = escaped_query_string;
			escaped_query_string = "";
		}
		if (tuple.empty()) continue;

		// parse tuple
		std::string::size_type key_end = tuple.find('=');
		if (key_end != std::string::npos)
		{
			std::string key = unescape(tuple.substr(0,key_end));
			std::string value = unescape(tuple.substr(key_end+1));
			lldebugs << "inserting key " << key << " value " << value << llendl;
			result[key] = value;
		}
		else
		{
			lldebugs << "inserting key " << unescape(tuple) << " value true" << llendl;
		    result[unescape(tuple)] = true;
		}
	}
	return result;
}

std::string LLURI::mapToQueryString(const LLSD& queryMap)
{
	std::string query_string;
	if (queryMap.isMap())
	{
		bool first_element = true;
		LLSD::map_const_iterator iter = queryMap.beginMap();
		LLSD::map_const_iterator end = queryMap.endMap();
		std::ostringstream ostr;
		for (; iter != end; ++iter)
		{
			if(first_element)
			{
				ostr << "?";
				first_element = false;
			}
			else
			{
				ostr << "&";
			}
			ostr << escapeQueryVariable(iter->first);
			if(iter->second.isDefined())
			{
				ostr << "=" <<  escapeQueryValue(iter->second.asString());
			}
		}
		query_string = ostr.str();
	}
	return query_string;
}
