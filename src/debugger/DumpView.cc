// $Id$

#include "DumpView.hh"

DumpView::DumpView (int rows_, int columns_, bool border_):
	MemoryView (rows_, columns_, border_)
{
	numericSize=1;
}

void DumpView::setAsciiDisplay(bool mode)
{
	displayAscii = mode;
}

void DumpView::setNumericSize (int size)
{
	numericSize = size;
}

void DumpView::fill()
{
	// first calculated positions
	lines.clear();
	std::string temp;
	int num;
	bool addresses=true;
	int spaceleft = columns;
	int space = (slot.direct || slot.vram ? 8 : 4); 
	// is there enough space for the addresses ?
	if (((space + 2) >= spaceleft) || (!displayAddress)){
		addresses = false;
	}
	else {
	spaceleft -= space - 1;
	}
	
	// howmuch data can be displayed ?
	space = (displayAscii ? (numericSize * 3)+1 : (numericSize * 2)+1); 
	num = spaceleft / space;
	char hexbuffer[5];
	for (int i=0;i<rows;i++){
		temp = "";
		if (addresses) {
			char hexbuffer[5];
			sprintf (hexbuffer,"%04X ",address + i*num);
			temp += hexbuffer;
		}
		std::string asciidisplay;
		char asciibuffer[2];
		byte val;
		for (int j=0; j<num; j++){
			val = readMemory (address + i*num + j);
			sprintf (hexbuffer,"%02X ",val);
			temp += hexbuffer;
			if ((val <32) || (val > 126)) val='.';
			sprintf (asciibuffer,"%c",val);
			asciidisplay += asciibuffer;
		}
		if (displayAscii){
			temp += asciidisplay;
		}
		lines.push_back(temp);
	}
}

void DumpView::scroll (enum ScrollDirection direction, int lines)
{
}
