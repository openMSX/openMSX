#include "MSXS1985.hh"
#include "DeviceConfig.hh"
#include "HardwareConfig.hh"
#include "SRAM.hh"
#include "XMLElement.hh"
#include "enumerate.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

constexpr byte ID = 0xFE;

[[nodiscard]] static bool machineHasMemoryMapper(const DeviceConfig& config)
{
	for (auto& psElem : config.getHardwareConfig().getDevicesElem().getChildren("primary")) {
		if (psElem->findChild("MemoryMapper")) return true;
		for (auto& ssElem : psElem->getChildren("secondary")) {
			if (ssElem->findChild("MemoryMapper")) return true;
		}
	}
	return false;
}

MSXS1985::MSXS1985(const DeviceConfig& config)
	: MSXDevice(config)
	, MSXSwitchedDevice(getMotherBoard(), ID)
{
	if (!machineHasMemoryMapper(config)) {
		dummyMapper.emplace(config);
	}

	if (!config.findChild("sramname")) {
		// special case for backwards compatibility (S1985 didn't
		// always have SRAM in its config...)
		sram = std::make_unique<SRAM>(
			getName() + " SRAM", "S1985 Backup RAM",
			0x10, config, SRAM::DontLoadTag{});
	} else {
		sram = std::make_unique<SRAM>(
			getName() + " SRAM", "S1985 Backup RAM",
			0x10, config);
	}
	reset(EmuTime::dummy());
}

MSXS1985::~MSXS1985() = default;

void MSXS1985::reset(EmuTime::param /*time*/)
{
	color1 = color2 = pattern = address = 0; // TODO check this
}

byte MSXS1985::readSwitchedIO(word port, EmuTime::param time)
{
	byte result = peekSwitchedIO(port, time);
	switch (port & 0x0F) {
	case 7:
		pattern = (pattern << 1) | (pattern >> 7);
		break;
	}
	return result;
}

byte MSXS1985::peekSwitchedIO(word port, EmuTime::param /*time*/) const
{
	switch (port & 0x0F) {
	case 0:
		return byte(~ID);
	case 2:
		return (*sram)[address];
	case 7:
		return (pattern & 0x80) ? color2 : color1;
	default:
		return 0xFF;
	}
}

void MSXS1985::writeSwitchedIO(word port, byte value, EmuTime::param /*time*/)
{
	switch (port & 0x0F) {
	case 1:
		address = value & 0x0F;
		break;
	case 2:
		sram->write(address, value);
		break;
	case 6:
		color2 = color1;
		color1 = value;
		break;
	case 7:
		pattern = value;
		break;
	}
}

// version 1: initial version
// version 2: replaced RAM with SRAM
// version 3: added dummyMapper
template<typename Archive>
void MSXS1985::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	// no need to serialize MSXSwitchedDevice base class

	if (ar.versionAtLeast(version, 2)) {
		// serialize normally...
		ar.serialize("sram", *sram);
	} else {
		assert(ar.isLoader());
		// version 1 had here
		//    <ram>
		//      <ram encoding="..">...</ram>
		//    </ram>
		// deserialize that structure and transfer it to SRAM
		byte tmp[0x10];
		ar.beginTag("ram");
		ar.serialize_blob("ram", tmp, sizeof(tmp));
		ar.endTag("ram");
		for (auto [i, t] : enumerate(tmp)) {
			sram->write(unsigned(i), t);
		}
	}

	ar.serialize("address", address,
	             "color1",  color1,
	             "color2",  color2,
	             "pattern", pattern);

	if (ar.versionAtLeast(version, 3)) {
		if (dummyMapper) {
			ar.serialize("dummyMapper", *dummyMapper);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXS1985);
REGISTER_MSXDEVICE(MSXS1985, "S1985");


[[nodiscard]] static DeviceConfig getDummyConfig(const DeviceConfig& config)
{
	static const XMLElement xml = [&] {
		XMLElement result("DummyMemoryMapper");
		result.addAttribute("id", "DummyMemoryMapper");
		result.addChild("size", "dummy");
		return result;
	}();
	return DeviceConfig(config, xml);
}

DummyMemoryMapper::DummyMemoryMapper(const DeviceConfig& config)
	: MSXMemoryMapperBase(getDummyConfig(config))
{
}

byte DummyMemoryMapper::peekIO(word port, EmuTime::param /*time*/) const
{
	return registers[port & 3];
}

void DummyMemoryMapper::writeIO(word port, byte value, EmuTime::param /*time*/)
{
	registers[port & 3] = value;
}

template<typename Archive>
void DummyMemoryMapper::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("registers", registers);
}

} // namespace openmsx
