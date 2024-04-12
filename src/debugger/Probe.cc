#include "Probe.hh"

#include "Debugger.hh"

namespace openmsx {

ProbeBase::ProbeBase(Debugger& debugger_, std::string name_,
                     static_string_view description_)
	: debugger(debugger_)
	, name(std::move(name_))
	, description(description_)
{
	debugger.registerProbe(*this);
}

ProbeBase::~ProbeBase()
{
	debugger.unregisterProbe(*this);
}


Probe<void>::Probe(Debugger& debugger_, std::string name_,
                   static_string_view description_)
	: ProbeBase(debugger_, std::move(name_), description_)
{
}

void Probe<void>::signal() const
{
	notify();
}

std::string Probe<void>::getValue() const
{
	return {};
}

} // namespace openmsx
