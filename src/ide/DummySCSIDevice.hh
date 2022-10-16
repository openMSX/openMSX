#ifndef DUMMYSCSIDEVICE_HH
#define DUMMYSCSIDEVICE_HH

#include "SCSIDevice.hh"

namespace openmsx {

class DummySCSIDevice final : public SCSIDevice
{
public:
	void reset() override;
	[[nodiscard]] bool isSelected() override;
	[[nodiscard]] unsigned executeCmd(std::span<const uint8_t, 12> cdb, SCSI::Phase& phase,
	                                  unsigned& blocks) override;
	[[nodiscard]] unsigned executingCmd(SCSI::Phase& phase, unsigned& blocks) override;
	[[nodiscard]] uint8_t getStatusCode() override;
	int msgOut(uint8_t value) override;
	uint8_t msgIn() override;
	void disconnect() override;
	void busReset() override; // only used in MB89352 controller

	[[nodiscard]] unsigned dataIn(unsigned& blocks) override;
	[[nodiscard]] unsigned dataOut(unsigned& blocks) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
