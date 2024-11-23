#include "MSXFDC.hh"
#include "RealDrive.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "enumerate.hh"
#include "serialize.hh"
#include <array>
#include <memory>

namespace openmsx {

MSXFDC::MSXFDC(const DeviceConfig& config, const std::string& romId, bool needROM,
               DiskDrive::TrackMode trackMode)
	: MSXDevice(config)
	, rom(needROM
		? std::optional<Rom>(std::in_place, getName() + " ROM", "rom", config, romId)
		: std::nullopt) // e.g. Spectravideo_SVI-328 doesn't have a disk rom
{
	if (needROM && (rom->size() == 0)) {
		throw MSXException(
			"Empty ROM not allowed for \"", getName(), "\".");
	}
	bool singleSided = config.findChild("singlesided") != nullptr;
	int numDrives = config.getChildDataAsInt("drives", 1);
	if ((0 > numDrives) || (numDrives >= 4)) {
		throw MSXException("Invalid number of drives: ", numDrives);
	}
	unsigned timeout = config.getChildDataAsInt("motor_off_timeout_ms", 0);
	const auto* styleEl = config.findChild("connectionstyle");
	bool signalsNeedMotorOn = !styleEl || (styleEl->getData() == "Philips");
	EmuDuration motorTimeout = EmuDuration::msec(timeout);
	int i = 0;
	for (/**/; i < numDrives; ++i) {
		drives[i] = std::make_unique<RealDrive>(
			getMotherBoard(), motorTimeout, signalsNeedMotorOn,
			!singleSided, trackMode);
	}
	for (/**/; i < 4; ++i) {
		drives[i] = std::make_unique<DummyDrive>();
	}
}

void MSXFDC::powerDown(EmuTime::param time)
{
	for (const auto& drive : drives) {
		drive->setMotor(false, time);
	}
}

byte MSXFDC::readMem(word address, EmuTime::param /*time*/)
{
	return *MSXFDC::getReadCacheLine(address);
}

byte MSXFDC::peekMem(word address, EmuTime::param /*time*/) const
{
	return *MSXFDC::getReadCacheLine(address);
}

const byte* MSXFDC::getReadCacheLine(word start) const
{
	assert(rom);
	return &(*rom)[start & 0x3FFF];
}

void MSXFDC::getExtraDeviceInfo(TclObject& result) const
{
	if (rom) {
		rom->getInfo(result);
	}
}


template<typename Archive>
void MSXFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);

	// Drives are already constructed at this point, so we cannot use the
	// polymorphic object construction of the serialization framework.
	// Destroying and reconstructing the drives is not an option because
	// DriveMultiplexer already has pointers to the drives.
	std::array<char, 7> tag = {'d', 'r', 'i', 'v', 'e', 'X', 0};
	for (auto [i, drv] : enumerate(drives)) {
		if (auto* drive = dynamic_cast<RealDrive*>(drv.get())) {
			tag[5] = char('a' + i);
			ar.serialize(tag.data(), *drive);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXFDC);

} // namespace openmsx
