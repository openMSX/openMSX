// $Id$

#include <cassert>
#include <algorithm>
#include <cstdio>
#include "DasmTables.hh"
#include "DisAsmView.hh"

DisAsmView::DisAsmView (int rows_, int columns_, bool border_):
	MemoryView (rows_, columns_, border_)
{
	displayBytes = true;
}

void DisAsmView::setByteDisplay (bool mode)
{
	displayBytes = mode;
}

void DisAsmView::fill ()
{
	lines.clear();
	std::string hexcodes;
	std::string disass;
	std::string result;
	dword adr = address;
	for (int i=0;i<rows;i++){
		result = "";
		byte length = createDisAsmText (adr, disass);
		if (displayAddress){
			char hexbuffer[5];
			sprintf (hexbuffer,"%04X ",adr);
			result += hexbuffer;
		}
		if (displayBytes){
			createByteText(adr, hexcodes, length);
			byte length = hexcodes.size();
			if (length<12){
				string temp(12,' ');
				hexcodes += temp.substr(length-1);
			}
			result += hexcodes;
		}
		transform (disass.begin(), disass.end(), disass.begin(), ::toupper);
		result += disass;
		adr += length;
		lines.push_back(result);
	}
}

void DisAsmView::scroll (enum ScrollDirection direction, int lines)
{
}

byte DisAsmView::createDisAsmText (dword address_, std::string &dest)
{
	dest = "";
	byte val;
	std::string S;
	char buf[15];
	dword i=address_;
	char Offset=0;
	std::string indexReg = "";
	val = readMemory(i);
	switch (val) {
		case 0xCB:
			i += 2;
			S=mnemonic_cb[val];
			break;
		case 0xED:
			i += 2;
			S=mnemonic_ed[val];
			break;
		case 0xDD:
			i++;
			indexReg="ix";
			val = readMemory (i);
			switch (val)
			{
				case 0xcb:
					Offset = readMemory(i + 1);
					S=mnemonic_xx_cb[readMemory(i + 2)];
					i += 3;
					break;
				default:
					Offset = val;
					S=mnemonic_xx[readMemory(i + 1)];
					i += 2;
					break;
			}
			break;
		case 0xFD:
			i++;
			indexReg="iy";
			val = readMemory (i);
			switch (val)
			{
				case 0xcb:
					Offset = readMemory(i + 1);
					S=mnemonic_xx_cb[readMemory(i + 2)];
					i += 3;
					break;
				default:
					S=mnemonic_xx[val];
					i++;
					break;
			}
			break;
		default:
			S=mnemonic_main[val];
			i++;
			break;
	}
	for (unsigned j = 0; j < S.length(); j++) {
		switch (S[j]) {
			case 'B':
				sprintf (buf,"#%02X",readMemory(i++));
				dest += buf;
				break;
			case 'R':
				sprintf (buf,"#%04X",(address_+2+(signed char)readMemory(i++))&0xFFFF);
				dest += buf;
				break;
			case 'W':
				sprintf (buf,"#%04X",readMemory(i++)+readMemory(i++)*256);
				dest += buf;
				break;
			case 'X':
				assert (indexReg == "");
				val = readMemory (i++);
				sprintf (buf,"(%s%c#%02X)",indexReg.c_str(), sign(val),abs(val));
				dest += buf;
				break;
			case 'Y':
				assert (indexReg == "");
				sprintf (buf,"(%s%c#%02X)",indexReg.c_str(), sign(Offset),abs(Offset));
				dest += buf;
				break;
			case 'I':
				dest += indexReg;
				break;
			case '!':
				sprintf (buf,"db     #ED,#%02X",readMemory(address_ + 1));
				dest += buf;
				return 2;
			case '@':
				sprintf (buf,"db     #%02X",readMemory(address_));
				dest += buf;
				return 1;
			case '#':
				sprintf (buf,"db     #%02X,#CB,#%02X",readMemory(address_),readMemory(address_ + 2));
				dest += buf;
				return 2;
			case ' ': {
				unsigned k = dest.length();
				if (k<5) k=6-k;
				else k=1;
				std::string temp (k, ' ');
				dest += temp;
				break;
			}
			default:
				buf[0]=S[j];
				buf[1]='\0';
				dest += buf;
				break;
		}
	}		
	return i - address_;
}

void DisAsmView::createByteText (dword address_, std::string & dest, byte size)
{
	char hexbuf[5];
	dest = "";
	for (unsigned i = 0; i < size; ++i){
		sprintf (hexbuf,"%02X ", readMemory(address_++));
		dest += hexbuf;
	}
}

byte DisAsmView::sign (byte data)
{
	return (data & 128) ? '-' : '+';
}

byte DisAsmView::abs (byte data)
{
	if (data & 128) return 256 - data;
	else return data;
}
