#ifndef IDECDROM_HH
#define IDECDROM_HH

#include "AbstractIDEDevice.hh"
#include "File.hh"
#include <bitset>
#include <memory>

namespace openmsx {

class DeviceConfig;
class CDXCommand;

class IDECDROM final : public AbstractIDEDevice
{
public:
	IDECDROM(const IDECDROM&) = delete;
	IDECDROM& operator=(const IDECDROM&) = delete;

	explicit IDECDROM(const DeviceConfig& config);
	~IDECDROM();

	void eject();
	void insert(const std::string& filename);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	// AbstractIDEDevice:
	bool isPacketDevice() override;
	const std::string& getDeviceName() override;
	void fillIdentifyBlock (AlignedBuffer& buffer) override;
	unsigned readBlockStart(AlignedBuffer& buffer, unsigned count) override;
	void readEnd() override;
	void writeBlockComplete(AlignedBuffer& buffer, unsigned count) override;
	void executeCommand(byte cmd) override;

private:
	// Flags for the interrupt reason register:
	/** Bus release: 0 = normal, 1 = bus release */
	static const byte REL = 0x04;
	/** I/O direction: 0 = host->device, 1 = device->host */
	static const byte I_O = 0x02;
	/** Command/data: 0 = data, 1 = command */
	static const byte C_D = 0x01;

	/** Indicates the start of a read data transfer performed in packets.
	  * @param count Total number of bytes to transfer.
	  */
	void startPacketReadTransfer(unsigned count);

	void executePacketCommand(AlignedBuffer& packet);

	std::string name;
	std::unique_ptr<CDXCommand> cdxCommand;
	File file;
	unsigned byteCountLimit;
	unsigned transferOffset;

	unsigned senseKey;

	bool readSectorData;

	// Removable Media Status Notification Feature Set
	bool remMedStatNotifEnabled;
	bool mediaChanged;

	static const unsigned MAX_CD = 26;
	using CDInUse = std::bitset<MAX_CD>;
	std::shared_ptr<CDInUse> cdInUse;

	friend class CDXCommand;
};

} // namespace openmsx

#endif
