// $Id$

#ifndef __DUMPVIEW_HH__
#define __DUMPVIEW_HH__

#include "MemoryView.hh"

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
