// $Id$

#ifndef __MSXMULTIIODEVICE_HH__
#define __MSXMULTIIODEVICE_HH__

#include <vector>
#include "MSXDevice.hh"

using std::vector;

namespace openmsx {

class MSXMultiIODevice : public MSXDevice
{
public:
	typedef vector<MSXDevice*> Devices;

	MSXMultiIODevice();
	virtual ~MSXMultiIODevice();

	void addDevice(MSXDevice* device);
	void removeDevice(MSXDevice* device);
	Devices& getDevices();
	
	// MSXDevice
	virtual void reset(const EmuTime& time);
	virtual void reInit(const EmuTime& time);
	virtual const string& getName() const;
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	static const XMLElement& getMultiConfig();
	void preCalcName();
	
	Devices devices;
	string name;
};

} // namespace openmsx

#endif

