#include "MSXRom.hh"
#include "RomInfo.hh"
#include "XMLElement.hh"
#include "TclObject.hh"

namespace openmsx {

MSXRom::MSXRom(const DeviceConfig& config, Rom&& rom_)
	: MSXDevice(config, rom_.getName())
	, rom(std::move(rom_))
{
}

void MSXRom::writeMem(word /*address*/, byte /*value*/, EmuTime::param /*time*/)
{
	// nothing
}

byte* MSXRom::getWriteCacheLine(word /*address*/) const
{
	return unmappedWrite.data();
}

std::string_view MSXRom::getMapperTypeString() const
{
	// This value is guaranteed to be stored in the device config (and
	// 'auto' is already changed to actual type).
	const auto* mapper = getDeviceConfig().findChild("mappertype");
	assert(mapper);
	return mapper->getData();
}

RomType MSXRom::getRomType() const
{
	auto result = RomInfo::nameToRomType(getMapperTypeString());
	assert(result != ROM_UNKNOWN);
	return result;
}

void MSXRom::getInfo(TclObject& result) const
{
	// Add detected rom type.
	result.addDictKeyValues("mappertype", getMapperTypeString(),

	// add sha1sum, to be able to get a unique key for this ROM device,
	// so that it can be used to look up things in databases
	                        "actualSHA1", rom.getSHA1().toString(),

	// add original sha1sum
	                        "originalSHA1", rom.getOriginalSHA1().toString());

	// note that we're not using rom.getInfo(result); because we don't want
	// the filename included for this method...
}

void MSXRom::getExtraDeviceInfo(TclObject& result) const
{
	getInfo(result);

	// add original filename, e.g. to be able to see whether it comes
	// from a system_rom pool
	result.addDictKeyValue("filename", rom.getFilename());
}

} // namespace openmsx
