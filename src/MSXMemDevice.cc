// $Id$
 
#include "StringOp.hh"
#include "MSXCPUInterface.hh"
#include "Config.hh"
#include "MSXMemDevice.hh"

namespace openmsx {

byte MSXMemDevice::unmappedRead[0x10000];
byte MSXMemDevice::unmappedWrite[0x10000];

MSXMemDevice::MSXMemDevice(Config* config, const EmuTime& time)
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

byte MSXMemDevice::readMem(word address, const EmuTime& time)
{
	PRT_DEBUG("MSXMemDevice: read from unmapped memory " << hex <<
	          (int)address << dec);
	return 0xFF;
}

const byte* MSXMemDevice::getReadCacheLine(word start) const
{
	return NULL;	// uncacheable
}

void MSXMemDevice::writeMem(word address, byte value, const EmuTime& time)
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
	int ps = 0;
	int ss = 0;
	int pages = 0;
	
	const XMLElement::Children& children =
		deviceConfig->getXMLElement().getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		if ((*it)->getName() == "slotted") {
			int ps2 = -2;
			int ss2 = -1;
			int page = -1;
			const XMLElement::Children& slot_children = (*it)->getChildren();
			for (XMLElement::Children::const_iterator it2 = slot_children.begin();
			     it2 != slot_children.end(); ++it2) {
				if ((*it2)->getName() == "ps") {
					ps2 = StringOp::stringToInt((*it2)->getPcData());
				} else if ((*it2)->getName() == "ss") {
					ss2 = StringOp::stringToInt((*it2)->getPcData());
				} else if ((*it2)->getName() == "page") {
					page = StringOp::stringToInt((*it2)->getPcData());
				}
			}
			if ((pages != 0) && ((ps != ps2) || (ss != ss2))) {
				throw FatalError("All pages of one device must be in the same slot/subslot");
			}
			if (ps != -2) {
				ps = ps2;
				ss = ss2;
				pages |= 1 << page;
			}
		}
	}
	if (pages == 0) {
		return;
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

