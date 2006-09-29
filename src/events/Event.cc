// $Id$

#include "Event.hh"
#include "TclObject.hh"
#include <cassert>

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
	return result.getString();
}

bool Event::operator< (const Event& other) const
{
	return lessImpl(other);
}

bool Event::operator==(const Event& other) const
{
	return !(*this < other) && !(other < *this);
}

void Event::toStringImpl(TclObject& /*result*/) const
{
	assert(false);
}

bool Event::lessImpl(const Event& /*other*/) const
{
	assert(false);
	return false; // avoid warning
}

} // namespace openmsx
