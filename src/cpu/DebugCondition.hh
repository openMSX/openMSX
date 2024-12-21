#ifndef DEBUGCONDITION_HH
#define DEBUGCONDITION_HH

#include "BreakPointBase.hh"

namespace openmsx {

/** General debugger condition
 *  Like breakpoints, but not tied to a specific address.
 */
class DebugCondition final : public BreakPointBase
{
public:
	DebugCondition()
		: id(++lastId) {}
	DebugCondition(TclObject command_, TclObject condition_, bool once_)
		: BreakPointBase(std::move(command_), std::move(condition_), once_)
		, id(++lastId) {}

	[[nodiscard]] unsigned getId() const { return id; }

private:
	unsigned id;

	static inline unsigned lastId = 0;
};

} // namespace openmsx

#endif
