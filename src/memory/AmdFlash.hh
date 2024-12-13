#ifndef AMDFLASH_HH
#define AMDFLASH_HH

#include "serialize_meta.hh"

#include "cstd.hh"
#include "narrow.hh"
#include "power_of_two.hh"
#include "ranges.hh"
#include "static_vector.hh"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

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
		SST = 0xBF,
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

	enum class DeviceInterface : uint16_t {
		x8 = 0x0000,
		x8x16 = 0x0002,
	};

	struct Region {
		size_t count = 0;
		power_of_two<size_t> size = 1;
	};

	struct Geometry {
		constexpr Geometry(DeviceInterface deviceInterface_, std::initializer_list<Region> regions_,
			int writeProtectPinRange_ = 0)
			: deviceInterface(deviceInterface_)
			, regions(regions_)
			, writeProtectPinRange(writeProtectPinRange_)
			, size(sum(regions, [](Region r) { return r.count * r.size; }))
			// Originally sum(regions, &Region::count), but seems to mis-compile to 0 on MSVC.
			// It looks like sum with projection doesn’t work at compile-time in MSVC 2022?
			//   https://developercommunity.visualstudio.com/t/wrong-code-bug-in-constexpr-evaluation/10673004
			, sectorCount(sum(regions, [](Region r) { return r.count; })) {}
		DeviceInterface deviceInterface;
		static_vector<Region, 4> regions;
		int writeProtectPinRange; // sectors protected by WP#, negative: from top
		power_of_two<size_t> size;
		size_t sectorCount;

		constexpr void validate() const {
			assert(ranges::all_of(regions, [](const auto& region) { return region.count > 0; }));
			assert(narrow_cast<unsigned>(cstd::abs(writeProtectPinRange)) <= sectorCount);
		}
	};

	struct Program {
		bool fastCommand : 1 = false;
		bool bufferCommand : 1 = false;
		bool shortAbortReset : 1 = false;
		power_of_two<size_t> pageSize = 1;

		constexpr void validate() const {
			assert(!fastCommand || pageSize > 1);
			assert(!bufferCommand || pageSize > 1);
			assert(!bufferCommand || AmdFlash::MAX_CMD_SIZE >= pageSize + 5);
		}
	};

	struct CFI {
		bool command : 1 = false;
		bool withManufacturerDevice : 1 = false; // < 0x10 contains mfr / device
		bool withAutoSelect : 1 = false;         // < 0x10 contains autoselect
		bool exitCommand : 1 = false;            // also exit by writing 0xFF
		size_t commandMask = 0xFF;               // command address mask
		size_t readMask = 0x7F;                  // read address mask
		struct SystemInterface {
			struct Supply {
				uint8_t minVcc = 0;
				uint8_t maxVcc = 0;
				uint8_t minVpp = 0;
				uint8_t maxVpp = 0;
			} supply = {};
			struct TypicalTimeout {
				power_of_two<unsigned> singleProgram = 1;
				power_of_two<unsigned> multiProgram = 1;
				power_of_two<unsigned> sectorErase = 1;
				power_of_two<unsigned> chipErase = 1;
			} typTimeout = {};
			struct MaxTimeoutMultiplier {
				power_of_two<unsigned> singleProgram = 1;
				power_of_two<unsigned> multiProgram = 1;
				power_of_two<unsigned> sectorErase = 1;
				power_of_two<unsigned> chipErase = 1;
			} maxTimeoutMult = {};
		} systemInterface = {};
		struct PrimaryAlgorithm {
			struct Version {
				uint8_t major = 0;
				uint8_t minor = 0;
			} version = {};
			uint8_t addressSensitiveUnlock : 2 = 0;
			uint8_t siliconRevision : 6 = 0;
			uint8_t eraseSuspend = 0;
			uint8_t sectorProtect = 0;
			uint8_t sectorTemporaryUnprotect = 0;
			uint8_t sectorProtectScheme = 0;
			uint8_t simultaneousOperation = 0;
			uint8_t burstMode = 0;
			uint8_t pageMode = 0;
			struct Supply {
				uint8_t minAcc = 0;
				uint8_t maxAcc = 0;
			} supply = {};
			uint8_t bootBlockFlag = 0;
			uint8_t programSuspend = 0;
		} primaryAlgorithm = {};

		constexpr void validate() const {
			assert(!command || primaryAlgorithm.version.major == 1);
		}
	};

	struct Misc {
		bool statusCommand : 1 = false;
		bool continuityCommand : 1 = false;

		constexpr void validate() const {
			assert(!continuityCommand || statusCommand);
		}
	};

	struct Chip {
		AutoSelect autoSelect;
		Geometry geometry;
		Program program = {};
		CFI cfi = {};
		Misc misc = {};

		constexpr void validate() const {
			autoSelect.validate();
			geometry.validate();
			program.validate();
			cfi.validate();
			misc.validate();
		}
	};

	struct ValidatedChip {
		constexpr ValidatedChip(const Chip& chip_) : chip(chip_) {
			chip.validate();
		}
		const Chip chip;
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
	         std::span<const bool> writeProtectSectors,
	         const DeviceConfig& config);
	AmdFlash(const std::string& name, const ValidatedChip& chip,
	         std::span<const bool> writeProtectSectors,
	         const DeviceConfig& config, std::string_view id = {});
	~AmdFlash();

	void reset();
	/**
	 * Setting the Vpp/WP# pin LOW enables a certain kind of write
	 * protection of some sectors. Currently it is implemented that it will
	 * enable protection of the first two sectors. (As for example in
	 * Numonix/Micron M29W640FB/M29W640GB.)
	 */
	void setVppWpPinLow(bool value) { vppWpPinLow = value; }

	[[nodiscard]] power_of_two<size_t> size() const { return chip.geometry.size; }
	[[nodiscard]] uint8_t read(size_t address);
	[[nodiscard]] uint8_t peek(size_t address) const;
	void write(size_t address, uint8_t value);
	[[nodiscard]] const uint8_t* getReadCacheLine(size_t address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

//private:
	struct AddressValue {
		size_t addr;
		uint8_t value;

		auto operator<=>(const AddressValue&) const = default;

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);
	};

	enum class State { IDLE, IDENT, CFI, STATUS, PRGERR };

	struct Sector {
		size_t address;
		power_of_two<size_t> size;
		bool writeProtect;
		ptrdiff_t writeAddress = -1;
		const uint8_t* readAddress = nullptr;

		std::weak_ordering operator<=>(const Sector& sector) const { return address <=> sector.address; }
		bool operator==(const Sector& sector) const { return address == sector.address; }
	};

