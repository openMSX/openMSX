/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/MB89352.h,v
** Revision: 1.4
** Date: 2007/03/28 17:35:35
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, white cat
*/

#ifndef MB89352_HH
#define MB89352_HH

#include "SCSI.hh"
#include "SCSIDevice.hh"
#include "AlignedBuffer.hh"
#include <memory>

namespace openmsx {

class DeviceConfig;

class MB89352
{
public:
	explicit MB89352(const DeviceConfig& config);
	~MB89352();

	void reset(bool scsireset);
	byte readRegister(byte reg);
	byte peekRegister(byte reg) const;
	byte readDREG();
	byte peekDREG() const;
	void writeRegister(byte reg, byte value);
	void writeDREG(byte value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void disconnect();
	void softReset();
	void setACKREQ(byte& value);
	void resetACKREQ();
	byte getSSTS() const;

	std::unique_ptr<SCSIDevice> dev[8];
	AlignedByteArray<SCSIDevice::BUFFER_SIZE> buffer; // buffer for transfer
	unsigned cdbIdx;                // cdb index
	unsigned bufIdx;                // buffer index
	int msgin;                      // Message In flag
	int counter;                    // read and written number of bytes
	                                // within the range in the buffer
	unsigned blockCounter;          // Number of blocks outside buffer
	                                // (512bytes / block)
	int tc;                         // counter for hardware transfer
	SCSI::Phase phase;              //
	SCSI::Phase nextPhase;          // for message system
	byte myId;                      // SPC SCSI ID 0..7
	byte targetId;                  // SCSI Device target ID 0..7
	byte regs[16];                  // SPC register
	bool rst;                       // SCSI bus reset signal
	byte atn;                       // SCSI bus attention signal
	bool isEnabled;                 // spc enable flag
	bool isBusy;                    // spc now working
	bool isTransfer;                // hardware transfer mode
	//TODO: bool devBusy;           // CDROM busy (buffer conflict prevention)
	byte cdb[12];                   // Command Descripter Block
};

} // namespace openmsx

#endif
