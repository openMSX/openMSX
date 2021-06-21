#include "Event.hh"
#include "TclObject.hh"

namespace openmsx {

// class Event

std::string Event::toString() const
{
	return std::string(toTclList().getString());
}

bool Event::operator==(const Event& other) const
{
	return (getType() == other.getType()) && equalImpl(other);
}
bool Event::operator!=(const Event& other) const
{
	return !(*this == other);
}

TclObject SimpleEvent::toTclList() const
{
	return makeTclList("simple", int(getType()));
}

bool SimpleEvent::equalImpl(const Event& /*other*/) const
{
	return true;
}

} // namespace openmsx
