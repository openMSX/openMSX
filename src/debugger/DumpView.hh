// $Id$

#ifndef __DUMPVIEW_HH__
#define __DUMPVIEW_HH__

#include "MemoryView.hh"

namespace openmsx {

class DumpView: public MemoryView
{
	public:
		DumpView(unsigned rows, unsigned columns, bool border);
		void setAsciiDisplay(bool mode);
		void setNumericSize(unsigned size);
		void fill();
		void scroll(enum ScrollDirection direction, unsigned lines);

	private:
		unsigned numericSize;
		bool displayAscii;
};

} // namespace openmsx

#endif
