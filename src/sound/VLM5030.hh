// $Id$

#ifndef VLM5030_HH
#define VLM5030_HH

#include "EmuTime.hh"
#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class DeviceConfig;

class VLM5030
{
public:
	VLM5030(MSXMotherBoard& motherBoard, const std::string& name,
	        const std::string& desc, const std::string& romFilename,
	        const DeviceConfig& config);
	~VLM5030();
	void reset();

	/** latch control data */
	void writeData(byte data);

	/** set RST / VCU / ST pins */
	void writeControl(byte data, EmuTime::param time);

	/** get BSY pin level */
	bool getBSY(EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class Impl;
	const std::auto_ptr<Impl> pimpl;
};

} // namespace openmsx

#endif
