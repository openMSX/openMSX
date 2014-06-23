#ifndef SDCARD_HH
#define SDCARD_HH

#include "openmsx.hh"
#include "circular_buffer.hh"
#include <memory>
#include <string>

namespace openmsx {

class SRAM;
class DeviceConfig;

class SdCard
{
public:
	SdCard(const DeviceConfig& config, const std::string& name);
	~SdCard();

	byte transfer(byte value, bool cs);

private:
	void reset();
	void executeCommand();
	
	static const int SECTOR_SIZE = 512;

	enum Mode {
		COMMAND,
		READ,
		MULTI_READ,
		WRITE,
		MULTI_WRITE
	};
	std::unique_ptr<SRAM> ram;

	std::string name;

	byte cmdBuf[6];
	byte sectorBuf[SECTOR_SIZE];
	unsigned cmdIdx;

	cb_queue<byte> responseQueue;

	byte transferDelayCounter;
	Mode mode;
	unsigned currentSector;
	int currentByteInSector;
};

} // namespace openmsx

#endif
