#ifndef IDEHD_HH
#define IDEHD_HH

#include "HD.hh"
#include "AbstractIDEDevice.hh"
#include "noncopyable.hh"

namespace openmsx {

class DeviceConfig;
class DiskManipulator;

class IDEHD final : public HD, public AbstractIDEDevice, private noncopyable
{
public:
	explicit IDEHD(const DeviceConfig& config);
	virtual ~IDEHD();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// AbstractIDEDevice:
	virtual bool isPacketDevice();
	virtual const std::string& getDeviceName();
	virtual void fillIdentifyBlock (AlignedBuffer& buffer);
	virtual unsigned readBlockStart(AlignedBuffer& buffer, unsigned count);
	virtual void writeBlockComplete(AlignedBuffer& buffer, unsigned count);
	virtual void executeCommand(byte cmd);

	DiskManipulator& diskManipulator;
	unsigned transferSectorNumber;
};

} // namespace openmsx

#endif
