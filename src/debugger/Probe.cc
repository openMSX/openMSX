// $ID: $

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

const std::string& ProbeBase::getName() const
{
	return name;
}

const std::string& ProbeBase::getDescription() const
{
	return description;
}

} // namespace openmsx
