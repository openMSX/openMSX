// $Id$
//
// Emutime class for usage when referencing to the absolute emulation time
//
// usage:
//
//   Emutime time(3579545);
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
class Emutime;
std::ostream &operator<<(std::ostream &os, const Emutime &et);

class Emutime
{
public:
	friend std::ostream &operator<<(std::ostream &os, const Emutime &et);
	// constants
	static const uint64 MAIN_FREQ = 3579545*8;
	static const uint64 INFINITY = 18446744073709551615ULL;	//ULLONG_MAX;

	// constructors
	Emutime(int freq=DUMMY_FREQ, int val=0): _scale(MAIN_FREQ/freq)
	{
		//assert((MAIN_FREQ%freq)==0); cannot check because of rounding errors
		_emutime = val*_scale;
	}
	
	// destructor
	~Emutime() {}

	// copy constructor
	Emutime(const Emutime &foo):_emutime(foo._emutime),_scale(foo._scale) {}
	
	// assignment operator
	Emutime &operator =(const Emutime &foo) { _emutime=foo._emutime; return *this; }
	void operator() (const uint64 &foo) {_emutime=foo*_scale; }

	// arithmetic operators
	Emutime &operator +=(const uint64 &foo) 
		{ assert(_scale!=0); _emutime+=foo*_scale ; return *this; }
	Emutime &operator +=(const Emutime &foo) 
		{ assert(_scale!=0); _emutime+=foo._emutime ; return *this; }
	Emutime &operator -=(const uint64 &foo) 
		{ assert(_scale!=0); _emutime-=foo*_scale ; return *this; }
	Emutime &operator -=(const Emutime &foo) 
		{ assert(_scale!=0); _emutime-=foo._emutime ; return *this; }
	Emutime &operator ++() 
		{ assert(_scale!=0); _emutime+=_scale ; return *this; } // prefix
	Emutime &operator --() 
		{ assert(_scale!=0); _emutime-=_scale ; return *this; }
	Emutime &operator ++(int unused) 
		{ assert(_scale!=0); _emutime+=_scale ; return *this; } // postfix
	Emutime &operator --(int unused) 
		{ assert(_scale!=0); _emutime-=_scale ; return *this; }

	Emutime &operator +(const uint64 &foo) const {Emutime *bar = new Emutime(*this); *bar+=foo; return *bar; }
	Emutime &operator +(const Emutime &foo) const {Emutime *bar = new Emutime(*this); *bar+=foo; return *bar; }

	// comparison operators
	bool operator ==(const Emutime &foo) const { return _emutime == foo._emutime; }
	bool operator < (const Emutime &foo) const { return _emutime <  foo._emutime; }
	bool operator <=(const Emutime &foo) const { return _emutime <= foo._emutime; }
	bool operator > (const Emutime &foo) const { return _emutime >  foo._emutime; }
	bool operator >=(const Emutime &foo) const { return _emutime >= foo._emutime; }

	// distance functions
	uint64 getTicksTill(const Emutime &foo) const {
		assert (_scale!=0);
		assert (foo._emutime >= _emutime);
		return (foo._emutime-_emutime)/_scale;
	}
	float getDuration(const Emutime &foo) const {
		return (float)(foo._emutime-_emutime)/MAIN_FREQ;
	}

private:
	uint64 _emutime; 
	static const int DUMMY_FREQ = MAIN_FREQ*2;	// as long as it's greater than MAIN_FREQ
	
	const int _scale;
};

#endif
