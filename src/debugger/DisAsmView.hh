// $Id$

#ifndef __DISASMVIEW_HH__
#define __DISASMVIEW_HH__

#include "MemoryView.hh"
#include <string>

using std::string;


namespace openmsx {

class DisAsmView: public MemoryView
{
public:
	DisAsmView(unsigned rows, unsigned columns, bool border);
	void setByteDisplay(bool mode);

	void fill();
	void scroll(enum ScrollDirection direction, unsigned lines);

private:
	byte sign(byte data);
	byte abs(byte data);
	byte createDisAsmText(dword address, string& dest);
	void createByteText(dword address, string& dest, byte size);

	bool displayBytes;
	bool bytesInFront;
};

} // namespace openmsx

#endif // __DISASMVIEW_HH__
