// $Id$

#include <memory>
#include "Interpreter.hh"
#include "TCLInterp.hh"

using std::auto_ptr;

namespace openmsx {

Interpreter& Interpreter::instance()
{
	static auto_ptr<Interpreter> oneInstance;
	if (!oneInstance.get()) {
		oneInstance.reset(new TCLInterp());
	}
	return *oneInstance.get();
}

} // namespace openmsx
