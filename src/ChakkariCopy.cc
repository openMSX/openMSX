#include "ChakkariCopy.hh"
#include "CliComm.hh"
#include "serialize.hh"

// The cartridge has 2 buttons with 2 LEDs, marked PAUSE and COPY. It also has
// a toggle switch with the options COPY and RAM. Setting this toggle to RAM
// makes the cartridge a 16kB RAM expansion.
//
// The following information is provided by Rudolf Lechleitner, who devised it
// together with Enrico Barbisan.
//
// ----------------------------------------------------------------------------
//
// The memory layout:
//
// +--------------------------------------------+
// | 0000-3FFF: 16 KB RAM in COPY mode          |
// | Page 0: available in COPY mode only        |
// | can be toggled to read-only                |
// +--------------------------------------------+
// | 4000-5FFF: 8KB ROM code                    |
// | Page 1: +                                  |
// | 6000-67FF: 2KB internal RAM                |
// +--------------------------------------------+
//
// The shown 16KB RAM in page 0 are only available in COPY mode, but can be
// toggled to read-only. The location of the RAM in RAM mode is unknown.
//
// The memory at 6800-7FFF is unused by the cartridge.
//
// Internal handling: during cartridge initialisation Chakkari Copy copies the
// contents of BIOS page 0 into its own RAM and patches some entries to
// capture the input of PAUSE and COPY buttons of the cartridge. Afterwards the
// patched memory is set to read-only.
// PAUSE and COPY itself are processed by the routines in the ROM. In case the
// page 0 is (for any reason) switched to the original MSX-BIOS the cartridge
// buttons are no longer working. In this case CALL SCHANGE switches the BIOS
// to the patched version again.
//
// In case a program switches screen modes in a way that prevents Chakkari Copy
// from detecting the correct parameters Chakkari Copy can be started by
// keeping the COPY button pressed. In this case the screen-mode autodetect
// will be skipped and the parameters can be entered manually.
// This appears to be an undocumented feature, confirmed by typos and a very
// rudimentary screen handling.
//
// In case the printer is not ready or not available the printing can be
// aborted by pressing Ctrl+Stop.
//
// The Chakkari Copy cartridge has one register, which controls its complete
// behavior. This register is available at MSX port &H7F.
//
// READ only:
//
// +-----------------------------------------+
// |  1  |  1  |  1  |  1  |  1  | PSE | CPY |
// +-----+-----+-----+-----+-----+-----+-----+
// CPY: 0 = Copy button pressed, 1 = Copy button released
// PSE: 0 = Pause button pressed, 1 = Pause button released
//
// WRITE only:
//
// +-----+-----+-----+-----+-----+-----+-----+
// |  1  |  1  |  1  |  1  | RAM | PSE | CPY |
// +-----+-----+-----+-----+-----+-----+-----+
// CPY: 0 = Copy LED on, 1 = Copy LED off
// PSE: 0 = Pause LED on, 1 = Pause LED off
// RAM: 0 = Page 0 in RAM mode, 1 = Page 0 in ROM mode (applies to COPY mode only)
//
// ----------------------------------------------------------------------------

// The RAM mode has now been implemented to switch the RAM to page 1. This
// doesn't seem to be the most logical, but it's closest to the output of
// MSXMEM2. It could still be useful for 32kB machines.
//
// The work RAM has been mirrored in the 0x6000-0x8000 area to match the output
// of MSXMEM2.BAS which was run on a machine with the real cartridge inserted.
// It showed 'R' on page 1 for both COPY and RAM mode. The only thing that
// doesn't match yet is that it also showed 'R' in page 0 for COPY mode for
// subslots 0 and 1. The latter is unexplained so far.

