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

#include <assert.h>
#include <limits.h>


#define MAIN_FREQ  3579545*8
#define DUMMY_FREQ 3579545*99	// as long as it's greater than MAIN_FREQ
#define INFINITY 18446744073709551615ULL	// 2^64 - 1



#ifndef uint64
	typedef long long uint64;
	// this is not portable to 64bit platforms? -> TODO check
#endif

class Emutime
{
public:
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

	Emutime &operator +(const uint64 &foo) {Emutime *bar = new Emutime(*this); *bar+=foo; return *bar; }
	Emutime &operator +(const Emutime &foo) {Emutime *bar = new Emutime(*this); *bar+=foo; return *bar; }

	// comparison operators
	bool operator ==(const Emutime &foo) const { return _emutime == foo._emutime; }
	bool operator < (const Emutime &foo) const { return _emutime <  foo._emutime; }
	bool operator <=(const Emutime &foo) const { return _emutime <= foo._emutime; }
	bool operator > (const Emutime &foo) const { return _emutime >  foo._emutime; }
	bool operator >=(const Emutime &foo) const { return _emutime >= foo._emutime; }

	// distance function
	uint64 getTicksTill(const Emutime &foo) const {
		assert (_scale!=0);
		assert (foo._emutime >= _emutime);
		return (int)(foo._emutime-_emutime)/_scale;
	}

private:
	static const uint64 _mainFreq=MAIN_FREQ;
	
	uint64 _emutime;
	const int _scale;
};

#endif
