// $Id$

#ifndef __MSXSCCPLUSCART_HH__
#define __MSXSCCPLUSCART_HH__

#include <iostream>
#include <fstream>
#include <string>
#include "msxconfig.hh"
#include "MSXDevice.hh"
#include "MSXMemDevice.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include "Scheduler.hh"
#include "SoundDevice.hh"
#include "Mixer.hh"
#include "SCC.hh"

class MSXSCCPlusCart : public MSXMemDevice
{
	public:
		MSXSCCPlusCart(MSXConfig::Device *config, const EmuTime &time);
		~MSXSCCPlusCart();

		byte readMem(word address, const EmuTime &time);
		void writeMem(word address, byte value, const EmuTime &time);

		void reset(const EmuTime &time);

	protected:
		MSXConfig::Device *deviceConfig;
		const std::string* deviceName;


	private:
		void setModeRegister(byte value);
		void checkEnable();
		SCC* cartridgeSCC;

		byte scc_banked;
		byte ModeRegister;


		unsigned enable;

		byte* memoryBank;
		byte mapperMask;
		bool isRamSegment[4];
		byte internalMapper[4];
      		byte *internalMemoryBank[4]; // 4 blocks of 8kB starting at #4000
};

#endif //__MSXSCCPlusCart_HH__

