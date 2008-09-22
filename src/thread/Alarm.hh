// $Id$

#ifndef ALARM_HH
#define ALARM_HH

#include "noncopyable.hh"

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

private:
	/** This method gets called when the alarm timer expires.
	  * @see schedule()
	  * @result true iff alarm should be periodic
	  */
	virtual bool alarm() = 0;

	AlarmManager& manager;
	long long time;
	unsigned period;
	bool active;

	friend class AlarmManager;
};

} // namespace openmsx

#endif
