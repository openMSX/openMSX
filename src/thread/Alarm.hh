// $Id$

#ifndef ALARM_HH
#define ALARM_HH

#include "Semaphore.hh"
#include "noncopyable.hh"

namespace openmsx {

class Alarm : private noncopyable
{
public:
	void schedule(unsigned us);
	void cancel();
	bool pending() const;

protected:
	Alarm();
	virtual ~Alarm();

private:
	/** Callback function
	  * @result true iff alarm should be periodic
	  */
	virtual bool alarm() = 0;

	void do_cancel();
	static unsigned helper(unsigned interval, void* param);

	void* id;
	mutable Semaphore sem;
};

} // namespace openmsx

#endif
