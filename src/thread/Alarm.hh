// $Id$

#ifndef ALARM_HH
#define ALARM_HH

#include "Semaphore.hh"

namespace openmsx {

class Alarm
{
public:
	void schedule(unsigned interval);
	void cancel();
	bool pending() const;

protected:
	Alarm();
	virtual ~Alarm();

private:
	virtual void alarm() = 0;

	void do_cancel();
	static unsigned helper(unsigned interval, void* param);

	void* id;
	mutable Semaphore sem;
};

} // namespace openmsx

#endif
