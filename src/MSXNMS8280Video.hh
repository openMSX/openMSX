#ifndef MSXNMS8280VIDEO_HH
#define MSXNMS8280VIDEO_HH

#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "MSXDevice.hh"
#include "Observer.hh"
#include "SimpleDebuggable.hh"

namespace openmsx {

/** Video/audio control ports of the Philips NMS 8280 (I/O ports 0xF6..0xF8).
  *
  * Based on the NMS 8280 service manual (block diagram) and on the MSX2 sub-ROM
  * of that machine:
  *  - 0xF6 ("mode select 1"): bit 5 = 0 selects the 50% video mix (the
  *    DIZ/YMIX/B-W outputs go to the analog unit).
  *  - 0xF7 ("mode select 2"), written by SET VIDEO (measured with every
  *    parameter on its own):
  *      bits 7:6  inverse of S1:S0 of VDP R#9 (parameter 1, the mode); they
  *                drive the mode indicators
  *      bit 5     Ym (parameter 2): the external video at half intensity
  *      bit 4     AV control (parameter 7): 1 = RGB-SCART output
  *      bit 3     sync (parameter 4): 0 = internal, 1 = external
  *      bit 2     video input (parameter 6): 0 = RGB-SCART, 1 = other
  *      bits 1:0  inverse of the low two bits of the audio parameter (5): the
  *                external audio is mixed in the right (bit 0) and left (bit
  *                1) channel when the bit is 0
  *    The bits of the other parameter that SET VIDEO writes to the VDP are CB
  *    (parameter 3: R#8 bit 4, the color bus). The sub-ROM reads this port
  *    back at boot to detect the hardware.
  *  - 0xF8: audio mix control (written by the sub-ROM, shadow in 0xFFF8).
  * The 50% video mix, the SET VIDEO bits and the routing of the external audio
  * (see the External Audio device) have an effect on the emulation; the other
  * bits are just stored and can be read back.
  *
  * The device also holds the controls of the front panel that have no port:
  *  - external_video_level: the digitization level slider (25), the
  *    brightness of the external video (50 = normal, 100 = twice as bright)
  *  - video_mix_level: the video mix slider (27), the intensity of the image of
  *    the computer when it is superimposed (100 = GRAPHIC, 0 = only the
  *    external video)
  *  - av_source_euroconnector: the audio/video source selector (21), which
  *    only lights the indicators (there is one external source)
  */
class MSXNMS8280Video final : public MSXDevice, private Observer<Setting>
{
public:
	explicit MSXNMS8280Video(const DeviceConfig& config);
	~MSXNMS8280Video() override;

	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	/** The indicators of the front panel. */
	struct Leds {
		bool avInOut;    // (22) external source via the Euroconnector
		bool avIn;       // (23) external source via the separate AV IN jacks
		bool superimpose; // (24) external sync: superimposing (or the other external modes)
	};
	[[nodiscard]] Leds getLeds() const;

	// The sliders and the switch of the front panel (settings), for the GUI.
	[[nodiscard]] IntegerSetting& getVideoLevelSetting() { return videoLevelSetting; }
	[[nodiscard]] IntegerSetting& getVideoMixLevelSetting() { return videoMixLevelSetting; }
	[[nodiscard]] BooleanSetting& getEuroSourceSetting() { return euroSourceSetting; }

private:
	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	void applyVideoMix() const;
	void applyAudioRouting(EmuTime time) const;

private:
	/** The latches of the three ports (0 = 0xF6, 1 = 0xF7, 2 = 0xF8), for the
	  * debugger. Handy to see what SET VIDEO selected, see the table above.
	  */
	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard_, const std::string& name_);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value) override;
	} debuggable;

private:
	IntegerSetting videoLevelSetting;    // (25) digitization level
	IntegerSetting videoMixLevelSetting; // (27) video mix slider
	BooleanSetting euroSourceSetting;    // (21) audio/video source selector

	byte portF6 = 0xFF;
	byte portF7 = 0xFF;
	byte portF8 = 0x00;
};

} // namespace openmsx

#endif
