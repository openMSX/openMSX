// $Id$

#ifndef __DISASMVIEW_HH__
#define __DISASMVIEW_HH__

#include "MemoryView.hh"
#include <string>

using std::string;

class DisAsmView: public MemoryView
{
	public:
		DisAsmView (int rows, int columns, bool border);
		void setByteDisplay (bool mode);
		
	void fill();
		void scroll (enum ScrollDirection direction, int lines);
	private:
		bool displayBytes;
		bool bytesInFront;
		byte sign (byte data);
		byte abs (byte data);
		byte createDisAsmText (dword address, std::string & dest);
		void createByteText (dword address, std::string & dest, byte size);
};

#endif
