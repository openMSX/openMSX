#ifndef AMDFLASH_HH
#define AMDFLASH_HH

#include "MemBuffer.hh"
#include "power_of_two.hh"
#include "serialize_meta.hh"
#include "static_vector.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace openmsx {

class MSXMotherBoard;
class Rom;
class SRAM;
class DeviceConfig;

class AmdFlash
{
public:
	// See JEDEC JEP106 https://www.jedec.org/standards-documents/docs/jep-106ab
	enum class ManufacturerID : uint8_t {
		AMD = 0x01,
		STM = 0x20,
	};

	struct AutoSelect {
		ManufacturerID manufacturer;
		static_vector<uint8_t, 2> device; // single-byte or double-byte (0x7E prefix)
		uint16_t extraCode = 0x0000;      // code at 3rd index
		uint16_t undefined = 0xFFFF;      // undefined values
		bool oddZero : 1 = false;         // odd bytes are zero, not mirrored
		size_t readMask = 0x03;           // read address mask

		constexpr void validate() const {
			// check parity
			assert(std::popcount(to_underlying(manufacturer)) & 1);
			// extended marker is not part of ID
			assert(device.size() > 0 && device[0] != 0x7E);
		}
	};

	struct Chip {
		AutoSelect autoSelect;

		constexpr void validate() const {
			autoSelect.validate();
		}
	};

	struct ValidatedChip {
		constexpr ValidatedChip(const Chip& chip_) : chip(chip_) {
			chip.validate();
		}
		const Chip chip;
	};

	struct SectorInfo {
		size_t size;
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
	 * @param chip Contains chip configuration for this flash.
	 * @param sectorInfo
	 *   A span containing the size and write protected status of each
	 *   sector in the flash. This implicitly also communicates the number
	 *   of sectors (a sector is a region in the flash that can be erased
	 *   individually). There exist flash roms were the different sectors
	 *   are not all equally large, that's why it's required to enumerate
	 *   the size of each sector (instead of simply specifying the size and
	 *   the number of sectors).
	 * @param addressing Specify addressing mode (11-bit or 12-bit)
	 * @param config The motherboard this flash belongs to
	 * @param load Load initial content (hack for 'Matra INK')
	 */
	AmdFlash(const Rom& rom, const ValidatedChip& chip,
	         std::span<const SectorInfo> sectorInfo, Addressing addressing,
	         const DeviceConfig& config, Load load = Load::NORMAL);
	AmdFlash(const std::string& name, const ValidatedChip& chip,
	         std::span<const SectorInfo> sectorInfo, Addressing addressing,
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

	[[nodiscard]] size_t size() const { return sz; }
	[[nodiscard]] uint8_t read(size_t address) const;
	[[nodiscard]] uint8_t peek(size_t address) const;
	void write(size_t address, uint8_t value);
	[[nodiscard]] const uint8_t* getReadCacheLine(size_t address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

//private:
	struct AmdCmd {
		size_t addr;
		uint8_t value;

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);
	};

	enum class State { IDLE, IDENT };

private:
	void init(const std::string& name, const DeviceConfig& config, Load load, const Rom* rom);
	struct GetSectorInfoResult { size_t sector, sectorSize, offset; };
	[[nodiscard]] GetSectorInfoResult getSectorInfo(size_t address) const;

	[[nodiscard]] uint16_t peekAutoSelect(size_t address, uint16_t undefined = 0xFFFF) const;

	void setState(State newState);
	[[nodiscard]] bool checkCommandReset();
	[[nodiscard]] bool checkCommandEraseSector();
	[[nodiscard]] bool checkCommandEraseChip();
	[[nodiscard]] bool checkCommandProgramHelper(size_t numBytes, std::span<const uint8_t> cmdSeq);
	[[nodiscard]] bool checkCommandProgram();
	[[nodiscard]] bool checkCommandDoubleByteProgram();
	[[nodiscard]] bool checkCommandQuadrupleByteProgram();
	[[nodiscard]] bool checkCommandAutoSelect();
	[[nodiscard]] bool partialMatch(std::span<const uint8_t> dataSeq) const;

	[[nodiscard]] bool isSectorWritable(size_t sector) const;

private:
	MSXMotherBoard& motherBoard;
	std::unique_ptr<SRAM> ram;
	MemBuffer<ptrdiff_t> writeAddress;
	MemBuffer<const uint8_t*> readAddress;
	const Chip& chip;
	const std::span<const SectorInfo> sectorInfo;
	const power_of_two<size_t> sz;
	const Addressing addressing;

	static constexpr unsigned MAX_CMD_SIZE = 8;
	static_vector<AmdCmd, MAX_CMD_SIZE> cmd;
	State state = State::IDLE;
	bool vppWpPinLow = false; // true = protection on
};
SERIALIZE_CLASS_VERSION(AmdFlash, 3);

namespace AmdFlashChip
{
	using ValidatedChip = AmdFlash::ValidatedChip;
	using enum AmdFlash::ManufacturerID;

	// AMD AM29F040
	static constexpr ValidatedChip AM29F040 = {{
		.autoSelect{.manufacturer = AMD, .device{0xA4}, .extraCode = 0x01},
	}};

	// AMD AM29F016
	static constexpr ValidatedChip AM29F016 = {{
		.autoSelect{.manufacturer = AMD, .device{0xAD}},
	}};

	// Numonyx M29W800DB
	static constexpr ValidatedChip M29W800DB = {{
		.autoSelect{.manufacturer = STM, .device{0x5B}},
	}};

	// Micron M29W640GB
	static constexpr ValidatedChip M29W640GB = {{
		.autoSelect{.manufacturer = STM, .device{0x10, 0x00}, .extraCode = 0x0008, .undefined = 0, .oddZero = true, .readMask = 0x7F},
	}};
}

} // namespace openmsx

#endif
