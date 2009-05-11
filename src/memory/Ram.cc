// $Id$

#include "Ram.hh"
#include "SimpleDebuggable.hh"
#include "serialize.hh"
#include <cstring>

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

Ram::Ram(unsigned size_)
	: size(size_)
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

const std::string& Ram::getName() const
{
	return debuggable.get()->getName();
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


template<typename Archive>
void Ram::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize_blob("ram", ram, size);
}
INSTANTIATE_SERIALIZE_METHODS(Ram);

} // namespace openmsx
