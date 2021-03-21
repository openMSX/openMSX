// Note: this device is actually called SCC-I. But this would take a lot of
// renaming, which isn't worth it right now. TODO rename this :)

#include "MSXSCCPlusCart.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "XMLElement.hh"
#include "CacheLine.hh"
#include "Math.hh"
#include "enumerate.hh"
#include "ranges.hh"
#include "serialize.hh"

namespace openmsx {

static auto getMapperConfig(const DeviceConfig& config)
{
	MSXSCCPlusCart::MapperConfig result;

	std::string_view subtype = config.getChildData("subtype", "expanded");
	if (subtype == "Snatcher") {
		// blocks 0-7 in use, 8-15 unmapped, others are mirrored
		result.numBlocks = 8; // 64kB
		result.registerMask = 0b0000'1111; // ignore upper 4 bits
		result.registerOffset = 0; // start at block 0
	} else if (subtype == "SD-Snatcher") {
		// blocks 0-7 unmapped, 8-15 in use, others are mirrored
		result.numBlocks = 8; // 64kB
		result.registerMask = 0b0000'1111; // ignore upper 4 bits
		result.registerOffset = 8; // start at block 8
	} else if (subtype == "mirrored") {
		// blocks 0-7 in use, others are mirrored
		result.numBlocks = 8; // 64kB
		result.registerMask = 0b0000'0111; // ignore upper 5 bits
		result.registerOffset = 0; // start at block 0
	} else if (subtype == "Popolon") {
		// this subtype supports configurable size (128, 256, 512, 1024, 2048)
		unsigned ramSize = config.getChildDataAsInt("size", 2048);
		if (!Math::ispow2(ramSize)) {
			throw MSXException(
				"Popolon type Sound Cartridge must have a power-of-2 RAM size: ",
				ramSize);
		}
		if (ramSize < 128 || ramSize > 2048) {
			throw MSXException(
				"Popolon type Sound Cartridge must have a size between 128kB and 2048kB: ",
				ramSize);
		}
		result.numBlocks = ramSize / 8;
		result.registerMask = result.numBlocks - 1; // this assumes mirroring, correct?
		result.registerOffset = 0; // start at block 0
	} else {
		// subtype "expanded", and all others
		// blocks 0-15 in use, others are mirrored
		result.numBlocks = 16; // 128kB
		result.registerMask = 0b0000'1111; // ignore upper 4 bits
		result.registerOffset = 0; // start at block 0
	}
	return result;
}


MSXSCCPlusCart::MSXSCCPlusCart(const DeviceConfig& config)
	: MSXDevice(config)
	, mapperConfig(getMapperConfig(config))
	, ram(config, getName() + " RAM", "SCC+ RAM", mapperConfig.numBlocks * 0x2000)
	, scc(getName(), config, getCurrentTime(), SCC::SCC_Compatible)
	, romBlockDebug(*this, mapper, 0x4000, 0x8000, 13)
{
	if (const XMLElement* fileElem = config.findChild("filename")) {
		// read the rom file
		const std::string& filename = fileElem->getData();
		try {
			File file(config.getFileContext().resolve(filename));
			auto size = std::min<size_t>(file.getSize(), ram.getSize());
			file.read(&ram[0], size);
		} catch (FileException&) {
			throw MSXException("Error reading file: ", filename);
		}
	}
	// make valgrind happy
	ranges::fill(isRamSegment, true);
	ranges::fill(mapper, 0);

	powerUp(getCurrentTime());
}

void MSXSCCPlusCart::powerUp(EmuTime::param time)
{
	scc.powerUp(time);
	reset(time);
}

void MSXSCCPlusCart::reset(EmuTime::param time)
{
	setModeRegister(0);
	setMapper(0, 0);
	setMapper(1, 1);
	setMapper(2, 2);
	setMapper(3, 3);
	scc.reset(time);
}


byte MSXSCCPlusCart::readMem(word addr, EmuTime::param time)
{
	if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
	    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
		return scc.readMem(addr & 0xFF, time);
	} else {
		return MSXSCCPlusCart::peekMem(addr, time);
	}
}

byte MSXSCCPlusCart::peekMem(word addr, EmuTime::param time) const
{
	// modeRegister can not be read!
	if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
	    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
		// SCC  visible in 0x9800 - 0x9FFF
		// SCC+ visible in 0xB800 - 0xBFFF
		return scc.peekMem(addr & 0xFF, time);
	} else if ((0x4000 <= addr) && (addr < 0xC000)) {
		// SCC(+) enabled/disabled but not requested so memory stuff
		return internalMemoryBank[(addr >> 13) - 2][addr & 0x1FFF];
	} else {
		// outside memory range
		return 0xFF;
	}
}

const byte* MSXSCCPlusCart::getReadCacheLine(word start) const
{
	if (((enable == EN_SCC)     && (0x9800 <= start) && (start < 0xA000)) ||
	    ((enable == EN_SCCPLUS) && (0xB800 <= start) && (start < 0xC000))) {
		// SCC  visible in 0x9800 - 0x9FFF
		// SCC+ visible in 0xB800 - 0xBFFF
		return nullptr;
	} else if ((0x4000 <= start) && (start < 0xC000)) {
		// SCC(+) enabled/disabled but not requested so memory stuff
		return &internalMemoryBank[(start >> 13) - 2][start & 0x1FFF];
	} else {
		// outside memory range
		return unmappedRead;
	}
}


