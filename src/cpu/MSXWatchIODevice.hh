// $Id$

#ifndef MSXWATCHIODEVICE_HH
#define MSXWATCHIODEVICE_HH

#include "MSXMultiDevice.hh"
#include "WatchPoint.hh"

namespace openmsx {

class MSXCPUInterface;
class MSXWatchIODevice;

class WatchIO : public WatchPoint
{
public:
	WatchIO(MSXMotherBoard& motherboard,
	        WatchPoint::Type type,
	        unsigned beginAddr, unsigned endAddr,
	        std::auto_ptr<TclObject> command,
	        std::auto_ptr<TclObject> condition);
	virtual ~WatchIO();

	MSXWatchIODevice& getDevice(byte port);

private:
	void doReadCallback(unsigned port);
	void doWriteCallback(unsigned port, unsigned value);

	MSXCPUInterface& cpuInterface;
	typedef std::vector<MSXWatchIODevice*> IOs;
	IOs ios;

	friend class MSXWatchIODevice;
};

class MSXWatchIODevice : public MSXMultiDevice
{
public:
	MSXWatchIODevice(MSXMotherBoard& motherboard,
	                     WatchIO& watchIO);

	MSXDevice*& getDevicePtr();

private:
	// MSXDevice
	virtual std::string getName() const;
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	WatchIO& watchIO;
	MSXDevice* device;
};

} // namespace openmsx

#endif
