#ifndef STATECHANGE_HH
#define STATECHANGE_HH

#include "EmuTime.hh"
#include "serialize_meta.hh"

namespace openmsx {

/** Base class for all external MSX state changing events.
 * These are typically triggered by user input, like keyboard presses. The main
 * reason why these events exist is to be able to record and replay them.
 */
class StateChange
{
public:
	virtual ~StateChange() {} // must be polymorhpic

	EmuTime::param getTime() const
	{
		return time;
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("time", time);
	}

protected:
	StateChange() : time(EmuTime::zero) {} // for serialize
	StateChange(EmuTime::param time_)
		: time(time_)
	{
	}

private:
	EmuTime time;
};
REGISTER_BASE_CLASS(StateChange, "StateChange");

} // namespace openmsx

#endif
