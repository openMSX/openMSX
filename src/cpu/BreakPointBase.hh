// $Id$

#ifndef BREAKPOINTBASE_HH
#define BREAKPOINTBASE_HH

#include "noncopyable.hh"
#include <string>
#include <memory>

struct Tcl_Interp;

namespace openmsx {

class GlobalCliComm;
class TclObject;

/** Base class for CPU break and watch points.
 */
class BreakPointBase : private noncopyable
{
public:
	std::string getCondition() const;
	std::string getCommand() const;
	void checkAndExecute();

	// get associated interpreter
	Tcl_Interp* getInterpreter() const;

protected:
	// Note: we require GlobalCliComm here because breakpoint objects can
	// be transfered to different MSX machines, and so the MSXCliComm
	// object won't remain valid.
	BreakPointBase(GlobalCliComm& cliComm,
	               std::auto_ptr<TclObject> command,
	               std::auto_ptr<TclObject> condition);
	~BreakPointBase();

private:
	bool isTrue() const;

	GlobalCliComm& cliComm;
	const std::auto_ptr<TclObject> command;
	const std::auto_ptr<TclObject> condition;
	bool executing;
};

} // namespace openmsx

#endif
