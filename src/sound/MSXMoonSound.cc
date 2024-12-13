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

MSXMoonSound::MSXMoonSound(const DeviceConfig& config)
	: MSXDevice(config)
	, ymf278b(getName(), getRamSize(config), config, getCurrentTime())
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
