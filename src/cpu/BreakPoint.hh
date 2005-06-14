// $Id$

#ifndef BREAKPOINT_HH
#define BREAKPOINT_HH

#include "openmsx.hh"
#include <memory>

namespace openmsx {

class TclObject;

/** Base class for CPU breakpoints.
 *  For performance reasons every bp is associated with exactly one
 *  (immutable) address.
 */
class BreakPoint
{
public:
	explicit BreakPoint(word address);
	BreakPoint(word address, std::auto_ptr<TclObject> condition);
	~BreakPoint();

	word getAddress() const;
	std::string getCondition() const;
	unsigned getId() const;
	
	bool isTrue() const;

private:
	word address;
	std::auto_ptr<TclObject> condition;
	unsigned id;

	static unsigned lastId;
};

} // namespace openmsx

#endif
