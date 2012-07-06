// $Id$

#ifndef VIDEO9000_HH
#define VIDEO9000_HH

#include "MSXDevice.hh"
#include "VideoSystemChangeListener.hh"
#include "VideoLayer.hh"
#include "EventListener.hh"

namespace openmsx {

class Display;
class V9990;
class PostProcessor;

class Video9000 : public MSXDevice
                , private VideoSystemChangeListener
                , private VideoLayer
                , private EventListener
{
public:
	explicit Video9000(const DeviceConfig& config);
	virtual ~Video9000();

	// MSXDevice
	virtual void init();
	virtual void reset(EmuTime::param time);
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void recalc();
	void recalcVideoSource();

	// VideoSystemChangeListener
	virtual void preVideoSystemChange();
	virtual void postVideoSystemChange();

	// VideoLayer
	virtual void paint(OutputSurface& output);
	virtual string_ref getLayerName() const;
	virtual void takeRawScreenShot(unsigned height, const std::string& filename);

	// EventListener
	virtual int signalEvent(const shared_ptr<const Event>& event);

	// Observer<Setting>
	void update(const Setting& setting);

	Display& display;
	V9990* v9990;
	Layer* activeLayer;
	PostProcessor* v99x8Layer;
	PostProcessor* v9990Layer;
	byte value;
};

} // namespace openmsx

#endif
