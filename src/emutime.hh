// $Id$
//
// Emutime class for usage when referencing to the absolute emulation time
//
// usage:
//
//   Emutime time(0);
//   time++;
//   time+=100;
//

#ifndef __Emutime_HH__
#define __Emutime_HH__

#ifndef uint64
	typedef long int uint64;
#endif

// predefines for friend functions
class Emutime;
uint64 operator +(const uint64 &foo, const Emutime &bar);
uint64 operator -(const uint64 &foo, const Emutime &bar);

class Emutime
{
friend uint64 operator +(const uint64 &foo, const Emutime &bar);
friend uint64 operator -(const uint64 &foo, const Emutime &bar);
public:
	// ordinary ctor/dtor
	Emutime():_emutime(0) {}
	Emutime(int val):_emutime(val) {} // uint64 -> Emutime conversion operator
	~Emutime(void) {} // if inheritance from Emutime is needed, change to virtual

	// copy & assignment operator
	Emutime(const Emutime &foo):_emutime(foo._emutime) {}
	Emutime &operator =(const Emutime &foo) { _emutime=foo._emutime; }

	// conversion operator 
	// [note: conversion operators have an implicit return type]
	operator uint64() const { return _emutime; }
	// since uint64 is a typedef does this work? Aparantly.

	// basicaly needed operators
	Emutime &operator +(const uint64 &foo) { _emutime+=foo ; return *this; }
	Emutime &operator +=(const uint64 &foo) { _emutime+=foo ; return *this; }
	Emutime &operator +(const Emutime &foo) { _emutime+=foo._emutime ; return *this; }
	Emutime &operator +=(const Emutime &foo) { _emutime+=foo._emutime ; return *this; }
	Emutime &operator -(const uint64 &foo) { _emutime-=foo ; return *this; }
	Emutime &operator -=(const uint64 &foo) { _emutime-=foo ; return *this; }
	Emutime &operator -(const Emutime &foo) { _emutime-=foo._emutime ; return *this; }
	Emutime &operator -=(const Emutime &foo) { _emutime-=foo._emutime ; return *this; }
	Emutime &operator ++() { _emutime++ ; return *this; } // prefix
	Emutime &operator --() { _emutime-- ; return *this; }
	Emutime &operator ++(int unused) { _emutime++ ; return *this; } // postfix
	Emutime &operator --(int unused) { _emutime-- ; return *this; }
private:
	uint64 _emutime;
};

inline uint64 operator +(const uint64 &foo, const Emutime &bar)
{
	return foo+bar._emutime;
}

inline uint64 operator -(const uint64 &foo, const Emutime &bar)
{
	return foo-bar._emutime;
}

#endif
