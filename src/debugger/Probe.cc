#include "Probe.hh"
#include "Debugger.hh"

namespace openmsx {

ProbeBase::ProbeBase(Debugger& debugger_, const std::string& name_,
                     const std::string& description_)
	: debugger(debugger_)
	, name(name_)
	, description(description_)
{
	debugger.registerProbe(*this);
}

ProbeBase::~ProbeBase()
{
	debugger.unregisterProbe(*this);
}


Probe<void>::Probe(Debugger& debugger_, const std::string& name_,
                   const std::string& description_)
	: ProbeBase(debugger_, name_, description_)
{
}

void Probe<void>::signal()
{
	notify();
}

std::string Probe<void>::getValue() const
{
	return "";
}

} // namespace openmsx
