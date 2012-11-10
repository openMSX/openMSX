// $Id$

#ifndef MSXWATCHIODEVICE_HH
#define MSXWATCHIODEVICE_HH

#include "MSXMultiDevice.hh"
#include "WatchPoint.hh"
#include <memory>

namespace openmsx {

class MSXCPUInterface;
class MSXWatchIODevice;

class WatchIO : public WatchPoint
{
public:
	WatchIO(MSXMotherBoard& motherboard,
	        WatchPoint::Type type,
	        unsigned beginAddr, unsigned endAddr,
	        TclObject command, TclObject condition,
	        unsigned newId = -1);
	virtual ~WatchIO();

	MSXWatchIODevice& getDevice(byte port);

private:
	void doReadCallback(unsigned port);
	void doWriteCallback(unsigned port, unsigned value);

	MSXCPUInterface& cpuInterface;
	std::vector<std::unique_ptr<MSXWatchIODevice>> ios;

	friend class MSXWatchIODevice;
};

class MSXWatchIODevice : public MSXMultiDevice
{
public:
	MSXWatchIODevice(const HardwareConfig& hwConf, WatchIO& watchIO);

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
