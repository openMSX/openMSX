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
	                std::auto_ptr<TclObject> command,
	                std::auto_ptr<TclObject> condition,
	                Debugger& debugger,
	                ProbeBase& probe);
	~ProbeBreakPoint();

	unsigned getId() const;
	const ProbeBase& getProbe() const;

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
