#ifndef MSXWATCHIODEVICE_HH
#define MSXWATCHIODEVICE_HH

#include "MSXMultiDevice.hh"
#include "WatchPoint.hh"
#include <memory>

namespace openmsx {

class MSXWatchIODevice;

class WatchIO final : public WatchPoint
                    , public std::enable_shared_from_this<WatchIO>
{
public:
	WatchIO(MSXMotherBoard& motherboard,
	        WatchPoint::Type type,
	        unsigned beginAddr, unsigned endAddr,
	        TclObject command, TclObject condition,
	        unsigned newId = -1);
	~WatchIO();

	MSXWatchIODevice& getDevice(byte port);

private:
	void doReadCallback(unsigned port);
	void doWriteCallback(unsigned port, unsigned value);

	MSXMotherBoard& motherboard;
	std::vector<std::unique_ptr<MSXWatchIODevice>> ios;

	friend class MSXWatchIODevice;
};

class MSXWatchIODevice final : public MSXMultiDevice
{
public:
	MSXWatchIODevice(const HardwareConfig& hwConf, WatchIO& watchIO);

	MSXDevice*& getDevicePtr() { return device; }

private:
	// MSXDevice
	std::string getName() const override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	WatchIO& watchIO;
	MSXDevice* device;
};

} // namespace openmsx

#endif
