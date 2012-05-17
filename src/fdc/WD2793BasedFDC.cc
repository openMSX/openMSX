// $Id$

#include "WD2793BasedFDC.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

namespace openmsx {

WD2793BasedFDC::WD2793BasedFDC(MSXMotherBoard& motherBoard,
                               const DeviceConfig& config)
	: MSXFDC(motherBoard, config)
	, multiplexer(new DriveMultiplexer(reinterpret_cast<DiskDrive**>(drives)))
	, controller(new WD2793(motherBoard.getScheduler(), *multiplexer,
	                        motherBoard.getMSXCliComm(), getCurrentTime()))
{
}

WD2793BasedFDC::~WD2793BasedFDC()
{
}

void WD2793BasedFDC::reset(EmuTime::param time)
{
	controller->reset(time);
}

template<typename Archive>
void WD2793BasedFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXFDC>(*this);
	ar.serialize("multiplexer", *multiplexer);
	ar.serialize("wd2793", *controller);
}
INSTANTIATE_SERIALIZE_METHODS(WD2793BasedFDC);

} // namespace openmsx
