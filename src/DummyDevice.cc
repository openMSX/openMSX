#include "DummyDevice.hh"
#include "unreachable.hh"

namespace openmsx {

DummyDevice::DummyDevice(const DeviceConfig& config)
	: MSXDevice(config)
{
}

void DummyDevice::reset(EmuTime /*time*/)
{
	UNREACHABLE;
}

void DummyDevice::getNameList(TclObject& /*result*/) const
{
	// keep empty list
}

byte DummyDevice::readMem(uint16_t /*address*/, EmuTime /*time*/)
{
	return 0xFF;
}

byte DummyDevice::peekMem(uint16_t /*address*/, EmuTime /*time*/) const
{
	return 0xFF;
}

void DummyDevice::writeMem(uint16_t /*address*/, byte /*value*/, EmuTime /*time*/)
{
	// ignore
}

const byte* DummyDevice::getReadCacheLine(uint16_t /*start*/) const
{
	return unmappedRead.data();
}

byte* DummyDevice::getWriteCacheLine(uint16_t /*start*/)
{
	return unmappedWrite.data();
}

} // namespace openmsx
