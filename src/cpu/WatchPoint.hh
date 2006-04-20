// $Id$

#ifndef WATCHPOINT_HH
#define WATCHPOINT_HH

#include "BreakPointBase.hh"

namespace openmsx {

/** Base class for CPU breakpoints.
 *  For performance reasons every bp is associated with exactly one
 *  (immutable) address.
 */
class WatchPoint : public BreakPointBase
{
public:
	enum Type { READ_IO, WRITE_IO, READ_MEM, WRITE_MEM };

	WatchPoint(CliComm& CliComm,
	           std::auto_ptr<TclObject> command,
	           std::auto_ptr<TclObject> condition,
	           Type type, unsigned address);
	virtual ~WatchPoint();

	unsigned getId() const;
	Type getType() const;
	unsigned getAddress() const;

private:
	unsigned id;
	Type type;
	unsigned address;

	static unsigned lastId;
};

} // namespace openmsx

#endif
