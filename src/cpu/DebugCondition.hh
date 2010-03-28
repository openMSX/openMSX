// $Id$

#ifndef DEBUGCONDITION_HH
#define DEBUGCONDITION_HH

#include "BreakPointBase.hh"

namespace openmsx {

/** General debugger condition
 *  Like breakpoints, but not tied to a specifc address.
 */
class DebugCondition : public BreakPointBase
{
public:
	DebugCondition(GlobalCliComm& CliComm,
	               std::auto_ptr<TclObject> command,
	               std::auto_ptr<TclObject> condition);
	unsigned getId() const;

private:
	const unsigned id;

	static unsigned lastId;
};

} // namespace openmsx

#endif
