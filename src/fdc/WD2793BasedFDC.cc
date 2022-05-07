#include "WD2793BasedFDC.hh"
#include "XMLElement.hh"
#include "serialize.hh"

namespace openmsx {

WD2793BasedFDC::WD2793BasedFDC(const DeviceConfig& config, const std::string& romId,
                               bool needROM, DiskDrive::TrackMode trackMode)
	: MSXFDC(config, romId, needROM, trackMode)
	, multiplexer(reinterpret_cast<DiskDrive**>(drives))
	, controller(
		getScheduler(), multiplexer, getCliComm(), getCurrentTime(),
		config.getXML()->getName() == "WD1770")
{
}

void WD2793BasedFDC::reset(EmuTime::param time)
{
	controller.reset(time);
}

void WD2793BasedFDC::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXFDC>(*this);
	ar.serialize("multiplexer", multiplexer,
	             "wd2793",      controller);
}
INSTANTIATE_SERIALIZE_METHODS(WD2793BasedFDC);

} // namespace openmsx
