// $Id$

#ifndef SCHEDULABLE_HH
#define SCHEDULABLE_HH

#include <string>

namespace openmsx {

class EmuTime;
class Scheduler;

/**
 * Every class that wants to get scheduled at some point must inherit from
 * this class.
 */
class Schedulable
{
public:
	Schedulable();

	/**
	 * When the previously registered syncPoint is reached, this
	 * method gets called. The parameter "userData" is the same
	 * as passed to setSyncPoint().
	 */
	virtual void executeUntil(const EmuTime& time, int userData) = 0;

	/**
	 * This method is only used to print meaningfull debug messages
	 */
	virtual const std::string& schedName() const = 0;

protected:
	virtual ~Schedulable();

	void setSyncPoint(const EmuTime& timestamp, int userData = 0);
	void removeSyncPoint(int userData = 0);
	bool pendingSyncPoint(int userData = 0);

private:
	Scheduler& scheduler;
};

} // namespace openmsx

#endif
