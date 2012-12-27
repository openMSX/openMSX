// $Id$

#ifndef ALARM_HH
#define ALARM_HH

#include "noncopyable.hh"
#include <cstdint>

namespace openmsx {

class AlarmManager;

class Alarm : private noncopyable
{
public:
	/** Arrange for the alarm() method to be called after some time.
	 * @param period Duration of the time in microseconds (us).
	 */
	void schedule(unsigned period);

	/** Cancel a previous schedule() request.
	 * It's ok to call cancel(), when there is no pending alarm.
	 */
	void cancel();

	/** Is there a pending alarm?
	 */
	bool pending() const;

protected:
	Alarm();
	virtual ~Alarm();

	/** Concrete subclasses MUST call this method in their destructor.
	  * This makes sure the timer thread is not executing the alarm()
	  * method (or will not execute it while this object is being
	  * destroyed).
	  */
	void prepareDelete();

private:
	/** This method gets called when the alarm timer expires.
	  * Note: This method executes in the timer thread, _NOT_ in the main
	  *       thread!!! Consider using the AlarmEvent class if you need
	  *       a callback in the main thread.
	  * @see schedule()
	  * @result true iff alarm should be periodic
	  */
	virtual bool alarm() = 0;

	AlarmManager& manager;
	int64_t time;
	unsigned period;
	bool active;
	bool destructing; // only for debugging

	friend class AlarmManager;
};

} // namespace openmsx

#endif
