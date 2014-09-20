#ifndef DUMMYSCSIDEVICE_HH
#define DUMMYSCSIDEVICE_HH

#include "SCSIDevice.hh"

namespace openmsx {

class DummySCSIDevice final : public SCSIDevice
{
public:
	void reset() override;
	bool isSelected() override;
	unsigned executeCmd(const byte* cdb, SCSI::Phase& phase,
	                    unsigned& blocks) override;
	unsigned executingCmd(SCSI::Phase& phase, unsigned& blocks) override;
	byte getStatusCode() override;
	int msgOut(byte value) override;
	byte msgIn() override;
	void disconnect() override;
	void busReset() override; // only used in MB89352 controller

	unsigned dataIn(unsigned& blocks) override;
	unsigned dataOut(unsigned& blocks) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
