// $Id$

#ifndef __SCHEDULABLE_HH__
#define __SCHEDULABLE_HH__

#include <string>

// forward declarations

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
		virtual void executeUntilEmuTime(const EmuTime &time, int userData) = 0;

#ifdef DEBUG
		/**
		 * This method is only used to print meaningfull debug messages
		 */
		virtual const std::string &getName();

	protected:
		static const std::string defaultName;
#endif
};

#endif
