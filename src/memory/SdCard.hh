#ifndef SDCARD_HH
#define SDCARD_HH

#include "openmsx.hh"
#include "circular_buffer.hh"
#include "DiskImageUtils.hh"
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
	enum Mode {
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

	byte cmdBuf[6];
	SectorBuffer sectorBuf;
	unsigned cmdIdx;

	cb_queue<byte> responseQueue;

	byte transferDelayCounter;
	Mode mode;
	unsigned currentSector;
	int currentByteInSector;
};

} // namespace openmsx

#endif
