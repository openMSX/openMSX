#ifndef EXTERNALAUDIO_HH
#define EXTERNALAUDIO_HH

#include "MSXDevice.hh"

#include "EmuTime.hh"

#include <memory>

namespace openmsx {

class ExternalAudioSound;

/** Audio of the external video source (webcam microphone, capture card, ...).
  *
  * The external video bridge script writes the audio of the source to a ring
  * buffer file (see share/scripts/external_video_bridge.py for the layout);
  * this device plays it through the mixer of the emulated machine. It does
  * nothing until the setting 'external_audio_file' names that file.
  *
  * Settings:
  *  - external_audio_file: the ring buffer file written by the bridge.
  *  - external_audio_delay: delay in milliseconds. The audio is played this
  *    long after it was captured; it absorbs the difference in timing between
  *    the capture clock and the emulation, and it can be used to line up the
  *    sound with the (also delayed) video.
  *  - external_audio_mix: the audio mix slider (0 = computer only, 50 = both,
  *    100 = external audio only).
  *  - external_audio_follow_set_video: obey the audio routing of the BASIC
  *    command SET VIDEO (port 0xF7 of the NMS 8280), which after a reset
  *    mixes in no external audio at all. Off by default.
  *  - external-audio_volume and external-audio_balance: the usual settings of
  *    a sound device.
  */
class MSXExternalAudio final : public MSXDevice
{
public:
	explicit MSXExternalAudio(const DeviceConfig& config);
	~MSXExternalAudio() override;

	/** Which channels of the external audio are mixed in. Set from the audio
	  * routing bits of the video control port of the NMS 8280 (0xF7). It only
	  * has an effect if the setting 'external_audio_follow_set_video' is on.
	  */
	void setRouting(bool left, bool right, EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::unique_ptr<ExternalAudioSound> sound;
};

} // namespace openmsx

#endif
