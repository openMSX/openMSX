// $Id$

#include "Views.hh"
#include "ViewControl.hh"
#include "MSXMapperIO.hh"
#include "MSXCPUInterface.hh"
#include <iostream>

DebugView::DebugView (int rows_, int columns_, bool border_):
			rows(rows_),columns(columns_),border(border_)
{
	cursorX = cursorY = 0;
}

DebugView::~DebugView ()
{
}

void DebugView::resize(int rows_,int columns_)
{
	if ((rows_ < 1) || (columns_ < 1)) return;
	while (rows < rows_){
		deleteLine();
	}
	while (rows > rows_){
		addLine();
	}
	if (columns_ < columns){
		for (int i=0;i<rows;i++){
			lines[i]=lines[i].substr(0,columns_);
		}
	}
		columns = columns_;
	cursorX = (cursorX > columns ? columns : cursorX);
	cursorY = (cursorY > rows ? rows : cursorY);
	fill();
}

void DebugView::getCursor (int *cursorx, int *cursory)
{
	*cursorx = cursorX;
	*cursory = cursorY;
}

void DebugView::setCursor (const int cursorx, const int cursory)
{
	cursorX = cursorx;
	cursorY = cursory;
}

void DebugView::clear ()
{
	for (int i=0;i<rows;i++){
			lines[i]="";
	}
}

void DebugView::deleteLine()
{
	lines.pop_back();
}

void DebugView::addLine(std::string message)
{
	lines.push_back(message);
}

std::string DebugView::getLine (int line)
{
	return lines[line];
}

MemoryView::MemoryView (int rows_, int columns_, bool border_):
		DebugView (rows_, columns_, border_)
{
//	MSXMapperIO * mapper = MSXMapperIO::instance();
	struct MSXCPUInterface::SlotSelection * slots = MSXCPUInterface::instance()->getCurrentSlots();
	useGlobalSlot=false;
	for (int i=0;i<4;i++){
		slot.ps[i] = slots->primary[i];
		slot.ss[i] = slots->secondary[i];
//		slot.map[i] = mapper->getSelectedPage(i);
	}
	slot.vram = false;
	slot.direct = false;
	viewControl = new ViewControl (this);
	int tempAddress = viewControl->getAddress();
	if (tempAddress == -1){
		viewControl->setAddress(0);
	}
	else{
		viewControl->setAddress(tempAddress);
	}
	numericBase = 16; // hex by default
	displayAddress = true;
	cursorAddress = tempAddress;
}

byte MemoryView::readMemory (dword address_)
{
	MSXCPUInterface * msxcpu = MSXCPUInterface::instance();
	if (useGlobalSlot){
		return msxcpu->peekMem(address_);
	}
	if (!slot.vram){
		return msxcpu->peekMemBySlot(address_, slot.ps[address_>>14], slot.ss[address>>14], slot.direct);
	}
	return 0;
}

void MemoryView::setNumericBase (int base)
{
	numericBase = base;
}

ViewControl * MemoryView::getViewControl ()
{
	return viewControl;
}

void MemoryView::notifyController ()
{
	viewControl->setAddress(address);
}

void MemoryView::setAddressDisplay (bool mode)
{
	displayAddress = mode;
}

void MemoryView::setAddress (int address_)
{
	address = address_;
}

void MemoryView::setSlot (int page, int slot_, int subslot, bool direct)
{
	if (slot_ == SLOTVRAM){
		slot.vram = true;
	}
	else if (slot_ == SLOTDIRECT){
		slot.direct = true;
	}
	else{
		if ((page>3) || (slot_<4) || (subslot<4)) return; // this can't be right
		slot.ps[page] = slot_;
		if (slot.subslotted[page]){
			slot.ss[page] = subslot;
		}
	}
}

void MemoryView::update ()
{
	int newAddress = viewControl->getAddress();
	if ((newAddress != -1) && ((unsigned)newAddress != address)){
		address = newAddress;
		fill();
	}
}

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