void MSXSCCPlusCart::writeMem(word address, byte value, EmuTime::param time)
{
	if ((address < 0x4000) || (0xC000 <= address)) {
		// outside memory range
		return;
	}

	// Mode register is mapped upon 0xBFFE and 0xBFFF
	if ((address | 0x0001) == 0xBFFF) {
		setModeRegister(value);
		return;
	}

	// Write to RAM
	int region = (address >> 13) - 2;
	if (isRamSegment[region]) {
		// According to Sean Young
		// when the regions are in RAM mode you can read from
		// the SCC(+) but not write to them
		// => we assume a write to the memory but maybe
		//    they are just discarded
		// TODO check this out => ask Sean...
		if (isMapped[region]) {
			internalMemoryBank[region][address & 0x1FFF] = value;
		}
		return;
	}

	/* Write to bankswitching registers
	 * The address to change banks:
	 *   bank 1: 0x5000 - 0x57FF (0x5000 used)
	 *   bank 2: 0x7000 - 0x77FF (0x7000 used)
	 *   bank 3: 0x9000 - 0x97FF (0x9000 used)
	 *   bank 4: 0xB000 - 0xB7FF (0xB000 used)
	 */
	if ((address & 0x1800) == 0x1000) {
		setMapper(region, value);
		return;
	}

	// call writeMemInterface of SCC if needed
	switch (enable) {
	case EN_NONE:
		// do nothing
		break;
	case EN_SCC:
		if ((0x9800 <= address) && (address < 0xA000)) {
			scc.writeMem(address & 0xFF, value, time);
		}
		break;
	case EN_SCCPLUS:
		if ((0xB800 <= address) && (address < 0xC000)) {
			scc.writeMem(address & 0xFF, value, time);
		}
		break;
	}
}

byte* MSXSCCPlusCart::getWriteCacheLine(word start) const
{
	if ((0x4000 <= start) && (start < 0xC000)) {
		if (start == (0xBFFF & CacheLine::HIGH)) {
			return nullptr;
		}
		int region = (start >> 13) - 2;
		if (isRamSegment[region] && isMapped[region]) {
			return &internalMemoryBank[region][start & 0x1FFF];
		}
		return nullptr;
	}
	return unmappedWrite;
}


void MSXSCCPlusCart::setMapper(int region, byte value)
{
	mapper[region] = value;

	value &= mapperConfig.registerMask;
	value -= mapperConfig.registerOffset;
	byte* block = [&] {
		if (value < mapperConfig.numBlocks) {
			isMapped[region] = true;
			return &ram[0x2000 * value];
		} else {
			isMapped[region] = false;
			return unmappedRead;
		}
	}();

	checkEnable(); // invalidateDeviceRWCache() done below
	internalMemoryBank[region] = block;
	invalidateDeviceRWCache(0x4000 + region * 0x2000, 0x2000);
}

void MSXSCCPlusCart::setModeRegister(byte value)
{
	modeRegister = value;
	checkEnable(); // invalidateDeviceRWCache() done below

	if (modeRegister & 0x20) {
		scc.setChipMode(SCC::SCC_plusmode);
	} else {
		scc.setChipMode(SCC::SCC_Compatible);
	}

	if (modeRegister & 0x10) {
		isRamSegment[0] = true;
		isRamSegment[1] = true;
		isRamSegment[2] = true;
		isRamSegment[3] = true;
	} else {
		isRamSegment[0] = (modeRegister & 0x01) == 0x01;
		isRamSegment[1] = (modeRegister & 0x02) == 0x02;
		isRamSegment[2] = (modeRegister & 0x24) == 0x24; // extra requirement: SCC+ mode
		isRamSegment[3] = false;
	}
	invalidateDeviceRWCache(0x4000, 0x8000);
}

void MSXSCCPlusCart::checkEnable()
{
	if ((modeRegister & 0x20) && (mapper[3] & 0x80)) {
		enable = EN_SCCPLUS;
	} else if ((!(modeRegister & 0x20)) && ((mapper[2] & 0x3F) == 0x3F)) {
		enable = EN_SCC;
	} else {
		enable = EN_NONE;
	}
}


template<typename Archive>
void MSXSCCPlusCart::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize_blob("ram", &ram[0], ram.getSize());
	ar.serialize("scc",    scc,
	             "mapper", mapper,
	             "mode",   modeRegister);

	if constexpr (Archive::IS_LOADER) {
		// recalculate: isMapped[4], internalMemoryBank[4]
		for (auto [i, m] : enumerate(mapper)) {
			setMapper(int(i), m);
		}
		// recalculate: enable, isRamSegment[4]
		setModeRegister(modeRegister);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXSCCPlusCart);
REGISTER_MSXDEVICE(MSXSCCPlusCart, "SCCPlus");

} // namespace openmsx
