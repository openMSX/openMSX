#include "MSXMoonSound.hh"

#include "serialize.hh"

#include "one_of.hh"

#include <type_traits>

namespace openmsx {

static size_t getRamSize(const DeviceConfig& config)
{
	int ramSizeInKb = config.getChildDataAsInt("sampleram", 512);
	if (ramSizeInKb != one_of(
		   0,     //
		 128,     // 128kB
		 256,     // 128kB  128kB
		 512,     // 512kB
		 640,     // 512kB  128kB
		1024,     // 512kB  512kB
		2048)) {  // 512kB  512kB  512kB  512kB
		throw MSXException(
			"Wrong sample RAM size for MoonSound's YMF278. "
			"Got ", ramSizeInKb, ", but must be one of "
			"0, 128, 256, 512, 640, 1024 or 2048.");
	}
	return size_t(ramSizeInKb) * 1024; // kilo-bytes -> bytes
}

static void setupMemPtrs(bool mode0, std::span<const uint8_t> rom, std::span<const uint8_t> ram,
                         std::span<YMF278::Block128, 32> memPtrs)
{
	// /MCS0: connected to a 2MB ROM chip
	// For RAM there are multiple possibilities (but they all use /MCS6../MCS9)
	// * 128kB:
	//   1 SRAM chip of 128kB, chip enable (/CE) of this SRAM chip is connected to
	//   the 1Y0 output of a 74LS139 (2-to-4 decoder). The enable input of the
	//   74LS139 is connected to YMF278 pin /MCS6 and the 74LS139 1B:1A inputs are
	//   connected to YMF278 pins MA18:MA17. So the SRAM is selected when /MC6 is
	//   active and MA18:MA17 == 0:0.
	// * 256kB:
	//   2 SRAM chips of 128kB. First one connected as above. Second one has /CE
	//   connected to 74LS139 pin 1Y1. So SRAM2 is selected when /MSC6 is active
	//   and MA18:MA17 == 0:1.
	// * 512kB:
	//   1 SRAM chip of 512kB, /CE connected to /MCS6
	// * 640kB:
	//   1 SRAM chip of 512kB, /CE connected to /MCS6
	//   1 SRAM chip of 128kB, /CE connected to /MCS7.
	//     (This means SRAM2 is mirrored over a 512kB region)
	// * 1024kB:
	//   1 SRAM chip of 512kB, /CE connected to /MCS6
	//   1 SRAM chip of 512kB, /CE connected to /MCS7
	// * 2048kB:
	//   1 SRAM chip of 512kB, /CE connected to /MCS6
	//   1 SRAM chip of 512kB, /CE connected to /MCS7
	//   1 SRAM chip of 512kB, /CE connected to /MCS8
	//   1 SRAM chip of 512kB, /CE connected to /MCS9
	//   This configuration is not so easy to create on the v2.0 PCB. So it's
	//   very rare.
	// /MCS1../MCS5: unused
	static constexpr auto k128 = YMF278::k128;

	// first 2MB, ROM, both mode0 and mode1
	for (auto i : xrange(16)) {
		memPtrs[i] = subspan<k128>(rom, i * k128);
	}

	auto ramPart = [&](int i) {
		return (ram.size() >= (i + 1) * k128)
			? subspan<k128>(ram, i * k128)
			: YMF278::Block128{};
	};
	if (mode0) [[likely]] {
		// second 2MB, RAM, as much as if available, upto 2MB
		for (auto i : xrange(16)) {
			memPtrs[i + 16] = ramPart(i);
		}
	} else {
		// mode1, normally this shouldn't be used on MoonSound
		for (auto i : xrange(12)) {
			memPtrs[i + 16] = YMF278::Block128{};
		}
		for (auto i : xrange(4)) {
			memPtrs[i + 28] = ramPart(i);
		}
	}
}

MSXMoonSound::MSXMoonSound(const DeviceConfig& config)
	: MSXDevice(config)
	, ymf278b(getName(), getRamSize(config), config, setupMemPtrs, getCurrentTime())
{
}

void MSXMoonSound::powerUp(EmuTime::param time)
{
	ymf278b.powerUp(time);
}

void MSXMoonSound::reset(EmuTime::param time)
{
	ymf278b.reset(time);
}

byte MSXMoonSound::readIO(word port, EmuTime::param time)
{
	return ymf278b.readIO(port, time);
}

byte MSXMoonSound::peekIO(word port, EmuTime::param time) const
{
	return ymf278b.peekIO(port, time);
}

void MSXMoonSound::writeIO(word port, byte value, EmuTime::param time)
{
	ymf278b.writeIO(port, value, time);
}

// version 1: initial version
// version 2: added alreadyReadID
// version 3: moved loadTime and busyTime from YMF278 to here
//            removed alreadyReadID
// version 4: moved (most) state to YMF278B
template<typename Archive>
void MSXMoonSound::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);

	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("ymf278b", ymf278b);
	} else {
		assert(Archive::IS_LOADER);
		if constexpr (std::is_same_v<Archive, XmlInputArchive>) {
			ymf278b.serialize_bw_compat(
				static_cast<XmlInputArchive&>(ar), version, getCurrentTime());
		} else {
			assert(false);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXMoonSound);
REGISTER_MSXDEVICE(MSXMoonSound, "MoonSound");

} // namespace openmsx
