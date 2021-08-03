#ifndef SDCARD_HH
#define SDCARD_HH

#include "openmsx.hh"
#include "circular_buffer.hh"
#include "DiskImageUtils.hh"
#include "HD.hh"
#include <optional>

namespace openmsx {

class DeviceConfig;

class SdCard
{
public:
	explicit SdCard(const DeviceConfig& config);

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
	std::optional<HD> hd; // can be nullopt

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
