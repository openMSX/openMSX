#include "Y8960AdpcmMemory.hh"

#include "serialize.hh"

namespace openmsx {

Y8960AdpcmMemory::Y8960AdpcmMemory(const DeviceConfig& config)
	: MSXDevice(config)
	, ram(config, getName(), "Y8960 ADPCM sample RAM", Size)
{
}

void Y8960AdpcmMemory::powerUp(EmuTime /*time*/)
{
	ram.clear(0xFF);
}

uint8_t Y8960AdpcmMemory::read(unsigned addr) const
{
	if (addr >= Size) return 0;
	return ram[addr];
}

void Y8960AdpcmMemory::write(unsigned addr, uint8_t value)
{
	if (addr >= Size) return;
	ram.write(addr, value);
}

void Y8960AdpcmMemory::clear()
{
	ram.clear(0xFF);
}

template<typename Archive>
void Y8960AdpcmMemory::serialize(Archive& ar, unsigned /*version*/)
{
	// Version 1 also stored how two OPL2 blocks divided the memory; that
	// went with the second block.
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ram", ram);
}
INSTANTIATE_SERIALIZE_METHODS(Y8960AdpcmMemory);
REGISTER_MSXDEVICE(Y8960AdpcmMemory, "Y8960-ADPCM-RAM");

} // namespace openmsx
