// $Id$

#ifndef __TIMER_HH__
#define __TIMER_HH__

#include "Schedulable.hh"
#include "openmsx.hh"


namespace openmsx {

class Scheduler;

class EmuTimerCallback
{
public:
	virtual ~EmuTimerCallback() {}
	virtual void callback(byte value) throw() = 0;
};

template<int freq, byte flag>
class EmuTimer : private Schedulable
{
public:
	EmuTimer(EmuTimerCallback* cb);
	virtual ~EmuTimer();
	void setValue(byte value);
	void setStart(bool start, const EmuTime& time);

private:
	virtual void executeUntil(const EmuTime& time, int userData) throw();
	virtual const string& schedName() const;
	void schedule(const EmuTime& time);
	void unschedule();

	int count;
	bool counting;
	EmuTimerCallback* cb;
	Scheduler& scheduler;
};

} // namespace openmsx

#endif // __TIMER_HH__
