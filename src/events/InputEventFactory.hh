// $Id$

#ifndef INPUTEVENTFACTORY_HH
#define INPUTEVENTFACTORY_HH

#include "shared_ptr.hh"
#include <string>

namespace openmsx {

class Event;

namespace InputEventFactory
{
	typedef shared_ptr<const Event> EventPtr;
	EventPtr createInputEvent(const std::string& str);
}

} // namespace openmsx

#endif
