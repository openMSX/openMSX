// $Id$

#ifndef __IDEHD_HH__
#define __IDEHD_HH__

#include <memory>
#include "IDEDevice.hh"

namespace openmsx {

class XMLElement;
class File;

class IDEHD : public IDEDevice
{
public:
	IDEHD(const XMLElement& config, const EmuTime& time);
	virtual ~IDEHD();

	virtual void reset(const EmuTime& time);

	virtual word readData(const EmuTime& time);
	virtual byte readReg(nibble reg, const EmuTime& time);

	virtual void writeData(word value, const EmuTime& time);
	virtual void writeReg(nibble reg, byte value, const EmuTime& time);

private:
	void setError(byte error);
	unsigned getSectorNumber() const;
	unsigned getNumSectors() const;
	void executeCommand(byte cmd);
	void setTransferRead(bool status);
	void setTransferWrite(bool status);

	byte errorReg;
	byte sectorCountReg;
	byte sectorNumReg;
	byte cylinderLowReg;
	byte cylinderHighReg;
	byte devHeadReg;
	byte statusReg;
	byte featureReg;

	std::auto_ptr<File> file;
	int totalSectors;
	
	byte* buffer;
	bool transferRead;
	bool transferWrite;
	unsigned transferCount;
	word* transferPntr;
	unsigned transferSectorNumber;

	static byte identifyBlock[512];
};

} // namespace openmsx
#endif
