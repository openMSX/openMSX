#ifndef DUMMYRS232DEVICE_HH
#define DUMMYRS232DEVICE_HH

#include "RS232Device.hh"

namespace openmsx {

class DummyRS232Device final : public RS232Device
{
public:
	void signal(EmuTime::param time) override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// SerialDataInterface (part)
	void recvByte(byte value, EmuTime::param time) override;
};

} // namespace openmsx

#endif
