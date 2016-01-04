#ifndef AMDFLASH_HH
#define AMDFLASH_HH

#include "MemBuffer.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <memory>
#include <vector>

namespace openmsx {

class MSXMotherBoard;
class Rom;
class SRAM;
class DeviceConfig;

class AmdFlash
{
public:
	struct SectorInfo {
		unsigned size;
		bool writeProtected;
	};
	/** Create AmdFlash with given configuration.
	 * @param rom The initial content for this flash
	 * @param sectorInfo
	 *   A vector containing the size and write protected status of each
	 *   sector in the flash. This implicitly also communicates the number
	 *   of sectors (a sector is a region in the flash that can be erased
	 *   individually). There exist flash roms were the different sectors
	 *   are not all equally large, that's why it's required to enumerate
	 *   the size of each sector (instead of simply specifying the size and
	 *   the number of sectors).
	 * @param ID
	 *   Contains manufacturer and device ID for this flash.
	 * @param use12bitAddressing set to true for 12-bit command addresses, false for 11-bit command addresses
	 * @param config The motherboard this flash belongs to
	 * @param load Load initial content (hack for 'Matra INK')
	 */
	AmdFlash(const Rom& rom, const std::vector<SectorInfo>& sectorInfo,
	         word ID, bool use12bitAddressing,
	         const DeviceConfig& config, bool load = true);
	~AmdFlash();

	void reset();
	/**
	 * Setting the Vpp/WP# pin LOW enables a certain kind of write
	 * protection of some sectors. Currently it is implemented that it will
	 * enable protection of the first two sectors. (As for example in
	 * Numonix/Micron M29W640FB/M29W640GB.)
	 */
	void setVppWpPinLow(bool value) { vppWpPinLow = value; }

	unsigned getSize() const { return size; }
	byte read(unsigned address);
	byte peek(unsigned address) const;
	void write(unsigned address, byte value);
	const byte* getReadCacheLine(unsigned address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

//private:
	struct AmdCmd {
		unsigned addr;
		byte value;

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);
	};

	enum State { ST_IDLE, ST_IDENT };

private:
	void getSectorInfo(unsigned address, unsigned& sector,
                           unsigned& sectorSize, unsigned& offset) const;

	void setState(State newState);
	bool checkCommandReset();
	bool checkCommandEraseSector();
	bool checkCommandEraseChip();
	bool checkCommandProgramHelper(unsigned, const byte*, size_t cmdLen);
	bool checkCommandProgram();
	bool checkCommandQuadrupleByteProgram();
	bool checkCommandManifacturer();
	bool partialMatch(size_t len, const byte* dataSeq) const;

	bool isSectorWritable(unsigned sector) const;

	MSXMotherBoard& motherBoard;
	const Rom& rom;
	std::unique_ptr<SRAM> ram;
	MemBuffer<int> writeAddress;
	MemBuffer<const byte*> readAddress;
	const std::vector<SectorInfo> sectorInfo;
	const unsigned size;
	const word ID;
	const bool use12bitAddressing;

	static const unsigned MAX_CMD_SIZE = 8;
	AmdCmd cmd[MAX_CMD_SIZE];
	unsigned cmdIdx;
	State state;
	bool vppWpPinLow; // true = protection on
};
SERIALIZE_CLASS_VERSION(AmdFlash, 2);

} // namespace openmsx

#endif
