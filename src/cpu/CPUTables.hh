// $Id$

#ifndef __CPUTABLES_HH__
#define __CPUTABLES_HH__

#include "openmsx.hh"


namespace openmsx {

class CPUTables {
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
	// flag-register tables
	// These are initialized at run-time
	static byte ZSTable[256];
	static byte ZSXYTable[256];
	static byte ZSPXYTable[256];
	static byte ZSPTable[256];

	static const word DAATable[0x800];

	/** tmp1 value for ini/inir/outi/otir for [C.1-0][io.1-0]
	  */
	static const byte irep_tmp1[4][4];

	/** tmp1 value for ind/indr/outd/otdr for [C.1-0][io.1-0]
	  */
	static const byte drep_tmp1[4][4];

	/** tmp2 value for all in/out repeated opcodes for B.7-0
	  */
	static const byte breg_tmp2[256];

	/** Forces table initialisation.
	  */
	CPUTables() {
		init();
	}
	virtual ~CPUTables() {}

private:
	/** Initialise the table contents.
	  * Some tables are initialised at run time,
	  * so it is essential to call this method before using any tables.
	  */
	static void init();

};

} // namespace openmsx

#endif // __CPUTABLES_HH__
