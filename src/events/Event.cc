#include "Event.hh"
#include "TclObject.hh"

namespace openmsx {

// class Event

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

void SimpleEvent::toStringImpl(TclObject& result) const
{
	result.addListElement("simple");
	result.addListElement(int(getType()));
}

bool SimpleEvent::lessImpl(const Event& /*other*/) const
{
	return false;
}

} // namespace openmsx
