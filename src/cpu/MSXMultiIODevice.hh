// $Id$

#ifndef __MSXMULTIIODEVICE_HH__
#define __MSXMULTIIODEVICE_HH__

#include <vector>
#include "MSXIODevice.hh"

using std::vector;

namespace openmsx {

class MSXMultiIODevice : public MSXIODevice
{
public:
	MSXMultiIODevice();
	virtual ~MSXMultiIODevice();

	void addDevice(MSXIODevice* device);
	void removeDevice(MSXIODevice* device);
	unsigned numDevices() const;
	
	// MSXDevice
	virtual void reset(const EmuTime& time);
	virtual void reInit(const EmuTime& time);
	virtual const string& getName() const;
	
	// MSXIODevice
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	void preCalcName();
	
	vector<MSXIODevice*> devices;
	string name;
};

} // namespace openmsx

#endif

