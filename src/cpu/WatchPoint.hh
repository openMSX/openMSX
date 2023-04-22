#ifndef WATCHPOINT_HH
#define WATCHPOINT_HH

#include "BreakPointBase.hh"
#include <cassert>

namespace openmsx {

/** Base class for CPU breakpoints.
 *  For performance reasons every bp is associated with exactly one
 *  (immutable) address.
 */
class WatchPoint : public BreakPointBase
{
public:
	enum Type { READ_IO = 0, WRITE_IO = 1, READ_MEM = 2, WRITE_MEM = 3 };

	/** Begin and end address are inclusive (IOW range = [begin, end])
	 */
	WatchPoint(TclObject command_, TclObject condition_,
	           Type type_, unsigned beginAddr_, unsigned endAddr_,
	           bool once_, unsigned newId = -1)
		: BreakPointBase(std::move(command_), std::move(condition_), once_)
		, id((newId == unsigned(-1)) ? ++lastId : newId)
		, beginAddr(beginAddr_), endAddr(endAddr_), type(type_)
	{
		assert(beginAddr <= endAddr);
	}

	virtual ~WatchPoint() = default; // needed for dynamic_cast

	[[nodiscard]] unsigned getId()           const { return id; }
	[[nodiscard]] Type     getType()         const { return type; }
	[[nodiscard]] unsigned getBeginAddress() const { return beginAddr; }
	[[nodiscard]] unsigned getEndAddress()   const { return endAddr; }

private:
	unsigned id;
	unsigned beginAddr;
	unsigned endAddr;
	Type type;

	static inline unsigned lastId = 0;
};

} // namespace openmsx

#endif