namespace openmsx {

ChakkariCopy::ChakkariCopy(const DeviceConfig& config)
	: MSXDevice(config)
	, biosRam(config, getName() + " BIOS RAM", "Chakkari Copy BIOS RAM", 0x4000)
	, workRam(config, getName() + " work RAM", "Chakkari Copy work RAM", 0x0800)
	, rom(getName() + " ROM", "rom", config)
	, pauseButtonPressedSetting(getCommandController(),
		getName() + " PAUSE button pressed",
		"controls the PAUSE button state", false, Setting::DONT_SAVE)
	, copyButtonPressedSetting(getCommandController(),
		getName() + " COPY button pressed",
		"controls the COPY button state", false, Setting::DONT_SAVE)
	, modeSetting(getCommandController(), getName() + " mode",
		"Sets mode of the cartridge: in COPY mode you can hardcopy MSX1 screens, "
		"in RAM mode you just have a 16kB RAM expansion", ChakkariCopy::COPY,
		EnumSetting<ChakkariCopy::Mode>::Map{
			{"COPY", ChakkariCopy::COPY}, {"RAM", ChakkariCopy::RAM}})
	, reg(0xFF) // avoid UMR in initial writeIO()
{
	reset(getCurrentTime());
	modeSetting.attach(*this);
}

ChakkariCopy::~ChakkariCopy()
{
	modeSetting.detach(*this);
}

void ChakkariCopy::reset(EmuTime::param time)
{
	writeIO(0, 0xFF, time);
}

void ChakkariCopy::writeIO(word /*port*/, byte value, EmuTime::param /*time*/)
{
	byte diff = reg ^ value;
	reg = value;

	if (diff & 0x01) {
		getCliComm().printInfo(getName(), " COPY LED ",
			(((value & 1) == 0x01) ? "OFF" : "ON"));
	}
	if (diff & 0x02) {
		getCliComm().printInfo(getName(), " PAUSE LED ",
			(((value & 2) == 0x02) ? "OFF" : "ON"));
	}
	if (diff & 0x04) {
		if (modeSetting.getEnum() == COPY) {
			// page 0 toggles writable/read-only
			invalidateDeviceRWCache(0x0000, 0x4000);
		}
	}
}

byte ChakkariCopy::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte ChakkariCopy::peekIO(word /*port*/, EmuTime::param /*time*/) const
{
	byte retVal = 0xFF;
	if (copyButtonPressedSetting .getBoolean()) retVal &= ~0x01;
	if (pauseButtonPressedSetting.getBoolean()) retVal &= ~0x02;
	return retVal;
}

byte ChakkariCopy::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

byte ChakkariCopy::peekMem(word address, EmuTime::param /*time*/) const
{
	return *getReadCacheLine(address);
}

const byte* ChakkariCopy::getReadCacheLine(word address) const
{
	if (modeSetting.getEnum() == COPY) {
		// page 0
		if (address < 0x4000) {
			return &biosRam[address];
		}
		// page 1
		if ((0x4000 <= address) && (address < 0x6000)) {
			return &rom[address & 0x1FFF];
		}
		// the work RAM is mirrored in 0x6000-0x8000, see above
		if ((0x6000 <= address) && (address < 0x8000)) {
			return &workRam[address & 0x07FF];
		}
	} else {
		// page 1 RAM mode
		if ((0x4000 <= address) && (address < 0x8000)) {
			return &biosRam[address & 0x3FFF];
		}
	}
	return unmappedRead;
}

void ChakkariCopy::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	*getWriteCacheLine(address) = value;
}

byte* ChakkariCopy::getWriteCacheLine(word address) const
{
	if (modeSetting.getEnum() == COPY) {
		// page 0
		if ((address < 0x4000) && ((reg & 0x04) == 0)) {
			return const_cast<byte*>(&biosRam[address & 0x3FFF]);
		}
		// page 1
		// the work RAM is mirrored in 0x6000-0x8000, see above
		if ((0x6000 <= address) && (address < 0x8000)) {
			return const_cast<byte*>(&workRam[address & 0x07FF]);
		}
	} else {
		// page 1 RAM mode
		if ((0x4000 <= address) && (address < 0x8000)) {
			return const_cast<byte*>(&biosRam[address & 0x3FFF]);
		}
	}
	return unmappedWrite;
}

void ChakkariCopy::update(const Setting& /*setting*/) noexcept
{
	// switch COPY <-> RAM mode, memory layout changes
	invalidateDeviceRWCache();
}

template<Archive Ar>
void ChakkariCopy::serialize(Ar& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("biosRam", biosRam,
	             "workRam", workRam,
	             "reg", reg);
	if constexpr (Ar::IS_LOADER) {
		writeIO(0, reg, getCurrentTime());
	}

}
INSTANTIATE_SERIALIZE_METHODS(ChakkariCopy);
REGISTER_MSXDEVICE(ChakkariCopy, "ChakkariCopy");

} // namespace openmsx
