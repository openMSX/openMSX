#ifndef BREAKPOINT_HH
#define BREAKPOINT_HH

#include "BreakPointBase.hh"
#include "openmsx.hh"

namespace openmsx {

/** Base class for CPU breakpoints.
 *  For performance reasons every bp is associated with exactly one
 *  (immutable) address.
 */
class BreakPoint final : public BreakPointBase
{
public:
	BreakPoint(word address_, TclObject command_, TclObject condition_, bool once_)
		: BreakPointBase(command_, condition_, once_)
		, id(++lastId)
		, address(address_) {}

	word getAddress() const { return address; }
	unsigned getId() const { return id; }

private:
	unsigned id;
	word address;

	static inline unsigned lastId = 0;
};

} // namespace openmsx

#endif
