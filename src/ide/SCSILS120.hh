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

#include "RecordedCommand.hh"
#include "SCSIDevice.hh"
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "File.hh"
#include <bitset>
#include <optional>

namespace openmsx {

class DeviceConfig;
class MSXMotherBoard;
class SCSILS120;

class LSXCommand final : public RecordedCommand
{
public:
	LSXCommand(CommandController& commandController,
	           StateChangeDistributor& stateChangeDistributor,
	           Scheduler& scheduler, SCSILS120& ls);
	void execute(span<const TclObject> tokens,
	             TclObject& result, EmuTime::param time) override;
	[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	SCSILS120& ls;
};

class SCSILS120 final : public SCSIDevice, public SectorAccessibleDisk
                      , public DiskContainer
{
public:
	SCSILS120(const SCSILS120&) = delete;
	SCSILS120 operator=(const SCSILS120&) = delete;

	SCSILS120(const DeviceConfig& targetconfig,
	          AlignedBuffer& buf, unsigned mode);
	~SCSILS120() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SectorAccessibleDisk:
	void readSectorsImpl(
		SectorBuffer* buffers, size_t startSector, size_t num) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] size_t getNbSectorsImpl() const override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;
	[[nodiscard]] Sha1Sum getSha1SumImpl(FilePool& filePool) override;

	// Diskcontainer:
	[[nodiscard]] SectorAccessibleDisk* getSectorAccessibleDisk() override;
	[[nodiscard]] std::string_view getContainerName() const override;
	[[nodiscard]] bool diskChanged() override;
	int insertDisk(const std::string& filename) override;

	// SCSI Device
	void reset() override;
	[[nodiscard]] bool isSelected() override;
	[[nodiscard]] unsigned executeCmd(const byte* cdb, SCSI::Phase& phase, unsigned& blocks) override;
	[[nodiscard]] unsigned executingCmd(SCSI::Phase& phase, unsigned& blocks) override;
	[[nodiscard]] byte getStatusCode() override;
	int msgOut(byte value) override;
	byte msgIn() override;
	void disconnect() override;
	void busReset() override;
	[[nodiscard]] unsigned dataIn(unsigned& blocks) override;
	[[nodiscard]] unsigned dataOut(unsigned& blocks) override;

	void eject();
	void insert(const std::string& filename);

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
	std::optional<LSXCommand> lsxCommand; // delayed init
	std::string name;
	const int mode;
	unsigned keycode;      // Sense key, ASC, ASCQ
	unsigned currentSector;
	unsigned currentLength;
	const byte scsiId;     // SCSI ID 0..7
	bool unitAttention;    // Unit Attention (was: reset)
	bool mediaChanged;     // Enhanced change flag for MEGA-SCSI driver
	byte message;
	byte lun;
	byte cdb[12];          // Command Descriptor Block

	static constexpr unsigned MAX_LS = 26;
	using LSInUse = std::bitset<MAX_LS>;
	std::shared_ptr<LSInUse> lsInUse;

	friend class LSXCommand;
};

} // namespace openmsx

#endif
