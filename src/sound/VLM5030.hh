// $Id$

#ifndef VLM5030_HH
#define VLM5030_HH

#include "openmsx.hh"
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class EmuTime;
class VLM5030Impl;

class VLM5030
{
public:
	VLM5030(MSXMotherBoard& motherBoard, const std::string& name,
	        const std::string& desc, const XMLElement& config);
	~VLM5030();
	void reset();

	/** latch control data */
	void writeData(byte data);

	/** set RST / VCU / ST pins */
	void writeControl(byte data, const EmuTime& time);

	/** get BSY pin level */
	bool getBSY(const EmuTime& time);

private:
	const std::auto_ptr<VLM5030Impl> pimple;
};

} // namespace openmsx

#endif
