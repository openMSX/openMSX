// $Id$

#ifndef __MSXSCCPLUSCART_HH__
#define __MSXSCCPLUSCART_HH__

#ifndef VERSION
#include "config.h"
#endif

#ifndef DONT_WANT_SCC

#include "MSXMemDevice.hh"

// forward declaration
class SCC;


class MSXSCCPlusCart : public MSXMemDevice
{
	public:
		MSXSCCPlusCart(MSXConfig::Device *config, const EmuTime &time);
		virtual ~MSXSCCPlusCart();

		virtual void reset(const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);
		virtual byte* getReadCacheLine(word start);
		virtual byte* getWriteCacheLine(word start);

	private:
		void setMapper(int regio, byte value);
		void setModeRegister(byte value);
		void checkEnable();
		
		SCC* cartridgeSCC;
		byte modeRegister;
		enum SCCEnable {EN_NONE, EN_SCC, EN_SCCPLUS} enable;
		byte* memoryBank;
		bool isRamSegment[4];
		byte *internalMemoryBank[4];	// 4 blocks of 8kB starting at #4000
		byte mapper[4];
};

#endif // ndef DONT_WANT_SCC

#endif //__MSXSCCPlusCart_HH__
