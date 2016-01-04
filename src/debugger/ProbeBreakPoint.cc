#include "ProbeBreakPoint.hh"
#include "Probe.hh"
#include "Debugger.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "TclObject.hh"

namespace openmsx {

unsigned ProbeBreakPoint::lastId = 0;

ProbeBreakPoint::ProbeBreakPoint(
		TclObject command_,
		TclObject condition_,
		Debugger& debugger_,
		ProbeBase& probe_,
		unsigned newId /*= -1*/)
	: BreakPointBase(command_, condition_)
	, debugger(debugger_)
	, probe(probe_)
	, id((newId == unsigned(-1)) ? ++lastId : newId)
{
	probe.attach(*this);
}

ProbeBreakPoint::~ProbeBreakPoint()
{
	probe.detach(*this);
}

void ProbeBreakPoint::update(const ProbeBase& /*subject*/)
{
	auto& reactor = debugger.getMotherBoard().getReactor();
	auto& cliComm = reactor.getGlobalCliComm();
	auto& interp  = reactor.getInterpreter();
	checkAndExecute(cliComm, interp);
}

void ProbeBreakPoint::subjectDeleted(const ProbeBase& /*subject*/)
{
	debugger.removeProbeBreakPoint(*this);
}

} // namespace openmsx
