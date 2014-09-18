#ifndef DEBUGCONDITION_HH
#define DEBUGCONDITION_HH

#include "BreakPointBase.hh"

namespace openmsx {

/** General debugger condition
 *  Like breakpoints, but not tied to a specifc address.
 */
class DebugCondition final : public BreakPointBase
{
public:
	DebugCondition(GlobalCliComm& CliComm, Interpreter& interp,
	               TclObject command, TclObject condition);
	unsigned getId() const { return id; }

private:
	const unsigned id;

	static unsigned lastId;
};

} // namespace openmsx

#endif
