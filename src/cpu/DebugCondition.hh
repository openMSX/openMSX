#ifndef DEBUGCONDITION_HH
#define DEBUGCONDITION_HH

#include "BreakPointBase.hh"

#include "strCat.hh"

namespace openmsx {

/** General debugger condition
 *  Like breakpoints, but not tied to a specific address.
 */
class DebugCondition final : public BreakPointBase
{
public:
	static constexpr std::string_view prefix = "cond#";

public:
	DebugCondition()
		: id(++lastId) {}

	[[nodiscard]] unsigned getId() const { return id; }
	[[nodiscard]] std::string getIdStr() const { return strCat(prefix, id); }

private:
	unsigned id;

	static inline unsigned lastId = 0;
};

} // namespace openmsx

#endif
