// $Id$
 
#include "StringOp.hh"
#include "MSXCPUInterface.hh"
#include "MSXMemDevice.hh"
#include "CartridgeSlotManager.hh"
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
	unregisterSlots();
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
	pages = 0;
	for (unsigned i = 0; i < (size / 0x4000); ++i, ++page) {
		pages |= 1 << page;
	}

	ps = 0;
	ss = 0;
	const XMLElement* parent = config.getParent();
	while (true) {
		const string& name = parent->getName();
		if (name == "secondary") {
			const string& secondSlot = parent->getAttribute("slot");
			ss = CartridgeSlotManager::getSlotNum(secondSlot);
		} else if (name == "primary") {
			const string& primSlot = parent->getAttribute("slot");
			ps = CartridgeSlotManager::getSlotNum(primSlot);
			break;
		}
		parent = parent->getParent();
		if (!parent) {
			throw FatalError("Invalid memory specification");
		}
	}

	if (ps == -256) {
		// any slot
		CartridgeSlotManager::instance().getSlot(ps, ss);
	} else if (ps < 0) {
		// specified slot by name (carta, cartb, ...)
		CartridgeSlotManager::instance().getSlot(-ps - 1, ps, ss);
	} else {
		// numerical specified slot (0, 1, 2, 3)
	}
	MSXCPUInterface::instance().registerMemDevice(*this, ps, ss, pages);
}

void MSXMemDevice::unregisterSlots()
{
	MSXCPUInterface::instance().unregisterMemDevice(*this, ps, ss, pages);
}

} // namespace openmsx

