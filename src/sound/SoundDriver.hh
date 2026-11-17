#ifndef SOUNDDRIVER_HH
#define SOUNDDRIVER_HH

#include "Mixer.hh"
#include <span>

namespace openmsx {

class SoundDriver
{
public:
	virtual ~SoundDriver() = default;

	/** Mute the sound system
	 */
	virtual void mute() = 0;

	/** Unmute the sound system
	 */
	virtual void unmute() = 0;

	/** Returns the actual sample frequency: the one of the 'frequency'
	  * setting, or with 0 (auto) the one of the sound device.
	  */
	[[nodiscard]] virtual unsigned getFrequency() const = 0;

	/** Get the number of samples that should be created 'per fragment'.
	  * This is not the same value as the 'samples setting'.
	  */
	[[nodiscard]] virtual unsigned getSamples() const = 0;

	virtual void uploadBuffer(std::span<const StereoFloat> buffer) = 0;

protected:
	SoundDriver() = default;
};

} // namespace openmsx

#endif
