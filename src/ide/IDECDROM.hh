#ifndef IDECDROM_HH
#define IDECDROM_HH

#include "AbstractIDEDevice.hh"
#include "File.hh"
#include "RecordedCommand.hh"
#include <bitset>
#include <memory>
#include <optional>

namespace openmsx {

class DeviceConfig;
class IDECDROM;

class CDXCommand final : public RecordedCommand
{
public:
	CDXCommand(CommandController& commandController,
	           StateChangeDistributor& stateChangeDistributor,
	           Scheduler& scheduler, IDECDROM& cd);
	void execute(std::span<const TclObject> tokens,
		TclObject& result, EmuTime::param time) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	IDECDROM& cd;
};

class IDECDROM final : public AbstractIDEDevice
{
public:
	IDECDROM(const IDECDROM&) = delete;
	IDECDROM& operator=(const IDECDROM&) = delete;

	explicit IDECDROM(const DeviceConfig& config);
	~IDECDROM() override;

	void eject();
	void insert(const std::string& filename);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	// AbstractIDEDevice:
	[[nodiscard]] bool isPacketDevice() override;
	[[nodiscard]] std::string_view getDeviceName() override;
	void fillIdentifyBlock (AlignedBuffer& buffer) override;
	[[nodiscard]] unsigned readBlockStart(AlignedBuffer& buffer, unsigned count) override;
	void readEnd() override;
	void writeBlockComplete(AlignedBuffer& buffer, unsigned count) override;
	void executeCommand(byte cmd) override;

private:
	// Flags for the interrupt reason register:
	/** Bus release: 0 = normal, 1 = bus release */
	static constexpr byte REL = 0x04;
	/** I/O direction: 0 = host->device, 1 = device->host */
	static constexpr byte I_O = 0x02;
	/** Command/data: 0 = data, 1 = command */
	static constexpr byte C_D = 0x01;

	/** Indicates the start of a read data transfer performed in packets.
	  * @param count Total number of bytes to transfer.
	  */
	void startPacketReadTransfer(unsigned count);

	void executePacketCommand(AlignedBuffer& packet);

	std::string name;
	std::optional<CDXCommand> cdxCommand; // delayed init
	File file;
	unsigned byteCountLimit;
	unsigned transferOffset;

	unsigned senseKey;

	bool readSectorData;

	// Removable Media Status Notification Feature Set
	bool remMedStatNotifEnabled;
	bool mediaChanged;

	static constexpr unsigned MAX_CD = 26;
	using CDInUse = std::bitset<MAX_CD>;
	std::shared_ptr<CDInUse> cdInUse;

	friend class CDXCommand;
};

} // namespace openmsx

#endif
