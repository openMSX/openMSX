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

#include "assert.h"


#ifndef uint64
	typedef long long uint64;
	// this is not portable to 64bit platforms? -> TODO check
#endif

class Emutime
{
public:
	// constructors
	Emutime() : _emutime(0), _scale(0) {}
	Emutime(int freq, int val=0): _scale(_mainFreq/freq)
	{
		assert((_mainFreq%freq)==0);
		_emutime = val*_scale;
	}
	
	// destructor
	~Emutime() {}

	// copy constructor
	Emutime(const Emutime &foo):_emutime(foo._emutime),_scale(foo._scale) {}
	
	// assignment operator
	Emutime &operator =(const Emutime &foo) { _emutime=foo._emutime; return *this; }

	// arithmetic operators
	Emutime &operator +=(const uint64 &foo) { _emutime+=foo*_scale ; return *this; }
	Emutime &operator +=(const Emutime &foo) { _emutime+=foo._emutime ; return *this; }
	Emutime &operator -=(const uint64 &foo) { _emutime-=foo*_scale ; return *this; }
	Emutime &operator -=(const Emutime &foo) { _emutime-=foo._emutime ; return *this; }
	Emutime &operator ++() { _emutime+=_scale ; return *this; } // prefix
	Emutime &operator --() { _emutime-=_scale ; return *this; }
	Emutime &operator ++(int unused) { _emutime+=_scale ; return *this; } // postfix
	Emutime &operator --(int unused) { _emutime-=_scale ; return *this; }
	
	// comparison operators
	bool operator ==(const Emutime &foo) const { return _emutime == foo._emutime; }
	bool operator < (const Emutime &foo) const { return _emutime <  foo._emutime; }
	bool operator <=(const Emutime &foo) const { return _emutime <= foo._emutime; }
	bool operator > (const Emutime &foo) const { return _emutime >  foo._emutime; }
	bool operator >=(const Emutime &foo) const { return _emutime >= foo._emutime; }


private:
	static const uint64 _mainFreq=3579545*8;
	
	uint64 _emutime;
	const int _scale;
};

#endif
