/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/ScsiDevice.h,v
** Revision: 1.6
** Date: 2007-05-22 20:05:38 +0200 (Tue, 22 May 2007)
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, white cat
*/
#ifndef SCSILS120_HH
#define SCSILS120_HH

#include "SCSIDevice.hh"

#include "DiskContainer.hh"
#include "File.hh"
#include "MSXMotherBoard.hh"
#include "RecordedCommand.hh"
#include "SectorAccessibleDisk.hh"

#include <array>
#include <bitset>
#include <optional>

namespace openmsx {

class DeviceConfig;
class SCSILS120;

class LSXCommand final : public RecordedCommand
{
public:
	LSXCommand(CommandController& commandController,
	           StateChangeDistributor& stateChangeDistributor,
	           Scheduler& scheduler, SCSILS120& ls);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result, EmuTime time) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	SCSILS120& ls;
};

class SCSILS120 final : public SCSIDevice, public SectorAccessibleDisk
                      , public DiskContainer, public MediaProvider
{
public:
	SCSILS120(const DeviceConfig& targetConfig,
	          AlignedBuffer& buf, unsigned mode);
	SCSILS120(const SCSILS120&) = delete;
	SCSILS120(SCSILS120&&) = delete;
	SCSILS120 operator=(const SCSILS120&) = delete;
	SCSILS120 operator=(SCSILS120&&) = delete;
	~SCSILS120() override;

	// MediaInfoProvider
	void getMediaInfo(TclObject& result) override;
	void setMedia(const TclObject& info, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SectorAccessibleDisk:
	void readSectorsImpl(
		std::span<SectorBuffer> buffers, size_t startSector) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] size_t getNbSectorsImpl() override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;
	[[nodiscard]] Sha1Sum getSha1SumImpl(FilePool& filePool) override;

	// DiskContainer:
	[[nodiscard]] SectorAccessibleDisk* getSectorAccessibleDisk() override;
	[[nodiscard]] std::string_view getContainerName() const override;
	[[nodiscard]] bool diskChanged() override;
	int insertDisk(const std::string& filename) override;

	// SCSI Device
	void reset() override;
	[[nodiscard]] bool isSelected() override;
	[[nodiscard]] unsigned executeCmd(std::span<const uint8_t, 12> cdb, SCSI::Phase& phase, unsigned& blocks) override;
	[[nodiscard]] unsigned executingCmd(SCSI::Phase& phase, unsigned& blocks) override;
	[[nodiscard]] uint8_t getStatusCode() override;
	int msgOut(uint8_t value) override;
	uint8_t msgIn() override;
	void disconnect() override;
	void busReset() override;
	[[nodiscard]] unsigned dataIn(unsigned& blocks) override;
	[[nodiscard]] unsigned dataOut(unsigned& blocks) override;

	void eject();
	void insert(zstring_view filename);

	[[nodiscard]] bool getReady();
	void testUnitReady();
	void startStopUnit();
	[[nodiscard]] unsigned inquiry();
	[[nodiscard]] unsigned modeSense();
	[[nodiscard]] unsigned requestSense();
	[[nodiscard]] bool checkReadOnly();
	[[nodiscard]] unsigned readCapacity();
	[[nodiscard]] bool checkAddress();
	[[nodiscard]] unsigned readSector(unsigned& blocks);
	[[nodiscard]] unsigned writeSector(unsigned& blocks);
	void formatUnit();

private:
	MSXMotherBoard& motherBoard;
	AlignedBuffer& buffer;
	File file;
	std::string filename;
	std::optional<LSXCommand> lsxCommand; // delayed init
	std::string name;
	const unsigned mode;
	unsigned keycode;      // Sense key, ASC, ASCQ
	unsigned currentSector;
	unsigned currentLength;
	const uint8_t scsiId;  // SCSI ID 0..7
	bool unitAttention;    // Unit Attention (was: reset)
	bool mediaChanged;     // Enhanced change flag for MEGA-SCSI driver
	uint8_t message;
	uint8_t lun;
	std::array<uint8_t, 12> cdb; // Command Descriptor Block

	static constexpr unsigned MAX_LS = 26;
	using LSInUse = std::bitset<MAX_LS>;
	std::shared_ptr<LSInUse> lsInUse;

	friend class LSXCommand;
};

} // namespace openmsx

#endif
