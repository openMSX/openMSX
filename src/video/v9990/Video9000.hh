// $Id$

#ifndef VIDEO9000_HH
#define VIDEO9000_HH

#include "MSXDevice.hh"
#include "VideoSystemChangeListener.hh"
#include "VideoLayer.hh"
#include "EventListener.hh"

namespace openmsx {

class Display;

class Video9000 : public MSXDevice
                , private VideoSystemChangeListener
                , private VideoLayer
                , private EventListener
{
public:
	explicit Video9000(const DeviceConfig& config);
	virtual ~Video9000();

	// MSXDevice
	virtual void reset(EmuTime::param time);
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void recalc();

	// VideoSystemChangeListener
	virtual void preVideoSystemChange();
	virtual void postVideoSystemChange();

	// VideoLayer
	virtual void paint(OutputSurface& output);
	virtual string_ref getLayerName() const;
	virtual void takeRawScreenShot(unsigned height, const std::string& filename);

	// EventListener
	virtual int signalEvent(const shared_ptr<const Event>& event);

	Display& display;
	Layer* activeLayer;
	byte value;
};

} // namespace openmsx

#endif
