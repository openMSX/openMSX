// $Id$

#ifndef BREAKPOINTBASE_HH
#define BREAKPOINTBASE_HH

#include "noncopyable.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXCliComm;
class TclObject;

/** Base class for CPU break and watch points.
 */
class BreakPointBase : private noncopyable
{
public:
	std::string getCondition() const;
	std::string getCommand() const;
	void checkAndExecute();

protected:
	BreakPointBase(MSXCliComm& CliComm,
	               std::auto_ptr<TclObject> command,
	               std::auto_ptr<TclObject> condition);
	~BreakPointBase();

private:
	bool isTrue() const;

	MSXCliComm& cliComm;
	std::auto_ptr<TclObject> command;
	std::auto_ptr<TclObject> condition;
	bool executing;
};

} // namespace openmsx

#endif
