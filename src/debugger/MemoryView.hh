// $Id$

#ifndef __MEMORYVIEW_HH__
#define __MEMORYVIEW_HH__

#include "DebugView.hh"

namespace openmsx {

class ViewControl;

enum ScrollDirection {SCR_UP, SCR_DOWN, SCR_LEFT, SCR_RIGHT};
struct DebugSlot
{
	int ps[4];
	int ss[4];
	bool subslotted[4];
	bool vram; // due to change to a more global approach
	bool direct; // mapper direct 
};

class MemoryView: public DebugView
{
	public:
		MemoryView(int rows, int columns, bool border);
		virtual ~MemoryView();
		
		void setAddressDisplay (bool mode); 
		void setNumericBase (int base_);
		void setAddress (int address_);
		void setSlot (int page, int slot=0, int subslot=0, bool direct=false);
		void update();
		ViewControl * getViewControl();
		virtual void scroll (enum ScrollDirection direction, int lines)=0;
		virtual void fill ()=0;
		void notifyController ();
		byte readMemory (dword address_);
		
		static const int SLOTVRAM = 4;
		static const int SLOTDIRECT = 5;
	private:
		
	protected:
		bool useGlobalSlot;
		DebugSlot slot;
		ViewControl * viewControl;
		dword address;
		dword cursorAddress;
		int numericBase;
		bool displayAddress;
};

} // namespace openmsx

#endif
