// $Id$

#ifndef STATECHANGE_HH
#define STATECHANGE_HH

#include "EmuTime.hh"
#include "noncopyable.hh"

namespace openmsx {

/** Base class for all external MSX state changing events.
 * These are typically triggered by user input, like keyboard presses. The main
 * reason why these events exist is to be able to record and replay them.
 */
class StateChange : private noncopyable
{
public:
	virtual ~StateChange() {}
	EmuTime::param getTime() const
	{
		return time;
	}

protected:
	StateChange(EmuTime::param time_)
		: time(time_)
	{
	}

private:
	EmuTime time;
};

} // namespace openmsx

#endif
