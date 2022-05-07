#ifndef STATECHANGE_HH
#define STATECHANGE_HH

#include "EmuTime.hh"
#include "serialize_meta.hh"

namespace openmsx {

// TODO:
//  See commit '2b0c6cf467d510' for how to turn this class hierarchy into
//  std::variant. That saves a lot of heap-memory allocations.  Though we
//  reverted that commit because it triggered internal compiler errors in msvc.
//  In the future, when msvc gets fixed, we can try again.

/** Base class for all external MSX state changing events.
 * These are typically triggered by user input, like keyboard presses. The main
 * reason why these events exist is to be able to record and replay them.
 */
class StateChange
{
public:
	virtual ~StateChange() = default; // must be polymorphic

	[[nodiscard]] EmuTime::param getTime() const
	{
		return time;
	}

	void serialize(Archive auto& ar, unsigned /*version*/)
	{
		ar.serialize("time", time);
	}

protected:
	StateChange() : time(EmuTime::zero()) {} // for serialize
	explicit StateChange(EmuTime::param time_)
		: time(time_)
	{
	}

private:
	EmuTime time;
};
REGISTER_BASE_CLASS(StateChange, "StateChange");

} // namespace openmsx

#endif
