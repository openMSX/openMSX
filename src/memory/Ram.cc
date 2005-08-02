// $Id$

#include "Ram.hh"
#include "MSXMotherBoard.hh"

namespace openmsx {

Ram::Ram(MSXMotherBoard& motherBoard, const std::string& name,
         const std::string& description, unsigned size_)
	: SimpleDebuggable(motherBoard.getDebugger(), name, description, size_)
	, size(size_)
{
	ram = new byte[size];
	clear();
}

Ram::~Ram()
{
	delete[] ram;
}

void Ram::clear()
{
	memset(ram, 0xFF, size);
}

byte Ram::read(unsigned address)
{
	assert(address < size);
	return ram[address];
}

void Ram::write(unsigned address, byte value)
{
	assert(address < size);
	ram[address] = value;
}

} // namespace openmsx
