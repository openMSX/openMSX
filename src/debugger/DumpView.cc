// $Id$

#include <cstdio>
#include "DumpView.hh"

namespace openmsx {

DumpView::DumpView(unsigned rows, unsigned columns, bool border)
	: MemoryView(rows, columns, border)
{
	numericSize = 1;
}

void DumpView::setAsciiDisplay(bool mode)
{
	displayAscii = mode;
}

void DumpView::setNumericSize (unsigned size)
{
	numericSize = size;
}

void DumpView::fill()
{
	// first calculated positions
	lines.clear();
	bool addresses = true;
	int spaceleft = columns;
	int space = (slot.direct || slot.vram ? 8 : 4); 
	// is there enough space for the addresses ?
	if (((space + 2) >= spaceleft) || (!displayAddress)) {
		addresses = false;
	} else {
		spaceleft -= space - 1;
	}
	
	// howmuch data can be displayed ?
	space = displayAscii ? (numericSize * 3) + 1 : (numericSize * 2) + 1; 
	int num = spaceleft / space;
	char hexbuffer[5];
	for (unsigned i = 0; i < rows; ++i) {
		string temp;
		if (addresses) {
			char hexbuffer[5];
			sprintf(hexbuffer, "%04X ", address + i * num);
			temp += hexbuffer;
		}
		string asciidisplay;
		char asciibuffer[2];
		byte val;
		for (int j = 0; j < num; j++) {
			val = readMemory(address + i * num + j);
			sprintf(hexbuffer, "%02X ", val);
			temp += hexbuffer;
			if ((val < 32) || (val > 126)) {
				val = '.';
			}
			sprintf(asciibuffer, "%c", val);
			asciidisplay += asciibuffer;
		}
		if (displayAscii) {
			temp += asciidisplay;
		}
		lines.push_back(temp);
	}
}

void DumpView::scroll(enum ScrollDirection direction, unsigned lines)
{
}

} // namespace openmsx
