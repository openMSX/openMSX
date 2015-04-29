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
	WatchPoint(TclObject command, TclObject condition,
	           Type type, unsigned beginAddr, unsigned endAddr,
	           unsigned newId = -1);
	virtual ~WatchPoint(); // needed for dynamic_cast

	unsigned getId()           const { return id; }
	Type     getType()         const { return type; }
	unsigned getBeginAddress() const { return beginAddr; }
	unsigned getEndAddress()   const { return endAddr; }

private:
	unsigned id;
	unsigned beginAddr;
	unsigned endAddr;
	Type type;

	static unsigned lastId;
};

} // namespace openmsx

#endif
