// $Id$

#include "MSXMusic.hh"
#include "YM2413.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

MSXMusic::MSXMusic(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, rom(new Rom(motherBoard, getName() + " ROM", "rom", config))
	, ym2413(new YM2413(motherBoard, getName(), config))
{
	reset(getCurrentTime());
}

MSXMusic::~MSXMusic()
{
}

void MSXMusic::reset(EmuTime::param time)
{
	ym2413->reset(time);
	registerLatch = 0; // TODO check
}


void MSXMusic::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x01) {
	case 0:
		writeRegisterPort(value, time);
		break;
	case 1:
		writeDataPort(value, time);
		break;
	}
}

void MSXMusic::writeRegisterPort(byte value, EmuTime::param /*time*/)
{
	registerLatch = value & 0x3F;
}

void MSXMusic::writeDataPort(byte value, EmuTime::param time)
{
	//PRT_DEBUG("YM2413: reg "<<(int)registerLatch<<" val "<<(int)value);
	ym2413->writeReg(registerLatch, value, time);
}

byte MSXMusic::readMem(word address, EmuTime::param /*time*/)
{
	return *MSXMusic::getReadCacheLine(address);
}

const byte* MSXMusic::getReadCacheLine(word start) const
{
	return &(*rom)[start & (rom->getSize() - 1)];
}

// version 1:  initial version
// version 2:  refactored YM2413 class structure
template<typename Archive>
void MSXMusic::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("ym2413", *ym2413);
	} else {
		// In older versions, the 'ym2413' level was missing, delegate
		// directly to YM2413 without emitting the 'ym2413' tag.
		ym2413->serialize(ar, version);
	}

	ar.serialize("registerLatch", registerLatch);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMusic);
REGISTER_MSXDEVICE(MSXMusic, "MSX-Music");

} // namespace openmsx
