// $Id$

#include "MSXFDC.hh"
#include "Rom.hh"
#include "RealDrive.hh"
#include "XMLElement.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include "serialize.hh"

namespace openmsx {

MSXFDC::MSXFDC(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, rom(new Rom(motherBoard, getName() + " ROM", "rom", config))
{
	bool singleSided = config.findChild("singlesided") != NULL;
	int numDrives = config.getChildDataAsInt("drives", 1);
	if ((0 >= numDrives) || (numDrives >= 4)) {
		throw MSXException(StringOp::Builder() <<
			"Invalid number of drives: " << numDrives);
	}
	int i = 0;
	for ( ; i < numDrives; ++i) {
		drives[i].reset(new RealDrive(getMotherBoard(), !singleSided));
	}
	for ( ; i < 4; ++i) {
		drives[i].reset(new DummyDrive());
	}
}

MSXFDC::~MSXFDC()
{
}

void MSXFDC::powerDown(EmuTime::param time)
{
	for (int i = 0; i < 4; ++i) {
		drives[i]->setMotor(false, time);
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
	return &(*rom)[start & 0x3FFF];
}


template<typename Archive>
void MSXFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);

	// Drives are already constructed at this point, so we cannot use the
	// polymorphic object construction of the serialization framework.
	// Destroying and reconstructing the drives is not an option because
	// DriveMultiplexer already has pointers to the drives.
	for (int i = 0; i < 4; ++i) {
		if (RealDrive* drive = dynamic_cast<RealDrive*>(drives[i].get())) {
			std::string tag = std::string("drive") + char('a' + i);
			ar.serialize(tag.c_str(), *drive);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXFDC);

} // namespace openmsx
