#include "MSXRom.hh"
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
	return unmappedWrite;
}

void MSXRom::getExtraDeviceInfo(TclObject& result) const
{
	TclObject dict;

	// Add detected rom type. This value is guaranteed to be stored in
	// the device config (and 'auto' is already changed to actual type).
	const XMLElement* mapper = getDeviceConfig().findChild("mappertype");
	assert(mapper);
	dict.addListElement("mappertype");
	dict.addListElement(mapper->getData());

	// add sha1sum, to be able to get a unique key for this ROM device,
	// so that it can be used to look up things in databases
	dict.addListElement("actualSHA1");
	dict.addListElement(rom.getSHA1().toString());

	// add original sha1sum
	dict.addListElement("originalSHA1");
	dict.addListElement(rom.getOriginalSHA1().toString());

	// add original filename, e.g. to be able to see whether it comes
	// from a system_rom pool
	dict.addListElement("filename");
	dict.addListElement(rom.getFilename());

	result.addListElement(dict);
}

} // namespace openmsx
