// $Id$

#include "MemoryView.hh"
#include "ViewControl.hh"
#include "MSXMapperIO.hh"
#include "MSXCPUInterface.hh"


namespace openmsx {

MemoryView::MemoryView(unsigned rows, unsigned columns, bool border)
	: DebugView(rows, columns, border)
{
	// MSXMapperIO * mapper = MSXMapperIO::instance();
	struct MSXCPUInterface::SlotSelection* slots =
		MSXCPUInterface::instance().getCurrentSlots();
	useGlobalSlot = false;
	for (int i = 0; i < 4; i++) {
		slot.ps[i] = slots->primary[i];
		slot.ss[i] = slots->secondary[i];
	// slot.map[i] = mapper->getSelectedPage(i);
	}
	delete slots;
	viewControl = new ViewControl(this);
	int tempAddress = viewControl->getAddress();
	if (tempAddress == -1) {
		viewControl->setAddress(0);
	} else {
		viewControl->setAddress(tempAddress);
	}
	numericBase = 16; // hex by default
	displayAddress = true;
	cursorAddress = tempAddress;
}

MemoryView::~MemoryView()
{
	delete viewControl;
}

byte MemoryView::readMemory(dword address)
{
	MSXCPUInterface& msxcpu = MSXCPUInterface::instance();
	if (useGlobalSlot) {
		return msxcpu.peekMem(address);
	} else {
		byte page = address >> 14;
		byte primSlot = slot.ps[page];
		byte subSlot  = slot.ss[page];
		unsigned addr = (primSlot << 18) + (subSlot << 16) + address;
		return msxcpu.peekSlottedMem(addr);
	}
}

void MemoryView::setNumericBase(int base)
{
	numericBase = base;
}

ViewControl* MemoryView::getViewControl()
{
	return viewControl;
}

void MemoryView::notifyController()
{
	viewControl->setAddress(address);
}

void MemoryView::setAddressDisplay(bool mode)
{
	displayAddress = mode;
}

void MemoryView::setAddress(int address_)
{
	address = address_;
}

void MemoryView::setSlot(int page, int slot_, int subslot)
{
	assert((page < 4) && (slot_ < 4) && (subslot < 4));
	slot.ps[page] = slot_;
	if (slot.subslotted[page]) {
		slot.ss[page] = subslot;
	}
}

void MemoryView::update()
{
	int newAddress = viewControl->getAddress();
	if ((newAddress != -1) && ((unsigned)newAddress != address)) {
		address = newAddress;
		fill();
	}
}

} // namespace openmsx
