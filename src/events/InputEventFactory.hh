#ifndef INPUTEVENTFACTORY_HH
#define INPUTEVENTFACTORY_HH

#include "Event.hh"
#include <string_view>

namespace openmsx {

class TclObject;
class Interpreter;

namespace InputEventFactory
{
	[[nodiscard]] Event createInputEvent(std::string_view str, Interpreter& interp);
	[[nodiscard]] Event createInputEvent(const TclObject& str, Interpreter& interp);
}

} // namespace openmsx

#endif
