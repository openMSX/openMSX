// $Id$

#ifndef __OPENMSX_HH__
#define __OPENMSX_HH__

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

#ifdef DEBUG
#define PRT_DEBUG(mes) {std::cout << mes << "\n"; }
#else
#define PRT_DEBUG(mes) {}
#endif

#define PRT_INFO(mes) {std::cout << mes << "\n"; }
#define PRT_ERROR(mes) {std::cout << mes << "\n"; exit(1); }

//#ifndef DEBUG
//#define NDEBUG		// for assert.h
//#endif

#endif //__OPENMSX_HH__

