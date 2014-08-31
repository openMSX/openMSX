#include "Probe.hh"
#include "Debugger.hh"

namespace openmsx {

ProbeBase::ProbeBase(Debugger& debugger_, const std::string& name_,
                     const std::string& description_)
	: debugger(debugger_)
	, name(name_)
	, description(description_)
{
	debugger.registerProbe(name, *this);
}

ProbeBase::~ProbeBase()
{
	debugger.unregisterProbe(name, *this);
}


Probe<void>::Probe(Debugger& debugger, const std::string& name,
                   const std::string& description)
	: ProbeBase(debugger, name, description)
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
