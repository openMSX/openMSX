// $Id$

#ifndef __SCHEDULABLE_HH__
#define __SCHEDULABLE_HH__

#include <string>

using std::string;

namespace openmsx {

class EmuTime;

/**
 * Every class that wants to get scheduled at some point must inherit from
 * this class
 */
class Schedulable
{
public:
	/**
	 * When the previously registered syncPoint is reached, this
	 * method gets called. The parameter "userData" is the same
	 * as passed to setSyncPoint().
	 */
	virtual void executeUntil(const EmuTime& time, int userData) = 0;

	/**
	 * This method is only used to print meaningfull debug messages
	 */
	virtual const string& schedName() const = 0;
};

} // namespace openmsx

#endif
