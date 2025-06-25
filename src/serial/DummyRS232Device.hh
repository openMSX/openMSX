#ifndef DUMMYRS232DEVICE_HH
#define DUMMYRS232DEVICE_HH

#include "RS232Device.hh"

namespace openmsx {

class DummyRS232Device final : public RS232Device
{
public:
	void signal(EmuTime time) override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

	// SerialDataInterface (part)
	void recvByte(uint8_t value, EmuTime time) override;
};

} // namespace openmsx

#endif
