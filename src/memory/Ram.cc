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
         const std::string& description, unsigned size)
	: ram(size)
	, debuggable(new RamDebuggable(motherBoard, name, description, *this))
{
	clear();
}

Ram::Ram(unsigned size)
	: ram(size)
{
	clear();
}

Ram::~Ram()
{
}

void Ram::clear()
{
	memset(ram.data(), 0xFF, ram.size());
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
	return ram[address];
}

void RamDebuggable::write(unsigned address, byte value)
{
	ram[address] = value;
}


template<typename Archive>
void Ram::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize_blob("ram", ram.data(), ram.size());
}
INSTANTIATE_SERIALIZE_METHODS(Ram);

} // namespace openmsx
