// $Id$

#ifndef __OPENMSX_HH__
#define __OPENMSX_HH__

typedef unsigned char nibble;	// 4 bit
typedef unsigned char byte;	// 8 bit
typedef unsigned short word;	// 16 bit 

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

