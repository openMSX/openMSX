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
#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include "File.hh"
#include <bitset>
#include <memory>

namespace openmsx {

class DeviceConfig;
class MSXMotherBoard;
class LSXCommand;

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
	void readSectorImpl (size_t sector,       SectorBuffer& buf) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	size_t getNbSectorsImpl() const override;
	bool isWriteProtectedImpl() const override;
	Sha1Sum getSha1SumImpl(FilePool& filePool) override;

	// Diskcontainer:
	SectorAccessibleDisk* getSectorAccessibleDisk() override;
	const std::string& getContainerName() const override;
	bool diskChanged() override;
	int insertDisk(std::string_view filename) override;

	// SCSI Device
	void reset() override;
	bool isSelected() override;
	unsigned executeCmd(const byte* cdb, SCSI::Phase& phase, unsigned& blocks) override;
	unsigned executingCmd(SCSI::Phase& phase, unsigned& blocks) override;
	byte getStatusCode() override;
	int msgOut(byte value) override;
	byte msgIn() override;
	void disconnect() override;
	void busReset() override;
	unsigned dataIn(unsigned& blocks) override;
	unsigned dataOut(unsigned& blocks) override;

	void eject();
	void insert(std::string_view filename);

	bool getReady();
	void testUnitReady();
	void startStopUnit();
	unsigned inquiry();
	unsigned modeSense();
	unsigned requestSense();
	bool checkReadOnly();
	unsigned readCapacity();
	bool checkAddress();
	unsigned readSector(unsigned& blocks);
	unsigned writeSector(unsigned& blocks);
	void formatUnit();

	MSXMotherBoard& motherBoard;
	AlignedBuffer& buffer;
	File file;
	std::unique_ptr<LSXCommand> lsxCommand;
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

	static const unsigned MAX_LS = 26;
	using LSInUse = std::bitset<MAX_LS>;
	std::shared_ptr<LSInUse> lsInUse;

	friend class LSXCommand;
};

} // namespace openmsx

#endif
