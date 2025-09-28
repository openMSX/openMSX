#include "DummyIDEDevice.hh"

#include "serialize.hh"

namespace openmsx {

void DummyIDEDevice::reset(EmuTime /*time*/)
{
	// do nothing
}

uint16_t DummyIDEDevice::readData(EmuTime /*time*/)
{
	return 0x7F7F;
}

uint8_t DummyIDEDevice::readReg(uint4_t /*reg*/, EmuTime /*time*/)
{
	return 0x7F;
}

void DummyIDEDevice::writeData(uint16_t /*value*/, EmuTime /*time*/)
{
	// do nothing
}

void DummyIDEDevice::writeReg(uint4_t /*reg*/, uint8_t /*value*/,
                              EmuTime /*time*/)
{
	// do nothing
}

template<typename Archive>
void DummyIDEDevice::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// nothing
}
INSTANTIATE_SERIALIZE_METHODS(DummyIDEDevice);
REGISTER_POLYMORPHIC_INITIALIZER(IDEDevice, DummyIDEDevice, "DummyIDEDevice");

} // namespace openmsx
