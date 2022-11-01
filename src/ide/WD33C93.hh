/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/wd33c93.h,v
** Revision: 1.6
** Date: 2007/03/22 10:55:08
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, Ricardo Bittencourt, white cat
*/

#ifndef WD33C93_HH
#define WD33C93_HH

#include "SCSI.hh"
#include "SCSIDevice.hh"
#include "AlignedBuffer.hh"
#include <array>
#include <memory>

namespace openmsx {

class DeviceConfig;

class WD33C93
{
public:
	explicit WD33C93(const DeviceConfig& config);

	void reset(bool scsiReset);

	[[nodiscard]] uint8_t readAuxStatus();
	[[nodiscard]] uint8_t readCtrl();
	[[nodiscard]] uint8_t peekAuxStatus() const;
	[[nodiscard]] uint8_t peekCtrl() const;
	void writeAdr(uint8_t value);
	void writeCtrl(uint8_t value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void disconnect();
	void execCmd(uint8_t value);

private:
	AlignedByteArray<SCSIDevice::BUFFER_SIZE> buffer;
	std::array<std::unique_ptr<SCSIDevice>, 8> dev;
	unsigned bufIdx;
	unsigned counter = 0;
	unsigned blockCounter = 0;
	int tc;
	SCSI::Phase phase;
	uint8_t myId;
	uint8_t targetId = 0;
	std::array<uint8_t, 32> regs;
	uint8_t latch;
	bool devBusy = false;
};

} // namespace openmsx

#endif