private:
	AmdFlash(const std::string& name, const ValidatedChip& chip,
	         const Rom* rom, std::span<const bool> writeProtectSectors,
	         const DeviceConfig& config, std::string_view id);

	[[nodiscard]] size_t getSectorIndex(size_t address) const;
	[[nodiscard]] Sector& getSector(size_t address) { return sectors[getSectorIndex(address)]; };
	[[nodiscard]] const Sector& getSector(size_t address) const { return sectors[getSectorIndex(address)]; };

	void softReset();
	[[nodiscard]] uint16_t peekAutoSelect(size_t address, uint16_t undefined = 0xFFFF) const;
	[[nodiscard]] uint16_t peekCFI(size_t address) const;

	void setState(State newState);
	[[nodiscard]] bool checkCommandReset();
	[[nodiscard]] bool checkCommandLongReset();
	[[nodiscard]] bool checkCommandCFIQuery();
	[[nodiscard]] bool checkCommandCFIExit();
	[[nodiscard]] bool checkCommandStatusRead();
	[[nodiscard]] bool checkCommandStatusClear();
	[[nodiscard]] bool checkCommandEraseSector();
	[[nodiscard]] bool checkCommandEraseChip();
	[[nodiscard]] bool checkCommandProgramHelper(size_t numBytes, std::span<const uint8_t> cmdSeq);
	[[nodiscard]] bool checkCommandProgram();
	[[nodiscard]] bool checkCommandDoubleByteProgram();
	[[nodiscard]] bool checkCommandQuadrupleByteProgram();
	[[nodiscard]] bool checkCommandBufferProgram();
	[[nodiscard]] bool checkCommandAutoSelect();
	[[nodiscard]] bool checkCommandContinuityCheck();
	[[nodiscard]] bool partialMatch(std::span<const uint8_t> dataSeq) const;

	[[nodiscard]] bool isWritable(const Sector& sector) const;

