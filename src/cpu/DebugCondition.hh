#ifndef DEBUGCONDITION_HH
#define DEBUGCONDITION_HH

#include "BreakPointBase.hh"

namespace openmsx {

/** General debugger condition
 *  Like breakpoints, but not tied to a specific address.
 */
class DebugCondition final : public BreakPointBase<DebugCondition>
{
public:
	static constexpr std::string_view prefix = "cond#";
};

} // namespace openmsx

#endif
