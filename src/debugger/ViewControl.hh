// $Id$

#ifndef __LAYOUTMANAGER_HH__
#define __LAYOUTMANAGER_HH__

#include "DebugView.hh"
#include <string>

using std::string;

namespace openmsx {

class DebugInterface;

class ViewControl
{
	public:
		ViewControl(MemoryView* view);
		~ViewControl();

		void setAddress(int address);
		int getAddress() const;

	private:
		bool linkToCriterium(DebugInterface* device, const string& regName);

		DebugInterface* currentDevice;
		MemoryView* view;
		word currentCriterium;
		bool indirect;
		bool useGlobalSlot;
		dword memoryAddress;
		ViewControl* linked;
		struct DebugSlot slot;
};

} // namespace openmsx





#endif
