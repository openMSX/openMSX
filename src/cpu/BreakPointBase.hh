// $Id$

#ifndef BREAKPOINTBASE_HH
#define BREAKPOINTBASE_HH

#include <string>
#include <memory>

namespace openmsx {

class CliComm;
class TclObject;

/** Base class for CPU break and watch points.
 */
class BreakPointBase
{
public:
	std::string getCondition() const;
	std::string getCommand() const;
	void checkAndExecute();

protected:
	BreakPointBase(CliComm& CliComm,
	               std::auto_ptr<TclObject> command,
	               std::auto_ptr<TclObject> condition);
	~BreakPointBase();

private:
	bool isTrue() const;

	CliComm& cliComm;
	std::auto_ptr<TclObject> command;
	std::auto_ptr<TclObject> condition;
	bool executing;
};

} // namespace openmsx

#endif
