#include "SimpleDebuggable.hh"
#include "MSXMotherBoard.hh"
#include "Debugger.hh"
#include "unreachable.hh"

namespace openmsx {

SimpleDebuggable::SimpleDebuggable(
		MSXMotherBoard& motherBoard_, std::string name_,
		static_string_view description_, unsigned size_)
	: motherBoard(motherBoard_)
	, name(std::move(name_))
	, description(description_)
	, size(size_)
{
	motherBoard.getDebugger().registerDebuggable(name, *this);
}

SimpleDebuggable::~SimpleDebuggable()
{
	motherBoard.getDebugger().unregisterDebuggable(name, *this);
}

unsigned SimpleDebuggable::getSize() const
{
	return size;
}

std::string_view SimpleDebuggable::getDescription() const
{
	return description;
}

byte SimpleDebuggable::read(unsigned address)
{
	return read(address, motherBoard.getCurrentTime());
}

byte SimpleDebuggable::read(unsigned /*address*/, EmuTime::param /*time*/)
{
	UNREACHABLE;
}

void SimpleDebuggable::write(unsigned address, byte value)
{
	write(address, value, motherBoard.getCurrentTime());
}

void SimpleDebuggable::write(unsigned /*address*/, byte /*value*/,
                             EmuTime::param /*time*/)
{
	// does nothing
}

} // namespace openmsx
