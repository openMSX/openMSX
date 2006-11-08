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

	/** Begin and end address are inclusive (IOW range = [begin, end])
	 */
	WatchPoint(MSXCliComm& CliComm,
	           std::auto_ptr<TclObject> command,
	           std::auto_ptr<TclObject> condition,
	           Type type, unsigned beginAddr, unsigned endAddr);
	virtual ~WatchPoint();

	unsigned getId() const;
	Type getType() const;
	unsigned getBeginAddress() const;
	unsigned getEndAddress() const;

private:
	unsigned id;
	Type type;
	unsigned beginAddr;
	unsigned endAddr;

	static unsigned lastId;
};

} // namespace openmsx

#endif
