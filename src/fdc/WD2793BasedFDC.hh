// $Id$

#ifndef WD2793BASEDFDC_HH
#define WD2793BASEDFDC_HH

#include "MSXFDC.hh"
#include <memory>

namespace openmsx {

class DriveMultiplexer;
class WD2793;

class WD2793BasedFDC : public MSXFDC
{
public:
	virtual void reset(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	explicit WD2793BasedFDC(const DeviceConfig& config);
	virtual ~WD2793BasedFDC();

	const std::unique_ptr<DriveMultiplexer> multiplexer;
	const std::unique_ptr<WD2793> controller;
};

REGISTER_BASE_NAME_HELPER(WD2793BasedFDC, "WD2793BasedFDC");

} // namespace openmsx

#endif
