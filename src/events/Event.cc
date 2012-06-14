// $Id$

#include "Event.hh"
#include "TclObject.hh"
#include "unreachable.hh"

namespace openmsx {

// class Event

Event::Event(EventType type_)
	: type(type_)
{
}

Event::~Event()
{
}

EventType Event::getType() const
{
	return type;
}

std::string Event::toString() const
{
	TclObject result;
	toStringImpl(result);
	return result.getString().str();
}

bool Event::operator<(const Event& other) const
{
	return (getType() != other.getType())
	     ? (getType() <  other.getType())
	     : lessImpl(other);
}

bool Event::operator==(const Event& other) const
{
	return !(*this < other) && !(other < *this);
}
bool Event::operator!=(const Event& other) const
{
	return !(*this == other);
}

void Event::toStringImpl(TclObject& /*result*/) const
{
	UNREACHABLE;
}

bool Event::lessImpl(const Event& /*other*/) const
{
	UNREACHABLE; return false;
}

} // namespace openmsx
