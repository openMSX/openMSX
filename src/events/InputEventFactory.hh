#ifndef INPUTEVENTFACTORY_HH
#define INPUTEVENTFACTORY_HH

#include <memory>
#include <string>

namespace openmsx {

class Event;

namespace InputEventFactory
{
	typedef std::shared_ptr<const Event> EventPtr;
	EventPtr createInputEvent(const std::string& str);
}

} // namespace openmsx

#endif
