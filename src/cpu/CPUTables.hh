// $Id$

#ifndef __CPUTABLES_HH__
#define __CPUTABLES_HH__

#include "openmsx.hh"

namespace openmsx {

class CPUTables
{
public:
	// flag positions
	static const byte S_FLAG = 0x80;
	static const byte Z_FLAG = 0x40;
	static const byte Y_FLAG = 0x20;
	static const byte H_FLAG = 0x10;
	static const byte X_FLAG = 0x08;
	static const byte V_FLAG = 0x04;
	static const byte P_FLAG = V_FLAG;
	static const byte N_FLAG = 0x02;
	static const byte C_FLAG = 0x01;

protected:
	CPUTables();

	// flag-register tables
	// These are initialized at run-time
	static byte ZSTable[256];
	static byte ZSXYTable[256];
	static byte ZSPXYTable[256];
	static byte ZSPTable[256];
	static word DAATable[0x800];

private:
	/** Initialise the table contents.
	  */
	static void init();
};

} // namespace openmsx

#endif // __CPUTABLES_HH__
