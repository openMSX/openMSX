// $Id$
 
#include "MSXCPUInterface.hh"
#include "MSXMemDevice.hh"
#include "Device.hh"


namespace openmsx {

byte MSXMemDevice::unmappedRead[0x10000];
byte MSXMemDevice::unmappedWrite[0x10000];

MSXMemDevice::MSXMemDevice(Device* config, const EmuTime& time)
	: MSXDevice(config, time)
{
	init();
	registerSlots();
}

MSXMemDevice::~MSXMemDevice()
{
	// TODO unregister slots
}

void MSXMemDevice::init()
{
	static bool alreadyInit = false;
	if (alreadyInit) {
		return;
	}
	alreadyInit = true;
	memset(unmappedRead, 0xFF, 0x10000);
}

byte MSXMemDevice::readMem(word address, const EmuTime &time)
{
	PRT_DEBUG("MSXMemDevice: read from unmapped memory " << hex <<
	          (int)address << dec);
	return 0xFF;
}

const byte* MSXMemDevice::getReadCacheLine(word start) const
{
	return NULL;	// uncacheable
}

void MSXMemDevice::writeMem(word address, byte value, const EmuTime &time)
{
	PRT_DEBUG("MSXMemDevice: write to unmapped memory " << hex <<
	          (int)address << dec);
	// do nothing
}

byte MSXMemDevice::peekMem(word address) const
{
	word base = address & CPU::CACHE_LINE_HIGH;
	const byte* cache = getReadCacheLine(base);
	if (cache) {
		word offset = address & CPU::CACHE_LINE_LOW;
		return cache[offset];
	} else {
		PRT_DEBUG("MSXMemDevice: peek not supported for this device");
		return 0xFF;
	}
}

byte* MSXMemDevice::getWriteCacheLine(word start) const
{
	return NULL;	// uncacheable
}

void MSXMemDevice::registerSlots()
{
	// register in slot-structure
	const Device::Slots& slots = deviceConfig->getSlots();
	if (slots.empty()) {
		return; // DummyDevice
	}
	
	int ps = slots.front().getPS();
	int ss = slots.front().getSS();
	int pages = 0;
	for (Device::Slots::const_iterator it = slots.begin();
	     it != slots.end(); ++it) {
		if ((it->getPS() != ps) || (it->getSS() != ss)) {
			throw FatalError("All pages of one device must be in the same slot/subslot");
		}
		pages |= 1 << (it->getPage());
	}
	if (ps >= 0) {
		// slot specified
		MSXCPUInterface::instance().
			registerSlottedDevice(this, ps, ss, pages);
	} else {
		// take any free slot
		MSXCPUInterface::instance().
			registerSlottedDevice(this, pages);
	}
}

} // namespace openmsx

