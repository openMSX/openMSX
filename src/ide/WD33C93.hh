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
#include <memory>

namespace openmsx {

class DeviceConfig;

class WD33C93
{
public:
	explicit WD33C93(const DeviceConfig& config);

	void reset(bool scsireset);

	[[nodiscard]] byte readAuxStatus();
	[[nodiscard]] byte readCtrl();
	[[nodiscard]] byte peekAuxStatus() const;
	[[nodiscard]] byte peekCtrl() const;
	void writeAdr(byte value);
	void writeCtrl(byte value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void disconnect();
	void execCmd(byte value);

private:
	AlignedByteArray<SCSIDevice::BUFFER_SIZE> buffer;
	std::unique_ptr<SCSIDevice> dev[8];
	unsigned bufIdx;
	int counter;
	unsigned blockCounter;
	int tc;
	SCSI::Phase phase;
	byte myId;
	byte targetId;
	byte regs[32];
	byte latch;
	bool devBusy;
};

} // namespace openmsx

#endif
