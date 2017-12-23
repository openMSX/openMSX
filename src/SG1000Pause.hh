#ifndef SG1000PAUSE_HH
#define SG1000PAUSE_HH

#include "MSXDevice.hh"
#include "MSXEventListener.hh"
#include "Schedulable.hh"

namespace openmsx {

/** This button is labeled "hold" on SG-1000, "pause" on SG-1000 mk 2 and
  * "reset" on SC-3000. It triggers a non-maskable interrupt when it is
  * pressed.
  */
class SG1000Pause final : public MSXDevice,
                          private MSXEventListener, private Schedulable
{
public:
	explicit SG1000Pause(const DeviceConfig& config);
	~SG1000Pause();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// MSXEventListener
	void signalEvent(const std::shared_ptr<const Event>& event,
	                 EmuTime::param time) override;

	// Schedulable
	void executeUntil(EmuTime::param time) override;
};

} // namespace openmsx

#endif
