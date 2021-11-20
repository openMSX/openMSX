#ifndef NOWINDHOST_HH
#define NOWINDHOST_HH

#include "DiskImageUtils.hh"
#include "circular_buffer.hh"
#include "openmsx.hh"
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace openmsx {

class SectorAccessibleDisk;
class DiskContainer;

class NowindHost
{
public:
	using Drives = std::vector<std::unique_ptr<DiskContainer>>;

	explicit NowindHost(const Drives& drives);
	~NowindHost();

	// public for usb-host implementation
	[[nodiscard]] bool isDataAvailable() const;

	// read one byte of response-data from the host (msx <- pc)
	byte read();

	// like read(), but without side effects (doesn't consume the data)
	[[nodiscard]] byte peek() const;

	// Write one byte of command-data to the host   (msx -> pc)
	// Time parameter is in milliseconds. Emulators can pass emulation
	// time, usbhost can pass real time.
	void write(byte data, unsigned time);

	void setAllowOtherDiskroms(bool allow) { allowOtherDiskroms = allow; }
	[[nodiscard]] bool getAllowOtherDiskroms() const { return allowOtherDiskroms; }

	void setEnablePhantomDrives(bool enable) { enablePhantomDrives = enable; }
	[[nodiscard]] bool getEnablePhantomDrives() const { return enablePhantomDrives; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialization
	enum State {
		STATE_SYNC1,     // waiting for AF
		STATE_SYNC2,     // waiting for 05
		STATE_COMMAND,   // waiting for command (9 bytes)
		STATE_DISKREAD,  // waiting for AF07
		STATE_DISKWRITE, // waiting for AA<data>AA
		STATE_DEVOPEN,   // waiting for filename (11 bytes)
		STATE_IMAGE,     // waiting for filename
		STATE_MESSAGE,   // waiting for null-terminated message
	};

private:
	void msxReset();
	[[nodiscard]] SectorAccessibleDisk* getDisk() const;
	void executeCommand();

	void send(byte value);
	void send16(word value);
	void sendHeader();
	void purge();

	void DRIVES();
	void DSKCHG();
	void CHOICE();
	void INIENV();
	void setDateMSX();

	[[nodiscard]] unsigned getSectorAmount() const;
	[[nodiscard]] unsigned getStartSector() const;
	[[nodiscard]] unsigned getStartAddress() const;
	[[nodiscard]] unsigned getCurrentAddress() const;

	void diskReadInit(SectorAccessibleDisk& disk);
	void doDiskRead1();
	void doDiskRead2();
	void transferSectors(unsigned transferAddress, unsigned amount);
	void transferSectorsBackwards(unsigned transferAddress, unsigned amount);

	void diskWriteInit(SectorAccessibleDisk& disk);
	void doDiskWrite1();
	void doDiskWrite2();

	[[nodiscard]] unsigned getFCB() const;
	[[nodiscard]] std::string extractName(int begin, int end) const;
	unsigned readHelper1(unsigned dev, char* buffer);
	void readHelper2(unsigned len, const char* buffer);
	[[nodiscard]] int getDeviceNum() const;
	int getFreeDeviceNum();
	void deviceOpen();
	void deviceClose();
	void deviceWrite();
	void deviceRead();

	void callImage(const std::string& filename);

private:
	static constexpr unsigned MAX_DEVICES = 16;

	const Drives& drives;

	cb_queue<byte> hostToMsxFifo;

	struct {
		std::optional<std::fstream> fs; // not in use when fs == nullopt
		unsigned fcb;
	} devices[MAX_DEVICES];

	// state-machine
	std::vector<SectorBuffer> buffer;// work buffer for diskread/write
	unsigned lastTime;       // last time a byte was received from MSX
	State state;
	unsigned recvCount;      // how many bytes recv in this state
	unsigned transferred;    // progress within diskread/write
	unsigned retryCount;     // only used for diskread
	unsigned transferSize;   // size of current chunk
	byte cmdData[9];         // reg_[cbedlhfa] + cmd
	byte extraData[240 + 2]; // extra data for diskread/write

	byte romdisk;            // index of romdisk (255 = no romdisk)
	bool allowOtherDiskroms;
	bool enablePhantomDrives;
};

} // namespace openmsx

#endif // NOWINDHOST_HH
