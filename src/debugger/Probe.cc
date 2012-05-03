#include "Probe.hh"
#include "Debugger.hh"

namespace openmsx {

ProbeBase::ProbeBase(Debugger& debugger_, std::string name_,
                     std::string description_)
	: debugger(debugger_)
	, name(std::move(name_))
	, description(std::move(description_))
{
	debugger.registerProbe(*this);
}

ProbeBase::~ProbeBase()
{
	debugger.unregisterProbe(*this);
}


Probe<void>::Probe(Debugger& debugger_, std::string name_,
                   std::string description_)
	: ProbeBase(debugger_, std::move(name_), std::move(description_))
{
}

void Probe<void>::signal()
{
	notify();
}

std::string Probe<void>::getValue() const
{
	return {};
}

} // namespace openmsx
