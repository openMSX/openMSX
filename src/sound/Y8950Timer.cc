// $Id$

#include "Y8950Timer.hh"
#include "Y8950.hh"
#include "Scheduler.hh"


template<int freq, byte flag>
Y8950Timer<freq, flag>::Y8950Timer(Y8950* y8950_)
	: y8950(y8950_)
{
	scheduler = Scheduler::instance();
}

template<int freq, byte flag>
Y8950Timer<freq, flag>::~Y8950Timer()
{
	unschedule();
}

template<int freq, byte flag>
void Y8950Timer<freq, flag>::setValue(byte value)
{
	count = 256 - value;
}

template<int freq, byte flag>
void Y8950Timer<freq, flag>::setStart(bool start, const EmuTime &time)
{
	if (start != counting) {
		counting = start;
		if (start) {
			schedule(time);
		} else {
			unschedule();
		}
	}
}

template<int freq, byte flag>
void Y8950Timer<freq, flag>::schedule(const EmuTime &time)
{
	EmuTimeFreq<freq> now(time);
	scheduler->setSyncPoint(now + count, this);
}

template<int freq, byte flag>
void Y8950Timer<freq, flag>::unschedule()
{
	scheduler->removeSyncPoint(this);
}

template<int freq, byte flag>
void Y8950Timer<freq, flag>::executeUntilEmuTime(const EmuTime &time, int userData)
{
	y8950->setStatus(flag);
	schedule(time);
}
