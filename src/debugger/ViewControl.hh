// $Id$

#ifndef __LAYOUTMANAGER_HH__
#define __LAYOUTMANAGER_HH__

#include "DebugView.hh"
#include <string>

namespace openmsx {

class DebugInterface;

class ViewControl
{
	public:
		ViewControl (MemoryView * view_);
		~ViewControl ();
		void setAddress(int address);
		int getAddress();
	private:
		DebugInterface * currentDevice;
		MemoryView * view;
		word currentCriterium;
		bool indirect;
		bool useGlobalSlot;
		dword memoryAddress;
		ViewControl * linked;
		struct DebugSlot slot;
		bool linkToCriterium(DebugInterface * device, std::string regName);
};

} // namespace openmsx





#endif
