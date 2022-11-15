#ifndef VIDEO9000_HH
#define VIDEO9000_HH

#include "MSXDevice.hh"
#include "VideoSystemChangeListener.hh"
#include "VideoLayer.hh"
#include "EventListener.hh"

namespace openmsx {

class VDP;
class V9990;
class PostProcessor;

class Video9000 final : public MSXDevice
                      , private VideoSystemChangeListener
                      , private VideoLayer
                      , private EventListener
{
public:
	explicit Video9000(const DeviceConfig& config);
	~Video9000() override;

	// MSXDevice
	void init() override;
	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void recalc();
	void recalcVideoSource();

	// VideoSystemChangeListener
	void preVideoSystemChange() noexcept override;
	void postVideoSystemChange() noexcept override;

	// VideoLayer
	void paint(OutputSurface& output) override;
	void takeRawScreenShot(unsigned height, const std::string& filename) override;

	// EventListener
	int signalEvent(const Event& event) override;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	VideoSourceSetting& videoSourceSetting;
	VDP* vdp;
	V9990* v9990;
	Layer* activeLayer = nullptr;
	PostProcessor* v99x8Layer = nullptr;
	PostProcessor* v9990Layer = nullptr;
	byte value = 0x10;
};

} // namespace openmsx

#endif
