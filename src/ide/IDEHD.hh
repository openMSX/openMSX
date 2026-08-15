#ifndef IDEHD_HH
#define IDEHD_HH

#include "AbstractIDEDevice.hh"
#include "HD.hh"

namespace openmsx {

class DeviceConfig;

class IDEHD final : public HD, public AbstractIDEDevice
{
public:
	explicit IDEHD(const DeviceConfig& config);
	IDEHD(const IDEHD&) = delete;
	IDEHD(IDEHD&&) = delete;
	IDEHD& operator=(const IDEHD&) = delete;
	IDEHD& operator=(IDEHD&&) = delete;
	~IDEHD() override = default;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// AbstractIDEDevice:
	[[nodiscard]] bool isPacketDevice() override;
	[[nodiscard]] std::string_view getDeviceName() override;
	void fillIdentifyBlock (AlignedBuffer& buffer) override;
	[[nodiscard]] unsigned readBlockStart(AlignedBuffer& buffer, unsigned count) override;
	void writeBlockComplete(AlignedBuffer& buffer, unsigned count) override;
	void executeCommand(uint8_t cmd) override;

private:
	unsigned transferSectorNumber = 0; // avoid UMR in serialize()
	const std::string devName;
};

} // namespace openmsx

#endif
