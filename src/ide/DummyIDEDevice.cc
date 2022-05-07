#include "DummyIDEDevice.hh"
#include "serialize.hh"

namespace openmsx {

void DummyIDEDevice::reset(EmuTime::param /*time*/)
{
	// do nothing
}

word DummyIDEDevice::readData(EmuTime::param /*time*/)
{
	return 0x7F7F;
}

byte DummyIDEDevice::readReg(nibble /*reg*/, EmuTime::param /*time*/)
{
	return 0x7F;
}

void DummyIDEDevice::writeData(word /*value*/, EmuTime::param /*time*/)
{
	// do nothing
}

void DummyIDEDevice::writeReg(nibble /*reg*/, byte /*value*/,
                              EmuTime::param /*time*/)
{
	// do nothing
}

void DummyIDEDevice::serialize(Archive auto& /*ar*/, unsigned /*version*/)
{
	// nothing
}
INSTANTIATE_SERIALIZE_METHODS(DummyIDEDevice);
REGISTER_POLYMORPHIC_INITIALIZER(IDEDevice, DummyIDEDevice, "DummyIDEDevice");

} // namespace openmsx
