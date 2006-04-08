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
	BreakPoint(CliComm& CliComm, word address,
	           std::auto_ptr<TclObject> command,
	           std::auto_ptr<TclObject> condition);
	~BreakPoint();

	word getAddress() const;
	std::string getCondition() const;
	std::string getCommand() const;
	unsigned getId() const;

	void checkAndExecute();

private:
	bool isTrue() const;

	CliComm& cliComm;
	std::auto_ptr<TclObject> command;
	std::auto_ptr<TclObject> condition;
	word address;
	unsigned id;

	static unsigned lastId;
};

} // namespace openmsx

#endif
