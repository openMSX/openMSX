// $Id$

#ifndef __IDEHD_HH__
#define __IDEHD_HH__

#include "IDEDevice.hh"

namespace openmsx {

class Config;
class File;


class IDEHD : public IDEDevice
{
	public:
		IDEHD(Config *config, const EmuTime &time);
		virtual ~IDEHD();
	
		virtual void reset(const EmuTime &time);
	
		virtual word readData(const EmuTime &time);
		virtual byte readReg(nibble reg, const EmuTime &time);

		virtual void writeData(word value, const EmuTime &time);
		virtual void writeReg(nibble reg, byte value, const EmuTime &time);
	
	private:
		void setError(byte error);
		int getSectorNumber();
		int getNumSectors();
		void executeCommand(byte cmd);

		byte errorReg;
		byte sectorCountReg;
		byte sectorNumReg;
		byte cylinderLowReg;
		byte cylinderHighReg;
		byte devHeadReg;
		byte statusReg;
		byte featureReg;
	
		File* file;
		int totalSectors;
		
		byte* buffer;
		bool transferRead;
		bool transferWrite;
		int transferCount;
		word* transferPntr;
		int transferSectorNumber;
		int transferNumSectors;

		static byte identifyBlock[512];
};

} // namespace openmsx
#endif
