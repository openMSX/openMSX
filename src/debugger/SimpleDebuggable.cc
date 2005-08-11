// $Id$

#include "SimpleDebuggable.hh"
#include "Debugger.hh"
#include "Scheduler.hh"
#include <cassert>


namespace openmsx {

SimpleDebuggable::SimpleDebuggable(
		Debugger& debugger_, const std::string& name_,
		const std::string& description_, unsigned size_)
	: debugger(debugger_)
	, name(name_)
	, description(description_)
	, size(size_)
{
	debugger.registerDebuggable(name, *this);
}

SimpleDebuggable::~SimpleDebuggable()
{
	debugger.unregisterDebuggable(name, *this);
}

unsigned SimpleDebuggable::getSize() const
{
	return size;
}

const std::string& SimpleDebuggable::getDescription() const
{
	return description;
}

byte SimpleDebuggable::read(unsigned address)
{
	return read(address, Scheduler::instance().getCurrentTime());
}

byte SimpleDebuggable::read(unsigned /*address*/, const EmuTime& /*time*/)
{
	assert(false);
	return 0;
}

void SimpleDebuggable::write(unsigned address, byte value)
{
	write(address, value, Scheduler::instance().getCurrentTime());
}

void SimpleDebuggable::write(unsigned /*address*/, byte /*value*/,
                             const EmuTime& /*time*/)
{
	// does nothing
}

} // namespace openmsx
