// $Id$

#include "Ram.hh"
#include "Debugger.hh"

namespace openmsx {

Ram::Ram(const string& name_, unsigned size_)
	: name(name_), description("ram"), size(size_)
{
	init();
}

Ram::Ram(const string& name_, const string& description_, unsigned size_)
	: name(name_), description(description_), size(size_)
{
	init();
}

void Ram::init()
{
	ram = new byte[size];
	clear();
	
	Debugger::instance().registerDebuggable(name, *this);
}

Ram::~Ram()
{
	Debugger::instance().unregisterDebuggable(name, *this);

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
