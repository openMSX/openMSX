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
};

class MemoryView: public DebugView
{
public:
	MemoryView(unsigned rows, unsigned columns, bool border);
	virtual ~MemoryView();
	
	void setAddressDisplay(bool mode); 
	void setNumericBase(int base);
	void setAddress(int address);
	void setSlot(int page, int slot = 0, int subslot = 0);
	void update();
	ViewControl* getViewControl();
	virtual void scroll(enum ScrollDirection direction, unsigned lines) = 0;
	virtual void fill() = 0;
	void notifyController();
	byte readMemory(dword address);
	
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
