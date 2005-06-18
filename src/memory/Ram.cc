// $Id$

#include "Ram.hh"
#include "MSXMotherBoard.hh"
#include "Debugger.hh"

using std::string;

namespace openmsx {

Ram::Ram(MSXMotherBoard& motherBoard, const string& name_, unsigned size_)
	: debug(*this), debugger(motherBoard.getDebugger())
	, name(name_), description("ram"), size(size_)
{
	init();
}

Ram::Ram(MSXMotherBoard& motherBoard, const string& name_,
         const string& description_, unsigned size_)
	: debug(*this), debugger(motherBoard.getDebugger())
	, name(name_), description(description_), size(size_)
{
	init();
}

void Ram::init()
{
	ram = new byte[size];
	clear();

	debugger.registerDebuggable(name, debug);
}

Ram::~Ram()
{
	debugger.unregisterDebuggable(name, debug);

	delete[] ram;
}

unsigned Ram::getSize() const
{
	return size;
}

void Ram::clear()
{
	memset(ram, 0xFF, size);
}

Ram::Debug::Debug(Ram& parent_)
	: parent(parent_)
{
}

unsigned Ram::Debug::getSize() const
{
	return parent.getSize();
}

const string& Ram::Debug::getDescription() const
{
	return parent.description;
}

byte Ram::Debug::read(unsigned address)
{
	assert(address < parent.getSize());
	return parent[address];
}

void Ram::Debug::write(unsigned address, byte value)
{
	assert(address < parent.getSize());
	parent[address] = value;
}

} // namespace openmsx
