// $Id$

#include "MemoryView.hh"
#include "ViewControl.hh"
#include "MSXCPUInterface.hh"
#include "Debuggable.hh"

namespace openmsx {

ViewControl::ViewControl(MemoryView* view_)
{
	view = view_;
	currentDevice = NULL;
	currentCriterium = 0;
	indirect = false;
	memoryAddress = 0;
	linked = NULL;
	struct MSXCPUInterface::SlotSelection* slots =
		MSXCPUInterface::instance().getCurrentSlots();
	useGlobalSlot = false;
	for (int i = 0; i < 4; i++) {
		slot.ps[i] = slots->primary[i];
		slot.ss[i] = slots->secondary[i];
//		slot.map[i] = mapper->getSelectedPage(i);
	}
	delete slots;
}

ViewControl::~ViewControl()
{
}

void ViewControl::setAddress(int address)
{
	view->setAddress(address);
	if (linked) {
		linked->setAddress(address);
	}
}

byte ViewControl::readByte(word address) const
{
	MSXCPUInterface& msxcpu = MSXCPUInterface::instance();
	if (useGlobalSlot) {
		return msxcpu.peekMem(memoryAddress);
	} else {
		byte page = address >> 14;
		byte primSlot = slot.ps[page];
		byte subSlot  = slot.ss[page];
		unsigned addr = (primSlot << 18) + (subSlot << 16) + address;
		return msxcpu.peekSlottedMem(addr);
	}
}

int ViewControl::getAddress() const
{
	if (indirect) {
		return readByte(memoryAddress) +
		       readByte(memoryAddress + 1) * 256;
	}
	if (!currentDevice) {
		return -1;
	}
	return currentDevice->read(currentCriterium);
}

bool ViewControl::linkToCriterium(Debuggable* device, const string& criteria)
{
	currentDevice = device;
	if ((criteria[0] >= '0') && (criteria[0] <= '9')) {
		currentCriterium = atoi(criteria.c_str());
		return true;
	} else {
		return false;
		//currentCriterium = currentDevice->getRegisterNumber(criteria);
	}
	if (currentCriterium != 0xffff) {
		return true;
	}
	return false;
}

} // namespace openmsx
