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

#ifndef __EMUTIME_HH__
#define __EMUTIME_HH__

#ifndef uint64
	typedef long int uint64;
	// this is not portable to 64bit platforms? -> TODO check
#endif

class Emutime
{
friend Emutime &operator +(const uint64 &foo, Emutime &bar);
friend Emutime &operator -(const uint64 &foo, Emutime &bar);
public:
	// ordinary ctor/dtor
	// uint64 -> Emutime conversion operator
	Emutime(int val=0, int freq=350000):_emutime(val), _scale(_scalefactor/freq) {}
	~Emutime(void) {}
	// if inheritance from Emutime is needed, change to virtual, but please don't!

	// copy & assignment operator
	Emutime(const Emutime &foo):_emutime(foo._emutime) {}
	Emutime &operator =(const Emutime &foo) { _emutime=foo._emutime; }

	// conversion operator 
	// [note: conversion operators have an implicit return type]
	// operator uint64() const { return _emutime; }
	// disabled, we want uint64 -> Emutime to be a onetime conversion
	// since uint64 is a typedef does this work? Aparantly.

	// basicaly needed operators
	Emutime &operator +(const uint64 &foo) { _emutime+=foo*_scale ; return *this; }
	Emutime &operator +=(const uint64 &foo) { _emutime+=foo*_scale ; return *this; }
	Emutime &operator +(const Emutime &foo) { _emutime+=foo._emutime ; return *this; }
	Emutime &operator +=(const Emutime &foo) { _emutime+=foo._emutime ; return *this; }
	Emutime &operator -(const uint64 &foo) { _emutime-=foo*_scale ; return *this; }
	Emutime &operator -=(const uint64 &foo) { _emutime-=foo*_scale ; return *this; }
	Emutime &operator -(const Emutime &foo) { _emutime-=foo._emutime ; return *this; }
	Emutime &operator -=(const Emutime &foo) { _emutime-=foo._emutime ; return *this; }
	Emutime &operator ++() { _emutime+=_scale ; return *this; } // prefix
	Emutime &operator --() { _emutime-=_scale ; return *this; }
	Emutime &operator ++(int unused) { _emutime+=_scale ; return *this; } // postfix
	Emutime &operator --(int unused) { _emutime-=_scale ; return *this; }
private:
	uint64 _emutime;
	static const uint64 _scalefactor=28000000;
	const int _scale;
};

inline Emutime &operator +(const uint64 &foo, Emutime &bar)
{
	return (bar+=foo);
}

inline Emutime &operator -(const uint64 &foo, Emutime &bar)
{
	bar._emutime=foo-bar._emutime;
	return bar;
}

#endif
