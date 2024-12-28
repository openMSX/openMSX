#include "ProbeBreakPoint.hh"

#include "Debugger.hh"
#include "Probe.hh"

#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "StateChangeDistributor.hh"
#include "TclObject.hh"

namespace openmsx {

ProbeBreakPoint::ProbeBreakPoint(
		TclObject command_,
		TclObject condition_,
		Debugger& debugger_,
		ProbeBase& probe_,
		bool once_,
		unsigned newId /*= -1*/)
	: debugger(debugger_)
	, probe(probe_)
{
	if (newId != unsigned(-1)) id = newId;
	setCommand(std::move(command_));
	setCondition(std::move(condition_));
	setOnce(once_);

	probe.attach(*this);
}

ProbeBreakPoint::~ProbeBreakPoint()
{
	probe.detach(*this);
}

void ProbeBreakPoint::update(const ProbeBase& /*subject*/) noexcept
{
	auto& motherBoard = debugger.getMotherBoard();
	auto scopedBlock = motherBoard.getStateChangeDistributor().tempBlockNewEventsDuringReplay();
	auto& reactor = motherBoard.getReactor();
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
