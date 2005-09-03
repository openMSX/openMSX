// $Id$

#ifndef BREAKPOINT_HH
#define BREAKPOINT_HH

#include "openmsx.hh"
#include <memory>

namespace openmsx {

class CliComm;
class TclObject;

/** Base class for CPU breakpoints.
 *  For performance reasons every bp is associated with exactly one
 *  (immutable) address.
 */
class BreakPoint
{
public:
	BreakPoint(CliComm& cliComm, word address);
	BreakPoint(CliComm& CliComm, word address,
	           std::auto_ptr<TclObject> condition);
	~BreakPoint();

	word getAddress() const;
	std::string getCondition() const;
	unsigned getId() const;
	
	bool isTrue() const;

private:
	CliComm& cliComm;
	word address;
	std::auto_ptr<TclObject> condition;
	unsigned id;

	static unsigned lastId;
};

} // namespace openmsx

#endif
