#include "Event.hh"
#include "TclObject.hh"

namespace openmsx {

// class Event

std::string Event::toString() const
{
	return std::string(toTclList().getString());
}

bool Event::operator<(const Event& other) const
{
	return (getType() != other.getType())
	     ? (getType() <  other.getType())
	     : lessImpl(other);
}
bool Event::operator>(const Event& other) const
{
	return other < *this;
}
bool Event::operator<=(const Event& other) const
{
	return !(other < *this);
}
bool Event::operator>=(const Event& other) const
{
	return !(*this < other);
}

bool Event::operator==(const Event& other) const
{
	return !(*this < other) && !(other < *this);
}
bool Event::operator!=(const Event& other) const
{
	return !(*this == other);
}

TclObject SimpleEvent::toTclList() const
{
	return makeTclList("simple", int(getType()));
}

bool SimpleEvent::lessImpl(const Event& /*other*/) const
{
	return false;
}

} // namespace openmsx
