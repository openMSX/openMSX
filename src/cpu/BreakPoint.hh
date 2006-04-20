// $Id$

#ifndef BREAKPOINT_HH
#define BREAKPOINT_HH

#include "BreakPointBase.hh"
#include "openmsx.hh"

namespace openmsx {

class CliComm;
class TclObject;

/** Base class for CPU breakpoints.
 *  For performance reasons every bp is associated with exactly one
 *  (immutable) address.
 */
class BreakPoint : public BreakPointBase
{
public:
	BreakPoint(CliComm& CliComm, word address,
	           std::auto_ptr<TclObject> command,
	           std::auto_ptr<TclObject> condition);

	word getAddress() const;
	unsigned getId() const;

private:
	word address;
	unsigned id;

	static unsigned lastId;
};

} // namespace openmsx

#endif
