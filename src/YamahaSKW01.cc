#include "YamahaSKW01.hh"
#include "DummyPrinterPortDevice.hh"
#include "CacheLine.hh"
#include "MSXException.hh"
#include "checked_cast.hh"
#include "serialize.hh"

// Implementation based on reverse-engineering of 'acet'/'uniabis', see:
//   https://www.msx.org/forum/msx-talk/hardware/trying-to-dump-yamaha-skw-01
//   https://github.com/uniabis/msxrawl/tree/main/dumpskw1
// The printer port is assumed to work the same as a standard MSX printer port.
// This seems to be OK, because when printing with the SKW-01 software and
// selecting the "HR-5X" printer and an emulated MSX Printer plugged in, sane
// output is produced.
// As the emulation is purely based on reverse engineering based on the built
// in software, there may be plenty of details wrong.
// Also note that the delay that is apparently present when writing the font
// address registers (other wise the READY bit wouldn't need to be read out) is
// not emulated. Because of that, the READY bit will always just be enabled.

namespace openmsx {

YamahaSKW01::YamahaSKW01(const DeviceConfig& config)
	: MSXDevice(config)
	, Connector(MSXDevice::getPluggingController(), MSXDevice::getName() + " printerport",
	            std::make_unique<DummyPrinterPortDevice>())
	, mainRom(MSXDevice::getName() + " main"     , "rom", config, "main")
	, fontRom(MSXDevice::getName() + " kanjifont", "rom", config, "kanjifont")
	, dataRom(MSXDevice::getName() + " data"     , "rom", config, "data")
	, sram(MSXDevice::getName() + " SRAM", 0x800, config)
{
	if (mainRom.size() != 32 * 1024) {
		throw MSXException("Main ROM must be exactly 32kB in size.");
	}
	if (fontRom.size() != 128 * 1024) {
		throw MSXException("Font ROM must be exactly 128kB in size.");
	}
	if (dataRom.size() != 32 * 1024) {
		throw MSXException("Data ROM must be exactly 32kB in size.");
	}

	reset(EmuTime::dummy());
}

void YamahaSKW01::reset(EmuTime::param time)
{
	fontAddress = {0, 0, 0, 0};
	dataAddress = 0;

	writeData(0, time);    // TODO check this, see MSXPrinterPort
	setStrobe(true, time); // TODO check this, see MSXPrinterPort
}

byte YamahaSKW01::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

byte YamahaSKW01::peekMem(word address, EmuTime::param time) const
{
	if (address == one_of(0x7FC0, 0x7FC2, 0x7FC4, 0x7FC6)) {
		return 0x01; // for now, always READY to read
	} else if (address == one_of(0x7FC1, 0x7FC3, 0x7FC5, 0x7FC7)) {
		unsigned group = (address - 0x7FC1) / 2;
		unsigned base = 0x8000 * group;
		unsigned offset = fontAddress[group] & 0x7FFF;
		return fontRom[base + offset];
	} else if (address == 0x7FC8 || address == 0x7FC9) {
		return 0xFF;
	} else if (address == 0x7FCA || address == 0x7FCB) {
		if ((dataAddress & (1 << 15)) == 0) {
			return dataRom[dataAddress & 0x7FFF];
		} else {
			return sram[dataAddress & 0x7FF];
		}
	} else if (address == 0x7FCC) {
		// bit 1 = status / other bits always 1
		return getPluggedPrintDev().getStatus(time)
		       ? 0xFF : 0xFD;
	} else if (address < 0x8000) {
		return mainRom[address];
	} else {
		return 0xFF;
	}
}

void YamahaSKW01::writeMem(word address, byte value, EmuTime::param time)
{
	if (0x7FC0 <= address && address <= 0x7FC7) {
		unsigned group = (address - 0x7FC0) / 2;
		if ((address & 1) == 0) {
			// LSB address
			fontAddress[group] = (fontAddress[group] & 0xFF00) | uint16_t(value << 0);
		} else {
			// MSB address
			fontAddress[group] = (fontAddress[group] & 0x00FF) | uint16_t(value << 8);
		}
	} else if (address == 0x7FC8) {
		dataAddress = (dataAddress & 0xFF00) | uint16_t(value << 0);
	} else if (address == 0x7FC9) {
		dataAddress = (dataAddress & 0x00FF) | uint16_t(value << 8);
	} else if (address == 0x7FCA || address == 0x7FCB) {
		if ((dataAddress & (1 << 15)) != 0) {
			sram.write(dataAddress & 0x7FF, value);
		}
	} else if (address == 0x7FCC) {
		setStrobe(value & 1, time);
	} else if (address == 0x7FCE) {
		writeData(value, time);
	}
}

const byte* YamahaSKW01::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FC0 & CacheLine::HIGH)) {
		// 0x7FC0-0x7FCF memory mapped registers
		return nullptr; // not cacheable
	} else if (start < 0x8000) {
		return &mainRom[start];
	} else {
		return unmappedRead.data();
	}
}

byte* YamahaSKW01::getWriteCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FC0 & CacheLine::HIGH)) {
		// 0x7FC0-0x7FCF memory mapped registers
		return nullptr; // not cacheable
	} else {
		return unmappedWrite.data();
	}
}

void YamahaSKW01::setStrobe(bool newStrobe, EmuTime::param time)
{
	if (newStrobe != strobe) {
		strobe = newStrobe;
		getPluggedPrintDev().setStrobe(strobe, time);
	}
}

void YamahaSKW01::writeData(uint8_t newData, EmuTime::param time)
{
	if (newData != data) {
		data = newData;
		getPluggedPrintDev().writeData(data, time);
	}
}

std::string_view YamahaSKW01::getDescription() const
{
	return "Yamaha SKW-01 printer port";
}

std::string_view YamahaSKW01::getClass() const
{
	return "Printer Port";
}

void YamahaSKW01::plug(Pluggable& dev, EmuTime::param time)
{
	Connector::plug(dev, time);
	getPluggedPrintDev().writeData(data, time);
	getPluggedPrintDev().setStrobe(strobe, time);
}

PrinterPortDevice& YamahaSKW01::getPluggedPrintDev() const
{
	return *checked_cast<PrinterPortDevice*>(&getPlugged());
}

template<typename Archive>
void YamahaSKW01::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("fontAddress", fontAddress,
	             "dataAddress", dataAddress,
	             "SRAM"       , sram,
	             "strobe"     , strobe,
	             "data"       , data);
	// TODO force writing data to port?? (See MSXPrinterPort)
}
INSTANTIATE_SERIALIZE_METHODS(YamahaSKW01);
REGISTER_MSXDEVICE(YamahaSKW01, "YamahaSKW01");

} // namespace openmsx
