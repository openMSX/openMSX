// $Id$

#ifndef __MSXSCCPLUSCART_HH__
#define __MSXSCCPLUSCART_HH__

#include "MSXMemDevice.hh"

namespace openmsx {

// forward declaration
class SCC;


class MSXSCCPlusCart : public MSXMemDevice
{
	public:
		MSXSCCPlusCart(Device *config, const EmuTime &time);
		virtual ~MSXSCCPlusCart();

		virtual void reset(const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);
		virtual const byte* getReadCacheLine(word start) const;
		virtual byte* getWriteCacheLine(word start) const;

	private:
		void setMapper(int regio, byte value);
		void setModeRegister(byte value);
		void checkEnable();
		
		SCC* scc;
		byte modeRegister;
		enum SCCEnable {EN_NONE, EN_SCC, EN_SCCPLUS} enable;
		byte memoryBank[0x20000];
		bool isRamSegment[4];
		bool isMapped[4];
		byte *internalMemoryBank[4];	// 4 blocks of 8kB starting at #4000
		byte mapper[4];
		byte mapperMask;
		bool lowRAM, highRAM; 
};

} // namespace openmsx

#endif //__MSXSCCPlusCart_HH__