public:
	static constexpr unsigned MAX_CMD_SIZE = 5 + 256; // longest command is BufferProgram

private:
	MSXMotherBoard& motherBoard;
	std::unique_ptr<SRAM> ram;
	const Chip& chip;
	std::vector<Sector> sectors;
	static_vector<AddressValue, MAX_CMD_SIZE> cmd;
	State state = State::IDLE;
	uint8_t status = 0x80;
	bool vppWpPinLow = false; // true = protection on
};
SERIALIZE_CLASS_VERSION(AmdFlash, 3);

namespace AmdFlashChip
{
	using ValidatedChip = AmdFlash::ValidatedChip;
	using enum AmdFlash::ManufacturerID;
	using DeviceInterface = AmdFlash::DeviceInterface;

	// AMD AM29F040
	static constexpr ValidatedChip AM29F040 = {{
		.autoSelect{.manufacturer = AMD, .device{0xA4}, .extraCode = 0x01},
		.geometry{DeviceInterface::x8, {{8, 0x10000}}},
	}};

	// AMD AM29F016
	static constexpr ValidatedChip AM29F016 = {{
		.autoSelect{.manufacturer = AMD, .device{0xAD}},
		.geometry{DeviceInterface::x8, {{32, 0x10000}}},
	}};

	// Microchip SST39SF010  (128kB, 32 x 4kB sectors)
	static constexpr ValidatedChip SST39SF010 = {{
		.autoSelect{.manufacturer = SST, .device{0xB5}},
		.geometry{DeviceInterface::x8, {{32, 0x1000}}},
	}};

	// Numonyx M29W800DB
	static constexpr ValidatedChip M29W800DB = {{
		.autoSelect{.manufacturer = STM, .device{0x5B}},
		.geometry{DeviceInterface::x8x16, {{1, 0x4000}, {2, 0x2000}, {1, 0x8000}, {15, 0x10000}}},
		.cfi{
			.command = true,
			.systemInterface{{0x27, 0x36, 0x00, 0x00}, {16, 1, 1024, 1}, {16, 1, 8, 1}},
			.primaryAlgorithm{{1, 0}, 0, 0, 2, 1, 1, 4, 0, 0, 0},
		},
	}};

	// Micron M29W640GB
	static constexpr ValidatedChip M29W640GB = {{
		.autoSelect{.manufacturer = STM, .device{0x10, 0x00}, .extraCode = 0x0008, .undefined = 0, .oddZero = true, .readMask = 0x7F},
		.geometry{DeviceInterface::x8x16, {{8, 0x2000}, {127, 0x10000}}, 2},
		.program{.fastCommand = true, .bufferCommand = true, .shortAbortReset = true, .pageSize = 32},
		.cfi{
			.command = true, .withManufacturerDevice = true, .commandMask = 0xFFF, .readMask = 0xFF,
			.systemInterface{{0x27, 0x36, 0xB5, 0xC5}, {16, 1, 1024, 1}, {16, 1, 8, 1}},
			.primaryAlgorithm{{1, 3}, 0, 0, 2, 4, 1, 4, 0, 0, 1, {0xB5, 0xC5}, 0x02, 1},
		},
	}};

	// Infineon / Cypress / Spansion S29GL064S70TFI040
	static constexpr ValidatedChip S29GL064S70TFI040 = {{
		.autoSelect{.manufacturer = AMD, .device{0x10, 0x00}, .extraCode = 0xFF0A, .readMask = 0x0F},
		.geometry{DeviceInterface::x8x16, {{8, 0x2000}, {127, 0x10000}}, 2},
		.program{.bufferCommand = true, .pageSize = 256},
		.cfi{
			.command = true, .withAutoSelect = true, .exitCommand = true, .commandMask = 0xFF, .readMask = 0x7F,
			.systemInterface{{0x27, 0x36, 0x00, 0x00}, {256, 256, 512, 65536}, {8, 8, 2, 1}},
			.primaryAlgorithm{{1, 3}, 0, 8, 2, 1, 0, 8, 0, 0, 2, {0xB5, 0xC5}, 0x02, 1},
		},
		.misc {.statusCommand = true, .continuityCommand = true},
	}};
}

} // namespace openmsx

#endif
