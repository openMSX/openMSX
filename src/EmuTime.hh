// $Id$
//
// EmuTime class for usage when referencing to the absolute emulation time
//
// usage:
//
//   EmuTime time(3579545);
//   time++;
//   time+=100;
//

#ifndef __EMUTIME_HH__
#define __EMUTIME_HH__

#include <cassert>
#include <climits>
#include <iostream>

#ifndef uint64
	typedef long long uint64;
	// this is not portable to 64bit platforms? -> TODO check
#endif

// predefines
class EmuTime;
std::ostream &operator<<(std::ostream &os, const EmuTime &et);

class EmuTime
{
public:
	friend std::ostream &operator<<(std::ostream &os, const EmuTime &et);
	// constants
	static const uint64 MAIN_FREQ = 3579545*24;
	static const uint64 INFINITY = 18446744073709551615ULL;	//ULLONG_MAX;

	// constructors
	EmuTime(int freq=DUMMY_FREQ, int val=0): _scale(MAIN_FREQ/freq)
	{
		//assert((MAIN_FREQ%freq)==0); cannot check because of rounding errors
		_EmuTime = val*_scale;
	}
	
	// destructor
	~EmuTime() {}

	// copy constructor
	EmuTime(const EmuTime &foo):_EmuTime(foo._EmuTime),_scale(foo._scale) {}
	
	// assignment operator
	EmuTime &operator =(const EmuTime &foo) { _EmuTime=foo._EmuTime; return *this; }
	void operator() (const uint64 &foo) {_EmuTime=foo*_scale; }

	// arithmetic operators
	EmuTime &operator +=(const uint64 &foo) 
		{ assert(_scale!=0); _EmuTime+=foo*_scale ; return *this; }
	EmuTime &operator +=(const EmuTime &foo) 
		{ assert(_scale!=0); _EmuTime+=foo._EmuTime ; return *this; }
	EmuTime &operator -=(const uint64 &foo) 
		{ assert(_scale!=0); _EmuTime-=foo*_scale ; return *this; }
	EmuTime &operator -=(const EmuTime &foo) 
		{ assert(_scale!=0); _EmuTime-=foo._EmuTime ; return *this; }
	EmuTime &operator ++() 
		{ assert(_scale!=0); _EmuTime+=_scale ; return *this; } // prefix
	EmuTime &operator --() 
		{ assert(_scale!=0); _EmuTime-=_scale ; return *this; }
	EmuTime &operator ++(int unused) 
		{ assert(_scale!=0); _EmuTime+=_scale ; return *this; } // postfix
	EmuTime &operator --(int unused) 
		{ assert(_scale!=0); _EmuTime-=_scale ; return *this; }

	EmuTime &operator +(const uint64 &foo) const {EmuTime *bar = new EmuTime(*this); *bar+=foo; return *bar; }
	EmuTime &operator +(const EmuTime &foo) const {EmuTime *bar = new EmuTime(*this); *bar+=foo; return *bar; }

	// comparison operators
	bool operator ==(const EmuTime &foo) const { return _EmuTime == foo._EmuTime; }
	bool operator < (const EmuTime &foo) const { return _EmuTime <  foo._EmuTime; }
	bool operator <=(const EmuTime &foo) const { return _EmuTime <= foo._EmuTime; }
	bool operator > (const EmuTime &foo) const { return _EmuTime >  foo._EmuTime; }
	bool operator >=(const EmuTime &foo) const { return _EmuTime >= foo._EmuTime; }

	// distance functions
	uint64 getTicksTill(const EmuTime &foo) const {
		assert (_scale!=0);
		assert (foo._EmuTime >= _EmuTime);
		return (foo._EmuTime-_EmuTime)/_scale;
	}
	float getDuration(const EmuTime &foo) const {
		return (float)(foo._EmuTime-_EmuTime)/MAIN_FREQ;
	}

private:
	static const int DUMMY_FREQ = MAIN_FREQ*2;	// as long as it's greater than MAIN_FREQ
	
	uint64 _EmuTime;
	const int _scale;
};

#endif
