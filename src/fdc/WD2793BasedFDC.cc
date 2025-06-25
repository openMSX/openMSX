#include "WD2793BasedFDC.hh"
#include "XMLElement.hh"
#include "serialize.hh"

namespace openmsx {

WD2793BasedFDC::WD2793BasedFDC(DeviceConfig& config, const std::string& romId,
                               bool needROM, DiskDrive::TrackMode trackMode)
	: MSXFDC(config, romId, needROM, trackMode)
	, multiplexer(drives)
	, controller(getScheduler(), multiplexer, getCliComm(), getCurrentTime(),
	             config.getXML()->getName() == "WD1770")
{
}

void WD2793BasedFDC::reset(EmuTime time)
{
	controller.reset(time);
}

template<typename Archive>
void WD2793BasedFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXFDC>(*this);
	ar.serialize("multiplexer", multiplexer,
	             "wd2793",      controller);
}
INSTANTIATE_SERIALIZE_METHODS(WD2793BasedFDC);

} // namespace openmsx
