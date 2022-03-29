#include "ProbeBreakPoint.hh"
#include "Probe.hh"
#include "Debugger.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "TclObject.hh"

namespace openmsx {

ProbeBreakPoint::ProbeBreakPoint(
		TclObject command_,
		TclObject condition_,
		Debugger& debugger_,
		ProbeBase& probe_,
		bool once_,
		unsigned newId /*= -1*/)
	: BreakPointBase(std::move(command_), std::move(condition_), once_)
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

void ProbeBreakPoint::update(const ProbeBase& /*subject*/) noexcept
{
	auto& reactor = debugger.getMotherBoard().getReactor();
	auto& cliComm = reactor.getGlobalCliComm();
	auto& interp  = reactor.getInterpreter();
	bool remove = checkAndExecute(cliComm, interp);
	if (remove) {
		debugger.removeProbeBreakPoint(*this);
	}
}

void ProbeBreakPoint::subjectDeleted(const ProbeBase& /*subject*/)
{
	debugger.removeProbeBreakPoint(*this);
}

} // namespace openmsx
