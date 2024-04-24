#ifndef TC8566AF_HH
#define TC8566AF_HH

#include "DynamicClock.hh"
#include "CRC16.hh"
#include "EmuTime.hh"
#include "Schedulable.hh"
#include "serialize_meta.hh"
#include <array>
#include <cstdint>
#include <span>

namespace openmsx {

class Scheduler;
class DiskDrive;
class MSXCliComm;

class TC8566AF final : public Schedulable
{
public:
	TC8566AF(Scheduler& scheduler, std::span<std::unique_ptr<DiskDrive>, 4>, MSXCliComm& cliComm,
	         EmuTime::param time);

	void reset(EmuTime::param time);
	[[nodiscard]] uint8_t peekDataPort(EmuTime::param time) const;
	uint8_t readDataPort(EmuTime::param time);
	[[nodiscard]] uint8_t peekStatus() const;
	uint8_t readStatus(EmuTime::param time);
	void writeControlReg0(uint8_t value, EmuTime::param time);
	void writeControlReg1(uint8_t value, EmuTime::param time);
	void writeDataPort(uint8_t value, EmuTime::param time);
	bool diskChanged(unsigned driveNum);
	[[nodiscard]] bool peekDiskChanged(unsigned driveNum) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// public for serialization
	enum class Command {
		UNKNOWN,
		READ_DATA,
		WRITE_DATA,
		WRITE_DELETED_DATA,
		READ_DELETED_DATA,
		READ_DIAGNOSTIC,
		READ_ID,
		FORMAT,
		SCAN_EQUAL,
		SCAN_LOW_OR_EQUAL,
		SCAN_HIGH_OR_EQUAL,
		SEEK,
		RECALIBRATE,
		SENSE_INTERRUPT_STATUS,
		SPECIFY,
		SENSE_DEVICE_STATUS,
	};
	enum class Phase {
		IDLE,
		COMMAND,
		DATA_TRANSFER,
		RESULT,
	};
	enum class Seek {
		IDLE,
		SEEK,
		RECALIBRATE
	};

private:
	// Schedulable
	void executeUntil(EmuTime::param time) override;

	[[nodiscard]] uint8_t executionPhasePeek(EmuTime::param time) const;
	uint8_t executionPhaseRead(EmuTime::param time);
	[[nodiscard]] uint8_t resultsPhasePeek() const;
	uint8_t resultsPhaseRead(EmuTime::param time);
	void idlePhaseWrite(uint8_t value, EmuTime::param time);
	void commandPhase1(uint8_t value);
	void commandPhaseWrite(uint8_t value, EmuTime::param time);
	void doSeek(int n);
	void executionPhaseWrite(uint8_t value, EmuTime::param time);
	void resultPhase(bool readId = false);
	void endCommand(EmuTime::param time);

	[[nodiscard]] bool isHeadLoaded(EmuTime::param time) const;
	[[nodiscard]] EmuDuration getHeadLoadDelay() const;
	[[nodiscard]] EmuDuration getHeadUnloadDelay() const;
	[[nodiscard]] EmuDuration getSeekDelay() const;

	[[nodiscard]] EmuTime locateSector(EmuTime::param time, bool readId);
	void startReadWriteSector(EmuTime::param time);
	void writeSector();
	void initTrackHeader(EmuTime::param time);
	void formatSector();
	void setDrqRate(unsigned trackLength);

private:
	MSXCliComm& cliComm;
	std::array<DiskDrive*, 4> drive;
	DynamicClock delayTime{EmuTime::zero()};

	// Before this time head is loaded, after this time it's unloaded. Set
	// to zero/infinity to force a (un)loaded head.
	EmuTime headUnloadTime = EmuTime::zero(); // head not loaded

	Command command;
	Phase phase;
	int phaseStep;

	//bool interrupt;

	unsigned dataAvailable = 0; // avoid UMR (on savestate)
	int dataCurrent = 0;
	CRC16 crc;

	uint8_t driveSelect;
	uint8_t mainStatus;
	uint8_t status0;
	uint8_t status1;
	uint8_t status2;
	uint8_t status3;
	uint8_t commandCode;

	uint8_t cylinderNumber;
	uint8_t headNumber;
	uint8_t sectorNumber;
	uint8_t number;
	uint8_t endOfTrack;
	uint8_t sectorsPerCylinder;
	uint8_t fillerByte;
	uint8_t gapLength;
	std::array<uint8_t, 2> specifyData; // filled in by SPECIFY command

	struct SeekInfo {
		EmuTime time = EmuTime::zero();
		uint8_t currentTrack = 0;
		uint8_t seekValue = 0;
		Seek state = Seek::IDLE;

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);
	};
	std::array<SeekInfo, 4> seekInfo;
};
SERIALIZE_CLASS_VERSION(TC8566AF, 7);

} // namespace openmsx

#endif
