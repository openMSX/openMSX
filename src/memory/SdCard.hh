#ifndef SDCARD_HH
#define SDCARD_HH

#include "openmsx.hh"
#include "circular_buffer.hh"
#include "DiskImageUtils.hh"
#include <array>
#include <memory>

namespace openmsx {

class DeviceConfig;
class HD;

class SdCard
{
public:
	explicit SdCard(const DeviceConfig& config);
	~SdCard();

	byte transfer(byte value, bool cs);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

// private:
	enum class Mode {
		COMMAND,
		READ,
		MULTI_READ,
		WRITE,
		MULTI_WRITE
	};

private:
	void executeCommand();
	[[nodiscard]] byte readCurrentByteFromCurrentSector();

private:
	const std::unique_ptr<HD> hd; // can be nullptr

	std::array<byte, 6> cmdBuf;
	SectorBuffer sectorBuf;
	unsigned cmdIdx = 0;

	cb_queue<byte> responseQueue;

	byte transferDelayCounter = 0;
	Mode mode = Mode::COMMAND;
	unsigned currentSector = 0;
	int currentByteInSector = 0;
};

} // namespace openmsx

#endif
