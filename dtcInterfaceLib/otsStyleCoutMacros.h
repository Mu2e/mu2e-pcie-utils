#ifndef _ots_Style_Cout_Macros_H
#define _ots_Style_Cout_Macros_H

// clang-format off

#include <string.h>  //for strstr (not the same as <string>)
#include <iostream>  //for cout
#include <sstream>   //for std::stringstream, std::stringbuf
#include <iomanip>   //for std::setfill, std::setw

#define TRACEMF_USE_VERBATIM 1 //for trace longer path filenames
#include "TRACE/tracemf.h"

#define __COUT_HDR__ 		""

#define __FILENAME__ 		(__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#define __MF_SUBJECT__ 		__FILENAME__
#define __MF_DECOR__		(__MF_SUBJECT__)
#define __SHORTFILE__ 		(__builtin_strstr(&__FILE__[0], "/srcs/") ? __builtin_strstr(&__FILE__[0], "/srcs/") + 6 : __FILE__)
#define __COUT_HDR_L__ 		":" << std::dec        << __LINE__ << " |\t"
#define __COUT_HDR_FL__ 	__SHORTFILE__ << ""    << __COUT_HDR_L__
#define __COUT_ERR__ 		TLOG(TLVL_ERROR)       << __COUT_HDR__
#define __COUT_WARN__ 		TLOG(TLVL_WARN)        << __COUT_HDR__
#define __COUT_INFO__ 		TLOG(TLVL_INFO)        << __COUT_HDR__
#define __COUT__			TLOG(TLVL_DEBUG)       << __COUT_HDR__
#define __COUTT__			TLOG(TLVL_TRACE)       << __COUT_HDR__
#define __COUT_TYPE__(LVL) 	TLOG(TLVL_DEBUG + LVL) << __COUT_HDR__
#define __COUTS__(LVL)		__COUT_TYPE__(LVL)
//std::cout << __MF_DECOR__ << __COUT_HDR_FL__

#define __THROW__(X) 		throw std::runtime_error(X)

#define __SS__            	std::stringstream ss; ss << "|" << __MF_DECOR__ << ": " << __COUT_HDR_FL__ << __COUT_HDR__
#define __SS_THROW__        { __COUT_ERR__ << "\n" << ss.str(); throw std::runtime_error(ss.str()); } //put in {}'s to prevent surprises, e.g. if ... else __SS_THROW__;
#define __SS_ONLY_THROW__   { throw std::runtime_error(ss.str()); } //put in {}'s to prevent surprises, e.g. if ... else __SS_THROW__;
#define __E__ 				std::endl
#define Q(X) #X
#define QUOTE(X) Q(X)
#define __COUTV__(X) 		__COUT__ << QUOTE(X)   << " = " << X << __E__
#define __COUTTV__(X) 		__COUTT__ << QUOTE(X)  << " = " << X << __E__
#define __COUTVS__(LVL,X)	TLOG(TLVL_DEBUG + LVL) << __COUT_HDR__ << QUOTE(X) << " = " << X << __E__

std::string 			otsStyleStackTrace					(void);

// clang-format on

#endif  // _ots_Style_Cout_Macros_H
