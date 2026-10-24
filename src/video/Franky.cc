#include "Franky.hh"

#include "MSXException.hh"
#include "VDP.hh"

#include "serialize.hh"

namespace openmsx {

namespace {

[[nodiscard]] SMSVDP::VideoStandard parseVideoStandard(std::string_view value)
{
	if (value == "NTSC") return SMSVDP::VideoStandard::NTSC;
	if (value == "PAL")  return SMSVDP::VideoStandard::PAL;
	throw MSXException("Invalid Franky video standard '", value,
	                   "', expected 'NTSC' or 'PAL'.");
}

[[nodiscard]] SMSVDPCore::Variant parseVdpType(std::string_view value)
{
	if (value == "VDP1") return SMSVDPCore::Variant::SMS1; // 315-5124
	if (value == "VDP2") return SMSVDPCore::Variant::SMS2; // 315-5246
	throw MSXException("Invalid Franky VDP type '", value,
	                   "', expected 'VDP1' or 'VDP2'.");
}

} // namespace

Franky::Franky(const DeviceConfig& config)
	: MSXDevice(config)
	, vdp(getMotherBoard(), getName(),
	      parseVideoStandard(config.getChildData("video_standard", "NTSC")),
	      parseVdpType(config.getChildData("vdp", "VDP2")))
	, sn76489(config, getName() + "_SN76489")
{
}

void Franky::init()
{
	MSXDevice::init();

	// The video auto switch needs the MSX VDP. Declared via
	// <device idref="VDP"/> in the extension; openMSX then guarantees the
	// VDP cannot be removed while this Franky exists.
	const auto& refs = getReferences();
	if (refs.size() != 1) {
		throw MSXException("Invalid Franky configuration: "
		                   "need reference to VDP device.");
	}
	auto* msxVdp = dynamic_cast<VDP*>(refs[0]);
	if (!msxVdp) {
		throw MSXException("Invalid Franky configuration: device '",
		                   refs[0]->getName(), "' is not a VDP device.");
	}
	vdp.setMsxVdp(*msxVdp);
}

void Franky::reset(EmuTime time)
{
	vdp.reset(time);
	sn76489.reset(time);
}

byte Franky::readIO(uint16_t port, EmuTime time)
{
	switch (port & 0xff) {
	case 0x48: return vdp.readVCounter(time);
	case 0x49: return vdp.readHCounter(time);
	case 0x88: return vdp.readData(time);
	case 0x89: return vdp.readControl(time);
	default:   return 0xff;
	}
}

byte Franky::peekIO(uint16_t port, EmuTime time) const
{
	// Side-effect-free variants of the readIO() values (no read-ahead
	// advance, no status clearing, no H-counter latch clearing).
	switch (port & 0xff) {
	case 0x48: return vdp.peekVCounter(time);
	case 0x49: return vdp.peekHCounter();
	case 0x88: return vdp.peekData();
	case 0x89: return vdp.peekControl(time);
	default:   return 0xff;
	}
}

void Franky::writeIO(uint16_t port, byte value, EmuTime time)
{
	switch (port & 0xff) {
	case 0x48:
	case 0x49:
		sn76489.write(value, time);
		break;
	case 0x88:
		vdp.writeData(value, time);
		break;
	case 0x89:
		vdp.writeControl(value, time);
		break;
	default:
		break;
	}
}

template<typename Archive>
void Franky::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("vdp", vdp);
	ar.serialize("sn76489", sn76489);
}
INSTANTIATE_SERIALIZE_METHODS(Franky);
REGISTER_MSXDEVICE(Franky, "Franky");

} // namespace openmsx
