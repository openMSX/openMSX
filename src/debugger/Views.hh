// $Id$

#ifndef __VIEWS_HH__
#define __VIEWS_HH__

#include "openmsx.hh"
#include <string>
#include <vector>

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

class DebugView
{
	public:
		DebugView (int rows_, int columns_, bool border_);
		std::string getLine (int line);
		virtual ~DebugView ();
		virtual void resize (int rows, int columns);
		virtual void update()=0;
		virtual void fill ()=0;
	private:
		int cursorX;
		int cursorY;
	protected:
		std::vector<std::string> lines;
		int rows;
		int columns;
		bool border;
		void setCursor (int rows_, int columns_);
		void getCursor (int *rows_, int *columns_);
		void clear ();
		void deleteLine ();
		void addLine (std::string message="");

};

class MemoryView: public DebugView
{
	public:
		MemoryView (int rows, int columns, bool border);
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

class DumpView: public MemoryView
{
	public:
		DumpView (int rows, int columns, bool border);
		void setAsciiDisplay (bool mode);
		void setNumericSize (int size);
		void fill();
		void scroll (enum ScrollDirection direction, int lines);
	private:
		int numericSize;
		bool displayAscii;
};
#endif
