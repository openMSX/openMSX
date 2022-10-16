#ifndef SCSIDEVICE_HH
#define SCSIDEVICE_HH

#include "SCSI.hh"
#include <span>

namespace openmsx {

class SCSIDevice
{
public:
	static constexpr unsigned BIT_SCSI2          = 0x0001;
	static constexpr unsigned BIT_SCSI2_ONLY     = 0x0002;
	static constexpr unsigned BIT_SCSI3          = 0x0004;

	static constexpr unsigned MODE_SCSI1         = 0x0000;
	static constexpr unsigned MODE_SCSI2         = 0x0003;
	static constexpr unsigned MODE_SCSI3         = 0x0005;
	static constexpr unsigned MODE_UNITATTENTION = 0x0008; // report unit attention
	static constexpr unsigned MODE_MEGASCSI      = 0x0010; // report disk change when call of
	                                              // 'test unit ready'
	static constexpr unsigned MODE_NOVAXIS       = 0x0100;

	static constexpr unsigned BUFFER_SIZE   = 0x10000; // 64KB

	virtual ~SCSIDevice() = default;

	virtual void reset() = 0;
	virtual bool isSelected() = 0;
	[[nodiscard]] virtual unsigned executeCmd(std::span<const uint8_t, 12> cdb, SCSI::Phase& phase,
	                                          unsigned& blocks) = 0;
	[[nodiscard]] virtual unsigned executingCmd(SCSI::Phase& phase, unsigned& blocks) = 0;
	[[nodiscard]] virtual uint8_t getStatusCode() = 0;
	virtual int msgOut(uint8_t value) = 0;
	virtual uint8_t msgIn() = 0;
	virtual void disconnect() = 0;
	virtual void busReset() = 0; // only used in MB89352 controller

	[[nodiscard]] virtual unsigned dataIn(unsigned& blocks) = 0;
	[[nodiscard]] virtual unsigned dataOut(unsigned& blocks) = 0;
};

} // namespace openmsx

#endif
