// $Id$

#include "Ram.hh"
#include "SimpleDebuggable.hh"

namespace openmsx {

class RamDebuggable : public SimpleDebuggable
{
public:
	RamDebuggable(MSXMotherBoard& motherBoard, const std::string& name,
	              const std::string& description, Ram& ram);
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);
private:
	Ram& ram;
};


Ram::Ram(MSXMotherBoard& motherBoard, const std::string& name,
         const std::string& description, unsigned size_)
	: size(size_)
	, debuggable(new RamDebuggable(motherBoard, name, description, *this))
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


RamDebuggable::RamDebuggable(MSXMotherBoard& motherBoard,
                             const std::string& name,
                             const std::string& description, Ram& ram_)
	: SimpleDebuggable(motherBoard, name, description, ram_.getSize())
	, ram(ram_)
{
}

byte RamDebuggable::read(unsigned address)
{
	assert(address < getSize());
	return ram[address];
}

void RamDebuggable::write(unsigned address, byte value)
{
	assert(address < getSize());
	ram[address] = value;
}

} // namespace openmsx
