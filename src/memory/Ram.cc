// $Id$

#include "Ram.hh"
#include "MSXMotherBoard.hh"
#include "Debugger.hh"

using std::string;

namespace openmsx {

Ram::Ram(MSXMotherBoard& motherBoard, const string& name_, unsigned size_)
	: debugger(motherBoard.getDebugger())
	, name(name_), description("ram"), size(size_)
{
	init();
}

Ram::Ram(MSXMotherBoard& motherBoard, const string& name_,
         const string& description_, unsigned size_)
	: debugger(motherBoard.getDebugger())
	, name(name_), description(description_), size(size_)
{
	init();
}

void Ram::init()
{
	ram = new byte[size];
	clear();

	debugger.registerDebuggable(name, *this);
}

Ram::~Ram()
{
	debugger.unregisterDebuggable(name, *this);

	delete[] ram;
}

void Ram::clear()
{
	memset(ram, 0xFF, size);
}

unsigned Ram::getSize() const
{
	return size;
}

const string& Ram::getDescription() const
{
	return description;
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
