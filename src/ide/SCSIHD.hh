/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/ScsiDevice.h,v
** Revision: 1.6
** Date: 2007-05-22 20:05:38 +0200 (Tue, 22 May 2007)
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, white cat
*/
#ifndef SCSIHD_HH
#define SCSIHD_HH

#include "HD.hh"
#include "SCSIDevice.hh"

namespace openmsx {

class DeviceConfig;

class SCSIHD final : public HD, public SCSIDevice
{
public:
	SCSIHD(const SCSIHD&) = delete;
	SCSIHD& operator=(const SCSIHD&) = delete;

	SCSIHD(const DeviceConfig& targetconfig,
	       AlignedBuffer& buf, unsigned mode);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SCSI Device
	void reset() override;
	bool isSelected() override;
	[[nodiscard]] unsigned executeCmd(const byte* cdb, SCSI::Phase& phase,
	                    unsigned& blocks) override;
	[[nodiscard]] unsigned executingCmd(SCSI::Phase& phase, unsigned& blocks) override;
	[[nodiscard]] byte getStatusCode() override;
	int msgOut(byte value) override;
	byte msgIn() override;
	void disconnect() override;
	void busReset() override;

	[[nodiscard]] unsigned dataIn(unsigned& blocks) override;
	[[nodiscard]] unsigned dataOut(unsigned& blocks) override;

	[[nodiscard]] unsigned inquiry();
	[[nodiscard]] unsigned modeSense();
	[[nodiscard]] unsigned requestSense();
	[[nodiscard]] bool checkReadOnly();
	[[nodiscard]] unsigned readCapacity();
	[[nodiscard]] bool checkAddress();
	[[nodiscard]] unsigned readSectors(unsigned& blocks);
	[[nodiscard]] unsigned writeSectors(unsigned& blocks);
	void formatUnit();

private:
	AlignedBuffer& buffer;

	const unsigned mode;

	unsigned keycode;      // Sense key, ASC, ASCQ
	unsigned currentSector;
	unsigned currentLength;

	const byte scsiId;     // SCSI ID 0..7
	bool unitAttention;    // Unit Attention (was: reset)
	byte message;
	byte lun;
	byte cdb[12];          // Command Descriptor Block
};

} // namespace openmsx

#endif
