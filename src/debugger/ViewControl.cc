// $Id$

#include "Views.hh"
#include "ViewControl.hh"
#include "MSXCPUInterface.hh"
#include "DebugInterface.hh"
#include <map>

ViewControl::ViewControl (MemoryView * view_)
{
	view = view_;
	currentDevice = NULL;
	currentRegister = "";
	registerIndirect = false;
	memoryIndirect = false;
	memoryAddress = 0;
	linked = NULL;
	struct MSXCPUInterface::SlotSelection * slots = MSXCPUInterface::instance()->getCurrentSlots();
	useGlobalSlot=false;
	for (int i=0;i<4;i++){
		slot.ps[i] = slots->primary[i];
		slot.ss[i] = slots->secondary[i];
//		slot.map[i] = mapper->getSelectedPage(i);
	}
	slot.vram = false;
	slot.direct = false;
}

ViewControl::~ViewControl ()
{
}

void ViewControl::setAddress(int address)
{
	view->setAddress(address);
	if (linked){
		linked->setAddress(address);
	}
}

int ViewControl::getAddress()
{
	if (memoryIndirect){
		MSXCPUInterface * msxcpu = MSXCPUInterface::instance();
		if (useGlobalSlot){
			return msxcpu->peekMem(memoryAddress)+256*msxcpu->peekMem(memoryAddress+1);
		}
		if (!slot.vram){
			return msxcpu->peekMemBySlot(memoryAddress, slot.ps[memoryAddress>>14], slot.ss[memoryAddress>>14], slot.direct) +
					msxcpu->peekMemBySlot(memoryAddress+1,slot.ps[(memoryAddress+1)>>14],slot.ss[(memoryAddress+1)>>14], slot.direct);
		}
	}
	if (!currentDevice) return -1;
	std::map < std::string, word > regMap;
	std::map < std::string, word >::iterator it;
	bool succes = currentDevice->getRegisters (regMap);
	if (succes){
		for (it = regMap.begin(); it != regMap.end();it++){
			if (it->first == currentRegister){
				return it->second;
			}
		}
	}
	return -1; //unable to find the right register
}

bool ViewControl::linkToRegisterName(DebugInterface * device, std::string regName)
{
	currentDevice = device;
	std::map < std::string, word > regMap;
	std::map < std::string, word >::iterator it;
	bool succes = currentDevice->getRegisters (regMap);
	if (succes){
		for (it = regMap.begin(); it != regMap.end();it++){
			if (it->first == currentRegister){
				return it->second;
			}
		}
	}
	currentRegister = regName;
	return true;

}
