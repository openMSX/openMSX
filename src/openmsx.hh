// $Id$

#ifndef __OPENMSX_HH__
#define __OPENMSX_HH__

using namespace std;

/** 4 bit integer */
typedef unsigned char nibble;

/** 8 bit signed integer */
typedef char signed_byte;
/** 8 bit unsigned integer */
typedef unsigned char byte;

/** 16 bit signed integer */
typedef short signed_word;
/** 16 bit unsigned integer */
typedef unsigned short word;

/** 32 bit unsigned integer */
typedef unsigned int dword;

#ifdef __WIN32__
/** 64 bit signed integer */
typedef __int64 int64;
/** 64 bit unsigned integer */
typedef unsigned __int64 uint64;
#else
// this is not portable to 64bit platforms? -> TODO check
/** 64 bit signed integer */
typedef long long int64;
// this is not portable to 64bit platforms? -> TODO check
/** 64 bit unsigned integer */
typedef unsigned long long uint64;
#endif

#ifdef DEBUGVAL
#undef DEBUGVAL
#endif
#ifdef DEBUG
#define DEBUGVAL 1
#else
#define DEBUGVAL 0
#endif
#include <iostream>
#include "Mutex.hh"
extern Mutex outputmutex, errormutex;
#define PRT_DEBUG(mes)					\
	do {						\
		if (DEBUGVAL) {				\
			outputmutex.grab();		\
			cout << mes << endl;	\
			cout.flush();	\
			outputmutex.release();		\
		}					\
	} while (0)
#define PRT_INFO(mes)				\
	do {					\
		outputmutex.grab();		\
		cout << mes << endl;	\
		cout.flush();	\
		outputmutex.release();		\
	} while (0)
#define PRT_ERROR(mes)				\
	do {					\
		errormutex.grab();		\
		cerr << mes << endl;	\
		cerr.flush();	\
		errormutex.release();		\
		exit(1);			\
	} while (0)

#endif //__OPENMSX_HH__
