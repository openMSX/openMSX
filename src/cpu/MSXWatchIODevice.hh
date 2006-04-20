// $Id$

#ifndef MSXWATCHIODEVICE_HH
#define MSXWATCHIODEVICE_HH

#include "MSXMultiDevice.hh"
#include "WatchPoint.hh"

namespace openmsx {

class MSXWatchIODevice : public MSXMultiDevice, public WatchPoint
{
public:
	MSXWatchIODevice(MSXMotherBoard& motherboard,
	                 WatchPoint::Type type, unsigned address,
	                 std::auto_ptr<TclObject> command,
	                 std::auto_ptr<TclObject> condition);
	virtual ~MSXWatchIODevice();

	MSXDevice*& getDevicePtr();

	// MSXDevice
	virtual std::string getName() const;
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

private:
	MSXDevice* device;
};

} // namespace openmsx

#endif
