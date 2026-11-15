#ifndef SDCARD_HH
#define SDCARD_HH

#include "DiskImageUtils.hh"

#include "circular_buffer.hh"
#include "serialize_meta.hh"

#include <array>
#include <cstdint>
#include <memory>

namespace openmsx {

class DeviceConfig;
class HD;

class SdCard
{
public:
	explicit SdCard(const DeviceConfig& config);
	~SdCard();

	uint8_t transfer(uint8_t value, bool cs);

	[[nodiscard]] bool isPresent() const;
	[[nodiscard]] bool isWriteProtected() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

// private:
	enum class Mode : uint8_t {
		COMMAND,
		READ,
		MULTI_READ,
		WRITE,
		MULTI_WRITE
	};

private:
	void executeCommand();
	void checkCardChanged();
	[[nodiscard]] uint8_t readCurrentByteFromCurrentSector();

private:
	const std::unique_ptr<HD> hd; // can be nullptr

	std::array<uint8_t, 6> cmdBuf;
	SectorBuffer sectorBuf;
	unsigned cmdIdx = 0;

	cb_queue<uint8_t> responseQueue;

	uint8_t transferDelayCounter = 0;
	Mode mode = Mode::COMMAND;
	unsigned currentSector = 0;
	int currentByteInSector = 0;

	// A freshly inserted card isn't in SPI mode yet: it ignores everything
	// until CMD0. Drivers rely on that to notice a card change.
	bool waitingForCmd0 = false;
	unsigned lastChangeCount = 0; // last seen HD::getChangeCount()
};
SERIALIZE_CLASS_VERSION(SdCard, 2);

} // namespace openmsx

#endif
