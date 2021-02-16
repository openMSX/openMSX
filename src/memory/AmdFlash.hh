#ifndef AMDFLASH_HH
#define AMDFLASH_HH

#include "MemBuffer.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include "span.hh"
#include <memory>

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
	enum class Addressing {
		BITS_11,
		BITS_12,
	};
	enum class Load {
		NORMAL,
		DONT, // don't load nor save modified flash content
	};

	/** Create AmdFlash with given configuration.
	 * @param rom The initial content for this flash
	 * @param sectorInfo
	 *   A span containing the size and write protected status of each
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
	AmdFlash(const Rom& rom, span<const SectorInfo> sectorInfo,
	         word ID, Addressing addressing,
	         const DeviceConfig& config, Load load = Load::NORMAL);
	AmdFlash(const std::string& name, span<const SectorInfo> sectorInfo,
	         word ID, Addressing addressing,
		 const DeviceConfig& config);
	~AmdFlash();

	void reset();
	/**
	 * Setting the Vpp/WP# pin LOW enables a certain kind of write
	 * protection of some sectors. Currently it is implemented that it will
	 * enable protection of the first two sectors. (As for example in
	 * Numonix/Micron M29W640FB/M29W640GB.)
	 */
	void setVppWpPinLow(bool value) { vppWpPinLow = value; }

	[[nodiscard]] unsigned getSize() const { return size; }
	[[nodiscard]] byte read(unsigned address) const;
	[[nodiscard]] byte peek(unsigned address) const;
	void write(unsigned address, byte value);
	[[nodiscard]] const byte* getReadCacheLine(unsigned address) const;

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
	void init(const std::string& name, const DeviceConfig& config, Load load, const Rom* rom);
	struct GetSectorInfoResult { unsigned sector, sectorSize, offset; };
	[[nodiscard]] GetSectorInfoResult getSectorInfo(unsigned address) const;

	void setState(State newState);
	[[nodiscard]] bool checkCommandReset();
	[[nodiscard]] bool checkCommandEraseSector();
	[[nodiscard]] bool checkCommandEraseChip();
	[[nodiscard]] bool checkCommandProgramHelper(unsigned numBytes, const byte* cmdSeq, size_t cmdLen);
	[[nodiscard]] bool checkCommandProgram();
	[[nodiscard]] bool checkCommandDoubleByteProgram();
	[[nodiscard]] bool checkCommandQuadrupleByteProgram();
	[[nodiscard]] bool checkCommandManufacturer();
	[[nodiscard]] bool partialMatch(size_t len, const byte* dataSeq) const;

	[[nodiscard]] bool isSectorWritable(unsigned sector) const;

private:
	MSXMotherBoard& motherBoard;
	std::unique_ptr<SRAM> ram;
	MemBuffer<int> writeAddress;
	MemBuffer<const byte*> readAddress;
	const span<const SectorInfo> sectorInfo;
	const unsigned size;
	const word ID;
	const Addressing addressing;

	static constexpr unsigned MAX_CMD_SIZE = 8;
	AmdCmd cmd[MAX_CMD_SIZE];
	unsigned cmdIdx;
	State state = ST_IDLE;
	bool vppWpPinLow = false; // true = protection on
};
SERIALIZE_CLASS_VERSION(AmdFlash, 2);

} // namespace openmsx

#endif
