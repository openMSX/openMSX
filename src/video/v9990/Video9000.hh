#ifndef VIDEO9000_HH
#define VIDEO9000_HH

#include "VideoLayer.hh"
#include "VideoSystemChangeListener.hh"

#include "EventListener.hh"
#include "MSXDevice.hh"

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
	void reset(EmuTime time) override;
	void writeIO(uint16_t port, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void recalculate();
	void recalculateVideoSource();

	// VideoSystemChangeListener
	void preVideoSystemChange() noexcept override;
	void postVideoSystemChange() noexcept override;

	// VideoLayer
	void paint(OutputSurface& output) override;
	void takeRawScreenShot(std::optional<unsigned> height, const std::string& filename) override;

	// EventListener
	bool signalEvent(const Event& event) override;

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	VideoSourceSetting& videoSourceSetting;
	VDP* vdp;
	V9990* v9990;
	Layer* activeLayer = nullptr;
	PostProcessor* v99x8Layer = nullptr;
	PostProcessor* v9990Layer = nullptr;
	uint8_t value = 0x10;
};

} // namespace openmsx

#endif
