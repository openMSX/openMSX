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
#include <array>
#include <memory>

namespace openmsx {

class DeviceConfig;

class MB89352
{
public:
	explicit MB89352(const DeviceConfig& config);

	void reset(bool scsiReset);
	[[nodiscard]] uint8_t readRegister(uint8_t reg);
	[[nodiscard]] uint8_t peekRegister(uint8_t reg) const;
	[[nodiscard]] uint8_t readDREG();
	[[nodiscard]] uint8_t peekDREG() const;
	void writeRegister(uint8_t reg, uint8_t value);
	void writeDREG(uint8_t value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void disconnect();
	void softReset();
	void setACKREQ(uint8_t& value);
	void resetACKREQ();
	[[nodiscard]] uint8_t getSSTS() const;

private:
	static constexpr unsigned MAX_DEV = 8;

	std::unique_ptr<SCSIDevice> dev[MAX_DEV];
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
	uint8_t myId;                   // SPC SCSI ID 0..7
	uint8_t targetId;               // SCSI Device target ID 0..7
	std::array<uint8_t, 16> regs;   // SPC register
	bool rst;                       // SCSI bus reset signal
	uint8_t atn;                    // SCSI bus attention signal
	bool isEnabled;                 // spc enable flag
	bool isBusy;                    // spc now working
	bool isTransfer;                // hardware transfer mode
	//TODO: bool devBusy;           // CD-ROM busy (buffer conflict prevention)
	std::array<uint8_t, 12> cdb;    // Command Descriptor Block
};

} // namespace openmsx

#endif
