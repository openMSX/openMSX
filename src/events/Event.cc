// $Id$

#include "Event.hh"
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
	assert(false);
}

bool Event::operator< (const Event& other) const
{
	return lessImpl(other);
}

bool Event::operator==(const Event& other) const
{
	return !(*this < other) && !(other < *this);
}

bool Event::lessImpl(const Event& /*other*/) const
{
	assert(false);
}

} // namespace openmsx
