// $Id$

#ifndef __LAYOUTMANAGER_HH__
#define __LAYOUTMANAGER_HH__

#include "Views.hh"
#include <string>

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
		std::string currentRegister;
		bool registerIndirect;
		bool memoryIndirect;
		bool useGlobalSlot;
		dword memoryAddress;
		ViewControl * linked;
		struct DebugSlot slot;
		bool linkToRegisterName(DebugInterface * device, std::string regName);
};





#endif
