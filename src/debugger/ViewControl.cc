// $Id$

#include "MemoryView.hh"
#include "ViewControl.hh"
#include "MSXCPUInterface.hh"
#include "DebugInterface.hh"

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
	slot.vram = false;
	slot.direct = false;
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

int ViewControl::getAddress() const
{
	if (indirect) {
		MSXCPUInterface& msxcpu = MSXCPUInterface::instance();
		if (useGlobalSlot) {
			return msxcpu.peekMem(memoryAddress) +
			       msxcpu.peekMem(memoryAddress + 1) * 256;
		}
		if (!slot.vram) {
			return msxcpu.peekMemBySlot(memoryAddress, slot.ps[memoryAddress>>14], slot.ss[memoryAddress>>14], slot.direct) +
					msxcpu.peekMemBySlot(memoryAddress+1,slot.ps[(memoryAddress+1)>>14],slot.ss[(memoryAddress+1)>>14], slot.direct);
		}
	}
	if (!currentDevice) {
		return -1;
	}
	return currentDevice->readDebugData(currentCriterium);
}

bool ViewControl::linkToCriterium(DebugInterface* device, const string& criteria)
{
	currentDevice = device;
	if ((criteria[0] >= '0') && (criteria[0] <= '9')) {
		currentCriterium = atoi(criteria.c_str());
		return true;
	} else {
		currentCriterium = currentDevice->getRegisterNumber(criteria);
	}
	if (currentCriterium != 0xffff) {
		return true;
	}
	return false;
}

} // namespace openmsx
