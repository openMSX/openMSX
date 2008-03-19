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
	        const std::string& desc, const XMLElement& config,
	        const EmuTime& time);
	~VLM5030();
	void reset(const EmuTime& time);

	// latch control data
	void writeData(byte data);
	// get BSY pin level
	bool getBSY(const EmuTime& time);
	// set RST pin level : reset / set table address A8-A15
	void setRST(bool pin, const EmuTime& time);
	// set VCU pin level : ?? unknown
	void setVCU(bool pin, const EmuTime& time);
	// set ST pin level  : set table address A0-A7 / start speech
	void setST(bool pin, const EmuTime& time);

private:
	const std::auto_ptr<VLM5030Impl> pimple;
};

} // namespace openmsx

#endif
