// $Id$
 
#include "StringOp.hh"
#include "MSXCPUInterface.hh"
#include "MSXMemDevice.hh"
#include "xmlx.hh"

namespace openmsx {

byte MSXMemDevice::unmappedRead[0x10000];
byte MSXMemDevice::unmappedWrite[0x10000];

MSXMemDevice::MSXMemDevice(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	init();
	registerSlots(config);
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

byte MSXMemDevice::readMem(word address, const EmuTime& /*time*/)
{
	PRT_DEBUG("MSXMemDevice: read from unmapped memory " << hex <<
	          (int)address << dec);
	return 0xFF;
}

const byte* MSXMemDevice::getReadCacheLine(word /*start*/) const
{
	return NULL;	// uncacheable
}

void MSXMemDevice::writeMem(word address, byte /*value*/,
                            const EmuTime& /*time*/)
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

byte* MSXMemDevice::getWriteCacheLine(word /*start*/) const
{
	return NULL;	// uncacheable
}

void MSXMemDevice::registerSlots(const XMLElement& config)
{
	const XMLElement* mem = config.findChild("mem");
	if (!mem) {
		return;
	}
	unsigned base = mem->getAttributeAsInt("base");
	unsigned size = mem->getAttributeAsInt("size");
	if ((base & ~0xC000) || ((size - 0x4000) & ~0xC000)) {
		throw FatalError("Invalid memory specification");
	}
	int page = base / 0x4000;
	int pages = 0;
	for (unsigned i = 0; i < (size / 0x4000); ++i, ++page) {
		pages |= 1 << page;
	}

	int ps = -2;
	int ss = 0;
	const XMLElement* parent = config.getParent();
	while (parent) {
		const string& name = parent->getName();
		if (name == "secondary") {
			const string& secondSlot = parent->getAttribute("slot");
			ss = (secondSlot == "any") ? -1 :
				StringOp::stringToInt(secondSlot);
		} else if (name == "primary") {
			const string& primSlot = parent->getAttribute("slot");
			ps = (primSlot == "any") ? -1 :
				StringOp::stringToInt(primSlot);
			break;
		}
		parent = parent->getParent();
	}

	if ((ps <= -2) || (4 <= ps)) {
		throw FatalError("Invalid memory specification");
	}
	if (ps != -1) {
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

