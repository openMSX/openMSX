#ifndef IDEHD_HH
#define IDEHD_HH

#include "HD.hh"
#include "AbstractIDEDevice.hh"

namespace openmsx {

class DeviceConfig;
class DiskManipulator;

class IDEHD final : public HD, public AbstractIDEDevice
{
public:
	IDEHD(const IDEHD&) = delete;
	IDEHD& operator=(const IDEHD&) = delete;

	explicit IDEHD(const DeviceConfig& config);
	~IDEHD();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// AbstractIDEDevice:
	bool isPacketDevice() override;
	const std::string& getDeviceName() override;
	void fillIdentifyBlock (AlignedBuffer& buffer) override;
	unsigned readBlockStart(AlignedBuffer& buffer, unsigned count) override;
	void writeBlockComplete(AlignedBuffer& buffer, unsigned count) override;
	void executeCommand(byte cmd) override;

	DiskManipulator& diskManipulator;
	unsigned transferSectorNumber;
};

} // namespace openmsx

#endif
