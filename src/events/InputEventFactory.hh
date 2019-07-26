#ifndef INPUTEVENTFACTORY_HH
#define INPUTEVENTFACTORY_HH

#include <memory>
#include <string_view>

namespace openmsx {

class Event;
class TclObject;
class Interpreter;

namespace InputEventFactory
{
	using EventPtr = std::shared_ptr<const Event>;

	EventPtr createInputEvent(std::string_view str, Interpreter& interp);
	EventPtr createInputEvent(const TclObject& str, Interpreter& interp);
}

} // namespace openmsx

#endif
