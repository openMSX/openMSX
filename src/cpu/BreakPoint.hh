#ifndef BREAKPOINT_HH
#define BREAKPOINT_HH

#include "BreakPointBase.hh"
#include "CommandException.hh"
#include "openmsx.hh"

#include "strCat.hh"

namespace openmsx {

/** Base class for CPU breakpoints.
 *  For performance reasons every bp is associated with exactly one
 *  (immutable) address.
 */
class BreakPoint final : public BreakPointBase
{
public:
	static constexpr std::string_view prefix = "bp#";

public:
	BreakPoint()
		: id(++lastId) {}
	BreakPoint(word address_, TclObject command_, TclObject condition_, bool once_)
		: BreakPointBase(std::move(command_), std::move(condition_), once_)
		, id(++lastId)
		, address(address_) {}

	[[nodiscard]] unsigned getId() const { return id; }
	[[nodiscard]] std::string getIdStr() const { return strCat(prefix, id); }
	[[nodiscard]] word getAddress() const { return address; }

	void setAddress(Interpreter& interp, const TclObject& a) {
		// TODO store value with given formatting
		auto tmp = a.getInt(interp); // may throw
		if ((tmp < 0) || (tmp > 0xffff)) {
			throw CommandException("address outside of range: ", a.getString());
		}
		address = word(tmp);
	}

private:
	unsigned id;
	word address = 0;

	static inline unsigned lastId = 0;
};

} // namespace openmsx

#endif
