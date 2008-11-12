// $Id$

#include "MSXMusic.hh"
#include "YM2413.hh"
#include "YM2413_2.hh"
#include "Rom.hh"
#include "XMLElement.hh"
#include "serialize.hh"

namespace openmsx {

static YM2413Interface* createYM2413(
	MSXMotherBoard& motherBoard, const XMLElement& config,
	const std::string& name, EmuTime::param time)
{
	if (config.getChildDataAsBool("alternative", false)) {
		return new YM2413_2(motherBoard, name, config, time);
	} else {
		return new YM2413  (motherBoard, name, config, time);
	}
}

MSXMusic::MSXMusic(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, rom(new Rom(motherBoard, getName() + " ROM", "rom", config))
	, ym2413(createYM2413(motherBoard, config, getName(), getCurrentTime()))
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
	return *getReadCacheLine(address);
}

const byte* MSXMusic::getReadCacheLine(word start) const
{
	return &(*rom)[start & (rom->getSize() - 1)];
}


template<typename Archive>
void MSXMusic::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serializePolymorphic("ym2413", *ym2413);
	ar.serialize("registerLatch", registerLatch);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMusic);
REGISTER_MSXDEVICE(MSXMusic, "MSX-Music");

} // namespace openmsx
