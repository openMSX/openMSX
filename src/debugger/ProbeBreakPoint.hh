// $Id$

#ifndef PROBEBREAKPOINT_HH
#define PROBEBREAKPOINT_HH

#include "BreakPointBase.hh"
#include "Observer.hh"
#include <string>

namespace openmsx {

class Debugger;
class ProbeBase;

class ProbeBreakPoint : public BreakPointBase, private Observer<ProbeBase>
{
public:
	ProbeBreakPoint(GlobalCliComm& CliComm,
	                TclObject command,
	                TclObject condition,
	                Debugger& debugger,
	                ProbeBase& probe,
	                unsigned newId = -1);
	~ProbeBreakPoint();

	unsigned getId() const { return id; }
	const ProbeBase& getProbe() const { return probe; }

private:
	// Observer<ProbeBase>
	virtual void update(const ProbeBase& subject);
	virtual void subjectDeleted(const ProbeBase& subject);

	Debugger& debugger;
	ProbeBase& probe;
	const unsigned id;

	static unsigned lastId;
};

} // namespace openmsx

#endif
